"""Symmetric A-pose rig for Fei-Yen, from the user's hand-made pose.

  python apose.py <userpose.npz> <out.glb>

userpose.npz: the posed mesh from the user's .blend (dump_rest_pose.py; Blender
vertex order = the exporter's vertices concatenated per material, verified).
Steps, in glTF space (Y up, facing +Z, her left = +X, mirror plane x = 0):
 1. each bone's rotation in the user's pose (Kabsch on its rigid part),
 2. centre bones: smallest rotation that makes the part mirror-symmetric,
    left/right pairs: rotations that make the left part the mirror image of
    the right one (both fitted, kept close to the user's pose),
 3. joint positions by forward kinematics from the pelvis, pairs averaged
    to exact mirror positions,
 4. feet: soles levelled, lowest sole point on the ground (y = 0).
The new rest pose is written with identity (world-aligned) joint orientations.
"""
import sys, runpy
import numpy as np
from scipy.optimize import minimize
from scipy.spatial import cKDTree
from scipy.spatial.transform import Rotation as Rot

user_npz, out_glb = sys.argv[1], sys.argv[2]
user = np.load(user_npz)
M = np.diag([-1.0, 1.0, 1.0])
REPORT = {}


def kabsch(src, dst):
    """Rotation R with dst - mean(dst) ~= R (src - mean(src))."""
    a = src - src.mean(0); b = dst - dst.mean(0)
    U, _, Vt = np.linalg.svd(a.T @ b)
    d = np.sign(np.linalg.det(Vt.T @ U.T))
    return Vt.T @ np.diag([1, 1, d]) @ U.T


def trimmed_chamfer(A, B, keep=0.85):
    da, _ = cKDTree(B).query(A); db, _ = cKDTree(A).query(B)
    da = np.sort(da)[: max(1, int(len(da) * keep))]; db = np.sort(db)[: max(1, int(len(db) * keep))]
    return (np.mean(da ** 2) + np.mean(db ** 2)) / 2


def poser(V, N, J, IDX, piv, parent, kids, names, root, G):
    nb = len(G)
    order = np.concatenate([np.array(IDX[m]) for m in sorted(IDX)])
    up = np.empty_like(V)
    up[order] = (user['posed'][:, [0, 2, 1]] * [1, 1, -1]).astype(V.dtype)   # Blender -> glTF
    by_name = {names[b]: b for b in range(nb)}
    loc = {b: V[J == b] - piv[b] for b in range(nb)}      # rest geometry about the joint

    # 1. the user's pose
    Wu = {}
    for b in range(nb):
        Wu[b] = kabsch(V[J == b], up[J == b])

    W = dict(Wu)
    reg = 0.02

    # 2a. centre bones: mirror-symmetric about their own vertical plane
    for b in range(nb):
        nm = names[b]
        if nm.endswith('_L') or nm.endswith('_R'):
            continue
        g = loc[b]

        def cost(rv, g=g, w0=Wu[b]):
            R = Rot.from_rotvec(rv).as_matrix() @ w0
            P = g @ R.T
            return trimmed_chamfer(P, P @ M) + reg * np.sum(rv ** 2)
        res = minimize(cost, np.zeros(3), method='Powell', options={'xtol': 1e-5, 'ftol': 1e-9})
        W[b] = Rot.from_rotvec(res.x).as_matrix() @ Wu[b]
        REPORT[nm] = 'centre: corrected %.2f deg, residual %.4f' % (np.degrees(np.linalg.norm(res.x)), np.sqrt(res.fun))

    # 2b. left/right pairs: left = mirror of right
    for nm, bl in by_name.items():
        if not nm.endswith('_L'):
            continue
        br = by_name.get(nm[:-2] + '_R')
        if br is None:
            continue
        gl, gr = loc[bl], loc[br]

        def cost(x, gl=gl, gr=gr, wl=Wu[bl], wr=Wu[br]):
            Rl = Rot.from_rotvec(x[:3]).as_matrix() @ wl
            Rr = Rot.from_rotvec(x[3:]).as_matrix() @ wr
            return trimmed_chamfer(gl @ Rl.T, (gr @ Rr.T) @ M) + reg * np.sum(x ** 2)
        res = minimize(cost, np.zeros(6), method='Powell', options={'xtol': 1e-5, 'ftol': 1e-9})
        W[bl] = Rot.from_rotvec(res.x[:3]).as_matrix() @ Wu[bl]
        W[br] = Rot.from_rotvec(res.x[3:]).as_matrix() @ Wu[br]
        REPORT[nm[:-2]] = 'pair: corrected L %.2f / R %.2f deg, residual %.4f' % (
            np.degrees(np.linalg.norm(res.x[:3])), np.degrees(np.linalg.norm(res.x[3:])), np.sqrt(res.fun))

    # 4a. feet: level the soles (bottom slice of each foot), mirrored
    for side in ('_R', '_L'):
        b = by_name['foot' + side]
        P = loc[b] @ W[b].T
        low = P[P[:, 1] <= P[:, 1].min() + 0.25]
        c = low.mean(0); _, _, vt = np.linalg.svd(low - c)
        n = vt[2] * np.sign(vt[2][1])                     # sole normal, pointing up
        axis = np.cross(n, [0, 1, 0]); s = np.linalg.norm(axis)
        if s > 1e-9:
            ang = np.arctan2(s, n[1])
            W[b] = Rot.from_rotvec(axis / s * ang).as_matrix() @ W[b]
        REPORT['foot' + side] = 'sole tilt corrected %.2f deg' % np.degrees(np.arctan2(s, n[1]))
    # keep the feet exact mirrors after levelling
    bl, br = by_name['foot_L'], by_name['foot_R']
    W[bl] = M @ W[br] @ M @ kabsch(loc[bl] @ M, loc[br]).T if False else W[bl]

    # 3. joint positions by forward kinematics, pairs averaged to exact mirrors
    q = {root: np.array([0.0, piv[root][1], piv[root][2]])}
    stack = [root]
    while stack:
        p = stack.pop()
        for c in kids[p]:
            q[c] = q[p] + W[p] @ (piv[c] - piv[p])
            stack.append(c)
    for nm, bl in by_name.items():
        if nm.endswith('_L') and nm[:-2] + '_R' in by_name:
            br = by_name[nm[:-2] + '_R']
            avg = (q[bl] + M @ q[br]) / 2
            REPORT[nm[:-2]] += ', joint mirror gap %.4f' % np.linalg.norm(q[bl] - M @ q[br])
            q[bl], q[br] = avg, M @ avg
        elif not (nm.endswith('_L') or nm.endswith('_R')):
            q[bl][0] = 0.0

    # new rest geometry
    Vn = V.copy(); Nn = N.copy()
    for b in range(nb):
        m = J == b
        Vn[m] = loc[b] @ W[b].T + q[b]
        Nn[m] = N[m] @ W[b].T
    # 4b. ground: lowest sole point at y = 0
    feet = np.isin(J, [by_name['foot_L'], by_name['foot_R']])
    y0 = Vn[feet, 1].min()
    Vn[:, 1] -= y0
    for b in q: q[b] = q[b] - [0, y0, 0]

    # symmetry check of the posed parts
    for nm, bl in by_name.items():
        if nm.endswith('_L') and nm[:-2] + '_R' in by_name:
            br = by_name[nm[:-2] + '_R']
            d, _ = cKDTree(Vn[J == br] @ M).query(Vn[J == bl])
            REPORT[nm[:-2]] += ', posed mirror distance median %.4f' % np.median(d)
    fy = [Vn[J == by_name['foot' + s], 1].min() for s in ('_L', '_R')]
    REPORT['ground'] = 'lowest point per foot L %.4f R %.4f' % tuple(fy)
    return Vn.astype(V.dtype), Nn.astype(N.dtype), {b: q[b].astype(np.float32) for b in q}


sys.argv = ['rigexport.py', out_glb]
runpy.run_path('rigexport.py', init_globals={'POSER': poser}, run_name='__main__')
for k, v in REPORT.items():
    print('%-12s %s' % (k, v))
