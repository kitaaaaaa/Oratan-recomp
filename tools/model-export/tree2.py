"""Bones v2 (skeleton.py + tree.py over rigdata2.pkl): rigid groups, least-squares pivots, bone tree.
Pairs of parts that are never drawn together (closed vs open versions) are never compared.
Overrides, each checked against the captures (see MODEL_FORMAT_NOTES.md):
- open hand: palm -> thumb chain and four 3-segment fingers (layout read from the captures;
  every joint solves with ~0 residual);
- open boosters: both bases hang from the pelvis (best residual, as the closed boosters);
- open V-converter: one half (55+58) swings on a hinge solved exactly from the closed piece 49
  (= both halves stacked); the V-disk (57) slides, so its bone sits at its own centre;
- for swapped-in parts, a pivot more than 1 unit from the child's geometry (near-rigid pair, pivot
  not determined by the motion) is moved to where the two parts touch."""
import pickle, numpy as np
D = pickle.load(open('rigdata2.pkl', 'rb'))
keys, rest, objs, sets, labels = D['keys'], D['rest'], D['objs'], D['sets'], D['labels']
n = len(keys); L = {l: i for i, l in enumerate(labels)}
def is_open(k): return 'open' in sets.get(k, '')
# the Customize capture is a real pose for base parts; swap parts' rest is derived, not observed
allposes = [{k: rest[k] for k in keys if not is_open(k)}] + D['poses']
inv = np.linalg.inv
def rel(c, p):
    return [P[keys[c]] @ inv(P[keys[p]]) for P in allposes if keys[c] in P and keys[p] in P]
def rigid_err(c, p):
    R = rel(c, p)
    return max(np.abs(r - R[0]).max() for r in R) if len(R) >= 2 else np.inf
def joint(c, p):
    R = rel(c, p)
    if len(R) < 2: return None, np.inf, len(R)
    rows = []; rhs = []
    for A in R:
        for j in range(3):
            row = np.zeros(6); row[:3] = A[:3, j]; row[3 + j] = -1; rows.append(row); rhs.append(-A[3, j])
    sol = np.linalg.lstsq(np.array(rows), np.array(rhs), rcond=None)[0]
    return sol[:3], np.sqrt(np.mean((np.array(rows) @ sol - np.array(rhs)) ** 2)), len(R)
def verts_world(i):
    o = objs[keys[i]]; V = np.array([v[:3] for v in o['verts']]); return (np.c_[V, np.ones(len(V))] @ rest[keys[i]])[:, :3]

# --- overrides ---
HAND = {'73.0': '72.0', '74.0': '73.0', '75.0': '74.0', '76.0': '72.0'}
for f in range(4): HAND['77.%d' % f] = '72.0'; HAND['78.%d' % f] = '77.%d' % f
for f in range(3): HAND['79.%d' % f] = '78.%d' % f
HAND['82.0'] = '78.3'
OWN = [['55.0', '58.0', '54.o'], ['57.0']] + [[l] for l in HAND] + [['72.0']]     # parts that get a bone of their own
def door_hinge():
    """V-converter door: closed frame = piece 49's frame shifted by the slab offset (49's outer
    vertices minus 58's), open frame = 58's. Returns a world point on the hinge axis."""
    i49, i58 = L['49.0'], L['58.0']
    v49 = np.array([v[:3] for v in objs[keys[i49]]['verts']]); v58 = np.array([v[:3] for v in objs[keys[i58]]['verts']])
    d = v49 - v58; off = d[np.argmax(np.linalg.norm(d, axis=1))]
    O = np.eye(4); O[3, :3] = off; C = O @ rest[keys[i49]]
    W = inv(C) @ rest[keys[i58]]; R, t = W[:3, :3], W[3, :3]
    x = np.linalg.lstsq((R - np.eye(3)).T, -t, rcond=None)[0]
    w, v = np.linalg.eig(R.T); ax = np.real(v[:, np.argmin(abs(w - 1))]); ax /= np.linalg.norm(ax)
    c = verts_world(i58).mean(0); p = x + np.dot(c - x, ax) * ax          # axis point nearest the door
    print('V-converter hinge: %.1f deg about %s through %s' % (np.degrees(np.arccos((np.trace(R) - 1) / 2)), np.round(ax, 3), np.round(p, 2)))
    return p

RG = np.array([[rigid_err(c, p) if c != p else 0 for p in range(n)] for c in range(n)])
own = {L[l] for g in OWN for l in g}
grp = list(range(n))
def find(a):
    while grp[a] != a: a = grp[a]
    return a
for a in range(n):
    for b in range(n):
        if a != b and RG[a, b] < 0.01 and a not in own and b not in own: grp[find(a)] = find(b)
for g in OWN:
    for l in g[1:]: grp[find(L[l])] = find(L[g[0]])
groups = {}
for a in range(n): groups.setdefault(find(a), []).append(a)
G = list(groups.values()); gof = {i: b for b, g in enumerate(G) for i in g}
# rigidity was only tested on poses where both parts are drawn, so check every bone's members pairwise
for g in G:
    for a in g:
        for b in g:
            if a < b and RG[a, b] >= 0.01 and RG[a, b] != np.inf:
                print('WARNING: bone members %s and %s move apart (%.3f)' % (labels[a], labels[b], RG[a, b]))
GV = [np.vstack([verts_world(i) for i in g]) for g in G]
def contact(cg, pg):
    d = np.linalg.norm(GV[cg][:, None] - GV[pg][None], axis=2); a, b = np.unravel_index(np.argmin(d), d.shape)
    return (GV[cg][a] + GV[pg][b]) / 2
def gjoint(cg, pg, pair=None):
    best = pair or max(((a, b) for a in G[cg] for b in G[pg]), key=lambda ab: len(rel(*ab)))
    x, err, cnt = joint(*best)
    if x is None: return None, np.inf, np.inf, np.inf
    pw = (np.append(x, 1) @ rest[keys[best[0]]])[:3]
    return pw, err, np.min(np.linalg.norm(GV[pg] - pw, axis=1)), np.min(np.linalg.norm(GV[cg] - pw, axis=1))
pelvis = max(range(n), key=lambda i: objs[keys[i]]['nv'] if not sets.get(keys[i]) else -1)
root = [g for g in range(len(G)) if pelvis in G[g]][0]
torso = gof[L['52.0']]
fixed = {gof[L[c]]: (gof[L[p]], (L[c], L[p])) for c, p in HAND.items()}
fixed[gof[L['57.0']]] = (torso, None); fixed[gof[L['55.0']]] = (torso, None)
for b in ('94.0', '112.0'):            # open booster bases: pelvis fits best (err 0.011 vs 0.029 off the other booster)
    fixed[gof[L[b]]] = (root, (L[b], pelvis))
cand = {(c, p): gjoint(c, p) for c in range(len(G)) for p in range(len(G)) if c != p and c not in fixed}
attached = {root: None}; info = {}
while len(attached) < len(G):
    best = None
    for c in range(len(G)):
        if c in attached: continue
        if c in fixed:
            p, pair = fixed[c]
            if p not in attached: continue
            if c == gof[L['57.0']]: pw = GV[c].mean(0); r = (pw, 0.0, np.min(np.linalg.norm(GV[p] - pw, axis=1)), 0.0)
            elif c == gof[L['55.0']]: pw = door_hinge(); r = (pw, 0.0, np.min(np.linalg.norm(GV[p] - pw, axis=1)), np.min(np.linalg.norm(GV[c] - pw, axis=1)))
            else: r = gjoint(c, p, pair)
            best = (-1, c, p) + r; break
        for p in attached:
            pw, err, dp, dc = cand[(c, p)]
            if pw is None: continue
            cost = err * 20 + dp + dc
            if best is None or cost < best[0]: best = (cost, c, p, pw, err, dp, dc)
    cost, c, p, pw, err, dp, dc = best
    if dc > 1.0 and all(is_open(keys[i]) for i in G[c]):   # pivot not pinned down by the motion (base rig unchanged)
        pw = contact(c, p); dp = np.min(np.linalg.norm(GV[p] - pw, axis=1)); dc = np.min(np.linalg.norm(GV[c] - pw, axis=1)); err = -err
    attached[c] = p; info[c] = (pw, err, dp, dc)
def gset(g): return sorted({sets.get(keys[i], 'body') for i in G[g]})
def show(g, depth=0):
    extra = '' if g == root else '  pivot %s err %.3f dpar %.2f dchild %.2f%s' % (np.round(info[g][0], 2), abs(info[g][1]), *info[g][2:], ' (contact)' if info[g][1] < 0 else '')
    print('  ' * depth + 'bone %d %s %s' % (g, '/'.join(gset(g)), [labels[i] for i in G[g]]) + extra)
    for c, p in attached.items():
        if p == g: show(c, depth + 1)
print(len(G), 'bones'); show(root)
pickle.dump(dict(groups=G, parent=attached, info=info, root=root), open('tree2.pkl', 'wb'))
