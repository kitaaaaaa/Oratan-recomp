"""Disassemble a range of a dumped guest image (see --dump_image).

Usage:
  python tools/disasm.py work/image.bin 0x824E7F40 [count]
  python tools/disasm.py work/image.bin 0x820F9CD0 16 --words   (hex dump)
"""

import argparse
import struct
import sys
from pathlib import Path

import capstone

IMAGE_BASE = 0x82000000


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("addr", type=lambda s: int(s, 0))
    ap.add_argument("count", type=int, nargs="?", default=24)
    ap.add_argument("--words", action="store_true", help="dump as big-endian words")
    args = ap.parse_args()

    img = Path(args.image).read_bytes()
    off = args.addr - IMAGE_BASE
    data = img[off : off + args.count * 4]

    if args.words:
        for i in range(0, len(data), 4):
            print(f"{args.addr + i:08X}: {struct.unpack_from('>I', data, i)[0]:08X}")
        return 0

    md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
    for i in range(0, len(data), 4):
        w = data[i : i + 4]
        ins = next(md.disasm(w, args.addr + i), None)
        text = f"{ins.mnemonic} {ins.op_str}" if ins else "<unknown>"
        print(f"{args.addr + i:08X}: {w.hex().upper()}  {text}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
