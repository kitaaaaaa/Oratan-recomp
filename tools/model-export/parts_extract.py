"""Turn an Oratan-trace run (trace_parts.bin + trace_objs.bin + trace_events.bin) into per-tick bone rotations for the rig.

    python parts_extract.py <trace_out dir> <out.npz>

Per tick: the draw-list world matrix of every Fei-Yen part -> the part's bone (via the rigdata2/tree2 part groups) ->
rotation relative to the A-pose rest (same maths as dump_poses.py), with the world heading removed.
"""
import sys, os, struct, re, pickle, collections, runpy, tempfile
import numpy as np
from scipy.spatial.transform import Rotation as Rot

# Paths: VOOT_WORK = the work folder (farc_data, export/FeiYen/feiyen_apose_v2.glb, the rig pickles); the rig scripts are next to this one.
TRACE = os.path.abspath(sys.argv[1]); OUT = os.path.abspath(sys.argv[2])
WORK = os.path.abspath(os.environ.get('VOOT_WORK') or ('.' if os.path.isdir('farc_data') else 'work'))     # run it from the work folder, or set VOOT_WORK
TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
os.chdir(WORK)

# ---- rig data (all bones, v2 skeleton incl. open parts) ----
REST_GLB = os.path.join(tempfile.gettempdir(), 'voot_rest_v2.glb')       # rigexport2's output: the v2 skeleton in the Customize-capture rest pose
sys.argv = ['rigexport2.py', REST_GLB]
g = runpy.run_path(TOOLS + '/rigexport2.py', run_name='__main__')
keys, G, names, rest, D, Ry, S, root, objs = g['keys'], g['G'], g['names'], g['rest'], g['D'], g['Ry'], g['S'], g['root'], g['objs']
T = S @ Ry.astype(float); Ti = np.linalg.inv(T)
exec(open(TOOLS + '/glbcheck.py').read().split('def rot(')[0])        # load()
def kabsch(src, dst):
    a = src - src.mean(0); b = dst - dst.mean(0); U, _, Vt = np.linalg.svd(a.T @ b); d = np.sign(np.linalg.det(Vt.T @ U.T))
    return Vt.T @ np.diag([1, 1, d]) @ U.T
def per_bone(path):
    js, acc = load(path); sk = js['skins'][0]; nm = [js['nodes'][n]['name'] for n in sk['joints']]
    a = js['meshes'][0]['primitives'][0]['attributes']; P = acc(a['POSITION']).astype(float); Jn = acc(a['JOINTS_0'])[:, 0]
    return {n: P[Jn == j] for j, n in enumerate(nm)}
A = per_bone(os.path.join(WORK, 'export', 'FeiYen', 'feiyen_apose_v2.glb')); R0 = per_bone(REST_GLB)
W = {n: kabsch(R0[n], A[n]) for n in R0 if n in A and len(R0[n]) == len(A[n])}
print('bones with A-pose rotation:', len(W), 'of', len(R0))

# ---- object signatures: trace object -> rig part key ----
def be32f(b, o): return struct.unpack_from('>f', b, o)[0]
sig = collections.defaultdict(list)
for k in keys:
    o = objs[k]; vs = o['verts'][:3]
    sig[(o['nv'], o['part'], tuple(round(v[0], 3) for v in vs), tuple(round(v[1], 3) for v in vs), tuple(round(v[2], 3) for v in vs))].append(k)
raw = open(os.path.join(TRACE, 'trace_objs.bin'), 'rb').read(); p = 0; trace_objs = {}
while p + 8 <= len(raw):
    obj, size = struct.unpack_from('<II', raw, p); body = raw[p + 8:p + 8 + size]; p += 8 + size
    if len(body) < size: break
    nv = struct.unpack_from('>I', body, 0x34)[0]; mat = struct.unpack_from('>I', body, 0x28)[0]
    vs = [struct.unpack_from('>3f', body, 56 + 16 * i) for i in range(min(3, nv))]
    trace_objs[obj] = (nv, mat, tuple(round(v[0], 3) for v in vs), tuple(round(v[1], 3) for v in vs), tuple(round(v[2], 3) for v in vs))
mapped = {}; ambiguous = 0
used = collections.Counter()
for obj, s in sorted(trace_objs.items()):
    cand = sig.get(s)
    if not cand: continue
    k = cand[min(used[s], len(cand) - 1)]; used[s] += 1; mapped[obj] = k
    if len(cand) > 1: ambiguous += 1
print('trace objects', len(trace_objs), 'mapped', len(mapped), 'ambiguous (identical signature)', ambiguous)

# ---- per-tick world matrices ----
raw = open(os.path.join(TRACE, 'trace_parts.bin'), 'rb').read(); p = 0; ticks = []
nvtx = {i: len(objs[k]['verts']) for i, k in enumerate(keys)}
group_of = {}
for b, grp in enumerate(G):
    for i in grp: group_of[keys[i]] = b
bone_rep = {}                                                     # bone -> its largest part
for b, grp in enumerate(G):
    bone_rep[b] = max(grp, key=lambda i: objs[keys[i]]['nv'])
def polar(M):
    U, _, Vt = np.linalg.svd(M); Rm = U @ Vt
    if np.linalg.det(Rm) < 0: U[:, -1] *= -1; Rm = U @ Vt
    return Rm
rows = []
while p + 6 <= len(raw):
    tick, kept = struct.unpack_from('<IH', raw, p); p += 6
    if p + kept * 68 > len(raw): break
    mats = {}
    for _ in range(kept):
        obj = struct.unpack_from('<I', raw, p)[0]; M = np.array(struct.unpack_from('>16f', raw, p + 4), float).reshape(4, 4); p += 68
        if obj in mapped: mats[mapped[obj]] = M
    rows.append((tick, mats))
print('ticks with parts:', len(rows), ' first', rows[0][0], 'last', rows[-1][0])

bone_names = {b: names[b] for b in range(len(G))}
out_q = {}; out_ticks = []; root_disp = []; heading = []
pelvis_b = root
for tick, mats in rows:
    ok = {}
    for b in range(len(G)):
        cands = [i for i in G[b] if keys[i] in mats]
        if not cands: continue
        i = bone_rep[b] if bone_rep[b] in cands else max(cands, key=lambda c: objs[keys[c]]['nv']); k = keys[i]
        Dm = np.linalg.inv(rest[k]) @ mats[k]
        Df = Ti @ polar(Dm[:3, :3]) @ T
        Wb = W.get(bone_names[b])
        if Wb is None: continue
        ok[bone_names[b]] = Rot.from_matrix((Df.T @ Wb.T)).as_quat()
        if b == pelvis_b:
            c = np.array([v[:3] for v in objs[k]['verts']]).mean(0)
            root_disp.append(((np.append(c, 1) @ mats[k])[:3] - (np.append(c, 1) @ rest[k])[:3]) @ T)
    if 'pelvis' not in ok: root_disp = root_disp[:len(out_ticks)]; continue
    fw = Rot.from_quat(ok['pelvis']).apply([0, 0, 1]); yaw = np.arctan2(fw[0], fw[2])
    Y = Rot.from_euler('y', -yaw)
    for n, q in ok.items(): out_q.setdefault(n, {})[tick] = (Y * Rot.from_quat(q)).as_quat()
    out_ticks.append(tick); heading.append(yaw)
np.savez(OUT, ticks=np.array(out_ticks), root_disp=np.array(root_disp), heading=np.array(heading),
         names=np.array(sorted(out_q)), **{'q_' + n: np.array([out_q[n].get(t, [np.nan] * 4) for t in out_ticks]) for n in out_q})
print('saved', OUT, len(out_ticks), 'ticks,', len(out_q), 'bones')
