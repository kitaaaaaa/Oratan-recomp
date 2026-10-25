"""Analyse an Oratan-trace log (trace_out folder).

    python trace_analyze.py <trace_out dir> [--tick N]

Prints: the motion-file range, which guest functions read the file (with read counts and the file offsets they touch),
which functions write the robot record's bone table (record +0x4B8, 24-byte entries), and for a chosen tick the sequence of
file reads next to the bone-table values the game wrote.
"""
import sys, struct, collections, re, os
import numpy as np

d = sys.argv[1] if len(sys.argv) > 1 else 'trace_out'
names = {}
for line in open(os.path.join(d, 'trace_names.txt')):
    i, n = line.split(); names[int(i)] = re.sub(r'^__imp__', '', n)
info = open(os.path.join(d, 'trace_info.txt')).read().strip().splitlines()
print('\n'.join(info))
m = re.search(r'motion file at 0x([0-9A-F]+)', info[-1]); lo = int(m.group(1), 16)
r = re.search(r'robot record at 0x([0-9A-F]+)', info[-1]); rec = int(r.group(1), 16)
dt = np.dtype([('tick', '<u4'), ('ea', '<u4'), ('value', '<u8'), ('fn', '<u2'), ('kind', 'u1'), ('size', 'u1')])
ev = np.fromfile(os.path.join(d, 'trace_events.bin'), dtype=dt)
print(len(ev), 'events;', 'ticks', ev['tick'].min() if len(ev) else None, '-', ev['tick'].max() if len(ev) else None)
reads = ev[ev['kind'] == 0]; writes = ev[ev['kind'] == 1]
print('\n== functions reading the motion file ==')
c = collections.Counter(reads['fn'].tolist())
for fn, n in c.most_common(25):
    sel = reads[reads['fn'] == fn]; offs = sel['ea'] - lo
    print('%-14s %8d reads   offsets %#x .. %#x   sizes %s' % (names.get(fn, 'fn%d' % fn), n, offs.min(), offs.max(), dict(collections.Counter(sel['size'].tolist()))))
print('\n== functions writing the robot record ==')
c = collections.Counter(writes['fn'].tolist())
for fn, n in c.most_common(25):
    sel = writes[writes['fn'] == fn]; offs = sel['ea'] - rec
    print('%-14s %8d writes  record offsets %#x .. %#x' % (names.get(fn, 'fn%d' % fn), n, offs.min(), offs.max()))
print('\n== writers of the bone table (record +0x4B8..+0x600) ==')
bt = writes[(writes['ea'] - rec >= 0x4B8) & (writes['ea'] - rec < 0x4B8 + 24 * 13)]
for fn, n in collections.Counter(bt['fn'].tolist()).most_common(10): print(' ', names.get(fn, 'fn%d' % fn), n)

if '--tick' in sys.argv:
    t = int(sys.argv[sys.argv.index('--tick') + 1])
    sel = ev[ev['tick'] == t]
    print('\n== tick %d: %d events, in order ==' % (t, len(sel)))
    for e in sel[:400]:
        k = 'R' if e['kind'] == 0 else 'W'
        where = ('file+%#07x' % (e['ea'] - lo)) if k == 'R' else ('rec+%#05x' % (e['ea'] - rec))
        print('%s %-14s %-12s size %d value %#x' % (k, names.get(int(e['fn']), 'fn%d' % int(e['fn'])), where, e['size'], e['value']))
