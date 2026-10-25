"""Which animation plays on which ticks, from an Oratan-trace run (default mode: table reads only).

    python segments.py <trace_out dir> <out.pkl>

Every game tick the animation evaluator (sub_82240808) reads one word of FEI.mt's animation table: the file offset read is
0x10 + 4*index, so the animation index is (offset - 0x10) / 4 (table A, offsets 0x10..0x9E3); a read at 0x9E4 or beyond
is table B and is reported as the string 'B<n>'. The result is a list of [animation, first tick, last tick] runs.
"""
import sys, os, re, pickle, collections
import numpy as np

d, out = sys.argv[1], sys.argv[2]
info = open(os.path.join(d, 'trace_info.txt')).read().strip().splitlines()[-1]
lo = int(re.search(r'motion file at 0x([0-9A-F]+)', info).group(1), 16)
names = {}
for line in open(os.path.join(d, 'trace_names.txt')):
    i, n = line.split(); names[int(i)] = re.sub(r'^__imp__', '', n)
dt = np.dtype([('tick', '<u4'), ('ea', '<u4'), ('value', '<u8'), ('fn', '<u2'), ('kind', 'u1'), ('size', 'u1')])
ev = np.fromfile(os.path.join(d, 'trace_events.bin'), dtype=dt)
sel_fn = [k for k, v in names.items() if v == 'sub_82240808'][0]
ev = ev[(ev['fn'] == sel_fn) & (ev['kind'] == 0) & (ev['size'] == 4)]
anim = {}
for e in ev:
    off = int(e['ea']) - lo
    anim[int(e['tick'])] = (off - 0x10) // 4 if 0x10 <= off < 0x9E4 else 'B%d' % ((off - 0x9E4) // 4)
seg = []; prev = None
for t in sorted(anim):
    if prev is not None and anim[t] == seg[-1][0] and t - prev <= 2: seg[-1][2] = t
    else: seg.append([anim[t], t, t])
    prev = t
pickle.dump(seg, open(out, 'wb'))
print(len(seg), 'segments;', len(set(s[0] for s in seg)), 'different animations;', collections.Counter(s[0] for s in seg).most_common(8))
