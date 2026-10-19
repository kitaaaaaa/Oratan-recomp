"""A-pose for rig v2, taken from the user-approved v1 A-pose (export/FeiYen/feiyen_apose.glb).

  python apose2.py <v1_rigged.glb> <v1_apose.glb> <out.glb> [closed|open]

v1 and v2 base bones hold identical geometry, so each v1 bone's A-pose rotation (Kabsch between
the two v1 files) and joint position are reused as they are. Bones new in v2 follow the bone they
replace or hang from: open hand -> hand_R, open boosters -> booster_L/R, everything else its
parent. Joint orientations stay world-aligned, as in v1."""
import sys, runpy
import numpy as np
exec(open('glbcheck.py').read().split('def rot(')[0])       # load()
v1_rig, v1_apose, out_glb = sys.argv[1:4]
V1_NAME = {'booster_L': 'skirt_flap_L', 'booster_R': 'skirt_flap_R'}   # renamed in v2

def kabsch(src, dst):
    a = src - src.mean(0); b = dst - dst.mean(0)
    U, _, Vt = np.linalg.svd(a.T @ b)
    d = np.sign(np.linalg.det(Vt.T @ U.T))
    return Vt.T @ np.diag([1, 1, d]) @ U.T

def per_bone(path):
    js, acc = load(path); sk = js['skins'][0]; names = [js['nodes'][n]['name'] for n in sk['joints']]
    a = js['meshes'][0]['primitives'][0]['attributes']; P = acc(a['POSITION']).astype(float); Jn = acc(a['JOINTS_0'])[:, 0]
    glob = {}
    def walk(n, t):
        nd = js['nodes'][n]; g = t + np.array(nd.get('translation', [0, 0, 0])); glob[nd['name']] = g
        for c in nd.get('children', []): walk(c, g)
    walk(js['scenes'][0]['nodes'][0], np.zeros(3))
    return {nm: P[Jn == j] for j, nm in enumerate(names)}, glob
rig1, p1 = per_bone(v1_rig); ap1, q1 = per_bone(v1_apose)        # p1 / q1: v1 joint positions, rest / A-pose
W1 = {nm: kabsch(rig1[nm], ap1[nm]) for nm in rig1}

def poser(V, N, J, piv, parent, kids, names, root, G):
    nb = len(G); by = {names[b]: b for b in range(nb)}
    target = {}                                   # new bone -> v1 bone whose rotation it takes
    for b in range(nb):
        nm = names[b]
        if nm.startswith('hand_open_') and not nm.startswith('hand_open_76'): target[b] = 'hand_' + nm[-1]
        elif nm.startswith('booster_open_') and nm.endswith('_1'): target[b] = V1_NAME['booster_' + nm.split('_')[2]]
    order = [root]
    for b in order: order += kids[b]
    W = {}; q = {}
    for b in order:                               # base bones first, then the new ones top-down
        v1n = V1_NAME.get(names[b], names[b])
        if v1n in W1: W[b] = W1[v1n]; q[b] = q1[v1n]
    for b in order:
        if b in W: continue
        if b in target:                           # v1 bone, which may not be in this file (open variant)
            t = target[b]; W[b] = W1[t]; q[b] = q1[t] + W1[t] @ (piv[b] - p1[t])
        else:
            t = parent[b]; W[b] = W[t]; q[b] = q[t] + W[t] @ (piv[b] - piv[t])
    Vn = V.copy(); Nn = N.copy()
    for b in range(nb):
        m = J == b
        Vn[m] = (V[m] - piv[b]) @ W[b].T + q[b]; Nn[m] = N[m] @ W[b].T
    return Vn.astype(V.dtype), Nn.astype(N.dtype), {b: q[b].astype(np.float32) for b in q}

sys.argv = ['rigexport2.py', out_glb] + sys.argv[4:5]        # optional variant: closed | open
runpy.run_path('rigexport2.py', init_globals={'POSER': poser}, run_name='__main__')
