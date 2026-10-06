"""Find functions that codegen missed because they are only reached indirectly.

Small leaf functions (vtable entries, callbacks) often have no .pdata entry and
are never the target of a direct `bl`, so analysis folds them into the
preceding function. At runtime the game calls them through a pointer and the
runtime aborts with "Call to invalid or unregistered function".

This scans the data sections of a dumped guest image for pointers into .text
that are not known function starts, and keeps the ones that look like
function-pointer tables (their neighbours point at real function starts)
rather than switch tables (neighbours land inside one function).

Usage:
  oratan.exe --dump_image=work/image.bin ...   (writes the loaded image)
  python tools/find_missing_funcs.py work/image.bin [--toml]
"""

import argparse
import bisect
import re
import struct
import sys
from pathlib import Path

IMAGE_BASE = 0x82000000
TEXT_START = 0x82110000
TEXT_END = 0x8288B3B4  # import thunks start here
DATA_SECTIONS = [
    (0x82000400, 0xF9F64),   # .rdata
    (0x82890000, 0x1171DFC),  # .data
]

BLR = 0x4E800020
BCTR = 0x4E800420
MR_R8_R8 = 0x7D084378  # marker MSVC emits at SEH handler/resume blocks


def load_known_functions(init_cpp: Path) -> list[int]:
    text = init_cpp.read_text()
    return sorted({int(m, 16) for m in re.findall(r"\{ 0x([0-9A-F]{8}), sub_", text)})


def word(img: bytes, addr: int) -> int:
    off = addr - IMAGE_BASE
    return struct.unpack_from(">I", img, off)[0]


def ends_block(insn: int) -> bool:
    """True if `insn` never falls through (blr, bctr, unconditional b)."""
    if insn in (BLR, BCTR):
        return True
    opcode = insn >> 26
    return opcode == 18 and (insn & 1) == 0  # b / ba, not bl


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--init", default="generated/default/oratan_init.cpp")
    ap.add_argument("--toml", action="store_true", help="print [functions] entries")
    args = ap.parse_args()

    img = Path(args.image).read_bytes()
    funcs = load_known_functions(Path(args.init))
    known = set(funcs)

    def is_code_ptr(v: int) -> bool:
        return TEXT_START <= v < TEXT_END and v % 4 == 0

    def containing(v: int) -> int:
        i = bisect.bisect_right(funcs, v) - 1
        return funcs[i] if i >= 0 else 0

    found: dict[int, list[int]] = {}
    for start, size in DATA_SECTIONS:
        for a in range(start, start + size - 3, 4):
            v = word(img, a)
            if not is_code_ptr(v) or v in known:
                continue
            # The instruction before must end a block, otherwise this is a label
            # in the middle of straight-line code.
            if not ends_block(word(img, v - 4)) and word(img, v - 4) != 0:
                continue
            if word(img, v) == MR_R8_R8:
                continue
            # SEH scope table entry {try_begin, try_end, filter, target}: the
            # target is a resume label inside the guarded function.
            b0, b1 = word(img, a - 12), word(img, a - 8)
            if is_code_ptr(b0) and is_code_ptr(b1) and b0 <= v and containing(b0) == containing(v):
                continue
            # Look at the neighbouring table slots.
            neigh = [word(img, a + d) for d in (-8, -4, 4, 8)]
            neigh = [n for n in neigh if is_code_ptr(n)]
            same_func = sum(1 for n in neigh if containing(n) == containing(v))
            fn_starts = sum(1 for n in neigh if n in known)
            if neigh and same_func > fn_starts:
                continue  # looks like a switch table inside one function
            found.setdefault(v, []).append(a)

    # Addresses built in code with `lis rX,hi` + `addi rY,rX,lo` (callbacks,
    # thread entry points passed to the kernel).
    for a in range(TEXT_START, TEXT_END, 4):
        w = word(img, a)
        if w >> 26 != 15 or (w >> 16) & 31 != 0:  # lis = addis rD,0,imm
            continue
        rd = (w >> 21) & 31
        hi = w & 0xFFFF
        for k in range(1, 12):
            w2 = word(img, a + 4 * k)
            if w2 >> 26 == 14 and (w2 >> 16) & 31 == rd:  # addi rY,rD,lo
                lo = w2 & 0xFFFF
                lo = lo - 0x10000 if lo & 0x8000 else lo
                v = ((hi << 16) + lo) & 0xFFFFFFFF
                if not is_code_ptr(v) or v in known:
                    break
                # `lis/addi; add; mtctr; bctr` computes a relative switch table
                # base, which is a case label, not a function.
                ry = (w2 >> 21) & 31
                tail = [word(img, a + 4 * (k + j)) for j in range(1, 8)]
                if any(t >> 26 == 31 and (t >> 1) & 0x3FF == 467 and (t >> 11) & 0x3FF == 0x120
                       for t in tail):  # mtspr CTR
                    break
                prev = word(img, v - 4)
                # An inline switch table right after `bctr` starts with a code
                # pointer, not an instruction.
                first = word(img, v)
                if (ends_block(prev) or prev == 0) and first != MR_R8_R8 and not is_code_ptr(first):
                    found.setdefault(v, []).append(a)
                break
            if (w2 >> 21) & 31 == rd and w2 >> 26 not in (36, 37, 38, 44, 45, 47, 52, 54):
                break  # rD overwritten by something other than a store

    for v in sorted(found):
        refs = ", ".join(f"{r:#010x}" for r in found[v][:3])
        if args.toml:
            print(f"0x{v:08X} = {{ name = \"sub_{v:08X}\" }}  # ptr at {refs}")
        else:
            print(f"{v:#010x} in sub_{containing(v):08X}  refs: {refs}")
    print(f"# {len(found)} candidates", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
