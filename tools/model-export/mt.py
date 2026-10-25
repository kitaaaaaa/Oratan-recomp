"""FEI.mt reader: blocks, channel entries and the cubic-Hermite curve evaluator, as the game's code does it
(sub_82240808 selects the block, sub_8223F478 fetches a channel, sub_8223F270 evaluates it).

File layout (little endian):
  0x10 ..          table A: u32 offsets of blocks (animation index -> block header)
  0x9e4 ..         table B: more block offsets
  block header (28 bytes): u32 n (length in ticks), u32 S, u32 A (channel-entry array), u32 B (constant array),
                           u32 n2, u32 C, u32 D
  channel entry (u16, at address a):  0 -> 0.0 ; < 0x2000 -> curve with keys at (a - 2*entry) ; >= 0x2000 -> constant
                           (constant / key values are bfloat16 with the sign in bit 0: bits = (v >> 1) | ((v & 1) << 15))
  key record (8 bytes):  u16 time (ticks, /60 = seconds), u16 value, u16 tangent_in, u16 tangent_out
  curve: Hermite between rec0 (value +2, out tangent +6) and rec1 (value +10, in tangent +12); tangents are scaled by
         the key spacing in seconds.
"""
import struct, math, os

# FEI.mt as extracted by farc.py (set FEI_MT to read another robot's .mt)
DATA = open(os.environ.get('FEI_MT', os.path.join('farc_data', 'FEI.mt')), 'rb').read()
u16 = lambda o: struct.unpack_from('<H', DATA, o)[0]
u32 = lambda o: struct.unpack_from('<I', DATA, o)[0]


def bf(v):
    """rotated-bfloat16 u16 -> float (0 stays 0)"""
    if v == 0: return 0.0
    bits = ((v >> 1) | ((v & 1) << 15)) << 16
    return struct.unpack('>f', struct.pack('>I', bits))[0]


def hermite(ptr, t):
    """sub_8223F270: key pair around time t (seconds); keys start at file offset ptr"""
    p = ptr
    while True:
        t0 = u16(p) / 60.0; t1 = u16(p + 8) / 60.0
        if t < t1 or u16(p + 8) == 0xffff or p + 16 > len(DATA): break
        # the game walks forward while t >= next key time; stop on the last pair
        if t == t1: p += 8; break
        if t > t1: p += 8; continue
        break
    t0 = u16(p) / 60.0; t1 = u16(p + 8) / 60.0
    if t == t0: return bf(u16(p + 2))
    p0, m0, p1, m1 = bf(u16(p + 2)), bf(u16(p + 6)), bf(u16(p + 10)), bf(u16(p + 12))
    d = t1 - t0; a = t - t0; s = a / d
    h00 = 2 * s ** 3 - 3 * s ** 2 + 1; h01 = 3 * s ** 2 - 2 * s ** 3
    h10 = d * (s ** 3 - 2 * s ** 2 + s); h11 = d * (s ** 3 - s ** 2)
    return h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1


class Block:
    def __init__(self, off):
        self.off = off
        self.n, self.S, self.A, self.B, self.n2, self.C, self.D = struct.unpack_from('<7I', DATA, off)
        self.count = (self.B - self.A) // 2

    def entry(self, i): return u16(self.A + 2 * i)

    def channel(self, i, t):
        e = self.entry(i); a = self.A + 2 * i
        if e == 0: return 0.0
        if e < 0x2000: return hermite(a - 2 * e, t)
        return bf(e)

    def values(self, t): return [self.channel(i, t) for i in range(self.count)]


def animation(index):
    """block for animation index (table A word)"""
    return Block(u32(0x10 + 4 * index))
