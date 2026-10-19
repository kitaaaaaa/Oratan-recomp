"""Rig data v2: base parts (Customize capture) + the parts the game swaps in during actions
(open V-converter + V-disk, articulated tail boosters, open hand with fingers), from all captures.
Writes rigdata2.pkl: keys, rest, objs, poses, sets (key -> set name)."""
import struct, pickle, collections, numpy as np
from mrg import parse
from memobj import Mem, read_object
objs_f = parse('farc_data/fei1c.mrg')
def fsig(o): return (o['nv'], o['part'], tuple(tuple(map(tuple, p['v'])) for p in o['polys']))
bysig = collections.defaultdict(list)
for i, o in enumerate(objs_f): bysig[fsig(o)].append(i)
# swapped-in sets, by fei1c object index (first index of identical copies)
SETS = {'vconverter_open': range(55, 60), 'booster_open_a': range(89, 98), 'booster_open_b': range(107, 116),
        'hand_open': range(72, 89)}
# base parts the sets replace (shown when the set is not)
CLOSED = {'vconverter_closed': [49], 'boosters_closed': [24, 25, 26, 27], 'hand_closed': [43]}
# rest placement of each set: capture to take its shape from, and a base part it is carried by
REST_FROM = {'vconverter_open': (('caps', 'cap17'), 52), 'booster_open_a': (('caps', 'cap17'), None),
             'booster_open_b': (('caps', 'cap17'), None), 'hand_open': (('caps', 'cap23'), 0)}
CAPS = [('caps', 'customize')] + [('caps', 'cap%d' % i) for i in range(33)] + [('caps2', 'cap%d' % i) for i in range(9)]

def capture(d, n):
    mem = Mem('%s/%s_mem.bin' % (d, n)); data = open('%s/%s_data.bin' % (d, n), 'rb').read()
    base = struct.unpack_from('>I', data, 0x839EFB78 - 0x82890000)[0]; cnt = struct.unpack_from('>I', data, 0x839EFB7C - 0x82890000)[0]
    out = {}; geo = {}; occ = collections.Counter()
    for i in range(cnt):
        r = np.frombuffer(mem.rd(base + i * 0x15C, 0x15C), dtype='>u4')
        o = read_object(mem, int(r[0x4C // 4]) - 0x2C)
        if o is None or o['part'] >= 47: continue
        s = fsig(o); k = (s, occ[s]); occ[s] += 1
        out[k] = r[0x60 // 4:0xA0 // 4].view('>f4').reshape(4, 4).astype(float); geo[k] = o
    return out, geo

def fid(k): return bysig.get(k[0], [None])[0]
caps = {c: capture(*c) for c in CAPS}
rest0, geo0 = caps[('caps', 'customize')]
keys = list(rest0); objs = dict(geo0); sets = {}
for k in keys:
    for name, ids in CLOSED.items():
        if fid(k) in ids: sets[k] = name
rest = dict(rest0); inv = np.linalg.inv
pelvis = max(keys, key=lambda k: k[0][0] if fid(k) is None else -1)   # largest non-fei1c part = pelvis (nv192)
for name, ids in SETS.items():
    c, anchor_id = REST_FROM[name]; P, Gm = caps[c]
    anchor = pelvis if anchor_id is None else [k for k in keys if fid(k) == anchor_id][0]
    for k in P:
        if fid(k) in ids and k not in rest:
            keys.append(k); objs[k] = Gm[k]; sets[k] = name
            rest[k] = P[k] @ inv(P[anchor]) @ rest0[anchor]
# parts that are carried by a swapped-in part while it is drawn: the Dreamcast lid face (54) rides on
# the open door (58) and sits on the closed box otherwise -> separate closed and open copies
SPLIT = {54: (58, 'vconverter_closed', 'vconverter_open', 52)}
# 54's vertices sit on the outer face of the full-thickness closed box (49); the game draws the same
# quad in the half-thickness door's frame (58), one door-thickness off the door's outer face. The open
# copy is moved flush by that offset (49's outer vertices minus 58's, in their shared local frame).
def local_verts(i): return np.array([v[:3] for v in objs_f[i]['verts']])
dv = local_verts(49) - local_verts(58); FLUSH = np.eye(4); FLUSH[3, :3] = -dv[np.argmax(np.linalg.norm(dv, axis=1))]
for pid, (carrier, cset, oset, anchor_id) in SPLIT.items():
    k = [k for k in keys if fid(k) == pid][0]; ko = (k[0], 'open'); sets[k] = cset; sets[ko] = oset
    P, _ = caps[REST_FROM['vconverter_open'][0]]; anchor = [a for a in keys if fid(a) == anchor_id][0]
    keys.append(ko); objs[ko] = objs[k]; rest[ko] = FLUSH @ P[k] @ inv(P[anchor]) @ rest0[anchor]
poses = []
for c in CAPS[1:]:
    P, _ = caps[c]; P = dict(P)
    for pid, (carrier, *_r) in SPLIT.items():
        k = [k for k in keys if fid(k) == pid][0]
        if any(fid(x) == carrier for x in P) and k in P: P[(k[0], 'open')] = FLUSH @ P.pop(k)
    poses.append({k: M for k, M in P.items() if k in rest})
labels = ['%s.%s' % (fid(k) if fid(k) is not None else 'x%d' % k[0][0], 'o' if k[1] == 'open' else k[1]) for k in keys]   # fei1c index.occurrence ('o' = open copy)
pickle.dump(dict(keys=keys, rest=rest, objs=objs, poses=poses, sets=sets, labels=labels, caps=CAPS[1:]), open('rigdata2.pkl', 'wb'))
cnt = collections.Counter(sets.get(k, 'body') for k in keys)
print(len(keys), 'parts', dict(cnt), len(poses), 'poses')
for name in SETS: print(' ', name, 'in', sum(1 for p in poses if any(sets.get(k) == name for k in p)), 'poses')
