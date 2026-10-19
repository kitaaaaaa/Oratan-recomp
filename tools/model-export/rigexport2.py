"""Export rigged Fei-Yen v2 as glTF binary (.glb) from rigdata2.pkl + tree2.pkl: one skeleton, and one
mesh per swappable piece so the open and closed versions can be shown/hidden:
FeiYen (body), FeiYen_vconverter_closed/_open, FeiYen_boosters_closed/_open, FeiYen_hand_closed/_open."""
import pickle, json, struct, numpy as np, io, zlib, sys
from pvr import mg2_textures
from feiexport import strip_tris, unpack_normal

D = pickle.load(open('rigdata2.pkl', 'rb'))
keys, rest, objs, sets, labels = D['keys'], D['rest'], D['objs'], D['sets'], D['labels']
tree = pickle.load(open('tree2.pkl', 'rb'))
G, parent, info, root = tree['groups'], tree['parent'], tree['info'], tree['root']
mats, TEX = mg2_textures('farc_data/fei1c.mg2')
S = np.diag([1, 1, -1.0])                      # game left-handed -> glTF right-handed
MESH = {'vconverter_closed': 'FeiYen_vconverter_closed', 'vconverter_open': 'FeiYen_vconverter_open',
        'boosters_closed': 'FeiYen_boosters_closed', 'booster_open_a': 'FeiYen_boosters_open', 'booster_open_b': 'FeiYen_boosters_open',
        'hand_closed': 'FeiYen_hand_closed', 'hand_open': 'FeiYen_hand_open'}
def is_open(k): return 'open' in sets.get(k, '')

# optional variant (argv[2] = closed | open): keep only that version of the swappable pieces, drop
# bones left without geometry (always whole leaf chains), and put everything in one mesh
VARIANT = sys.argv[2] if len(sys.argv) > 2 else None
if VARIANT:
    drop = {i for i, k in enumerate(keys) if sets.get(k) and (is_open(k) != (VARIANT == 'open'))}
    keep_b = [b for b, g in enumerate(G) if any(i not in drop for i in g)]
    newb = {b: j for j, b in enumerate(keep_b)}
    old_keys = keys; keys = [k for i, k in enumerate(old_keys) if i not in drop]
    newi = {i: j for j, i in enumerate(i for i in range(len(old_keys)) if i not in drop)}
    G = [[newi[i] for i in G[b] if i not in drop] for b in keep_b]
    parent = {newb[b]: (newb[p] if p is not None else None) for b, p in parent.items() if b in newb}
    info = {newb[b]: v for b, v in info.items() if b in newb}; root = newb[root]
    labels = [labels[i] for i in range(len(old_keys)) if i not in drop]
    MESH = {}

part_bone = {i: b for b, g in enumerate(G) for i in g}
V = []; N = []; UV = []; J = []; IDX = {}
for i, k in enumerate(keys):
    o = objs[k]; M = rest[k]; mat = o['part']; tex = mats[mat][4]; mesh = MESH.get(sets.get(k), 'FeiYen')
    tw, th = (TEX[tex]['w'], TEX[tex]['h']) if tex < len(TEX) else (1, 1)
    P = np.array([v[:3] for v in o['verts']]); W = (np.c_[P, np.ones(len(P))] @ M)[:, :3]
    Nn = np.array([unpack_normal(v[3]) for v in o['verts']]) @ M[:3, :3]
    Nn /= np.linalg.norm(Nn, axis=1, keepdims=True) + 1e-9
    for poly in o['polys']:
        for a, b, c in strip_tris(len(poly['v'])):
            tri = [poly['v'][a], poly['v'][b], poly['v'][c]]
            if len({t[0] for t in tri}) < 3: continue
            x, y, z = (t[0] for t in tri)
            if np.dot(np.cross(W[y] - W[x], W[z] - W[x]), Nn[x] + Nn[y] + Nn[z]) < 0:
                tri = [tri[0], tri[2], tri[1]]
            for t in tri[::-1]:                 # mirroring z flips handedness -> reverse winding
                IDX.setdefault((mesh, mat), []).append(len(V))
                V.append(W[t[0]] @ S); N.append(Nn[t[0]] @ S); UV.append((t[1] / tw, t[2] / th)); J.append(part_bone[i])
V = np.array(V, np.float32); N = np.array(N, np.float32); UV = np.array(UV, np.float32); J = np.array(J)

kids = {b: [c for c, p in parent.items() if p == b] for b in range(len(G))}
base = {b for b in range(len(G)) if any(not is_open(keys[i]) for i in G[b])}
bkids = {b: [c for c in kids[b] if c in base] for b in range(len(G))}   # base skeleton, named as in v1
piv = {}
for b in range(len(G)):
    piv[b] = V[J == b].mean(0) @ S if b == root else info[b][0]
piv = {b: (p @ S).astype(np.float32) for b, p in piv.items()}
off = np.array([piv[root][0], V[:, 1].min(), piv[root][2]], np.float32)   # feet on the ground
V -= off; piv = {b: p - off for b, p in piv.items()}

# face +Z: the hand holding the sabre (the longest part) is her LEFT hand (user-confirmed)
ext = {b: np.ptp(V[J == b], 0).max() for b in base if (J == b).any()}
sabre = max(ext, key=ext.get)
path = [sabre]
while parent[path[-1]] is not None: path.append(parent[path[-1]])
left_shoulder = path[-3]
others = [c for c in bkids[parent[left_shoulder]] if c != left_shoulder and bkids[c] and not (len(bkids[c]) == 2 and all(not bkids[x] for x in bkids[c]))]
right_shoulder = others[0]
lat = piv[left_shoulder] - piv[right_shoulder]; lat[1] = 0; lat /= np.linalg.norm(lat)
ang = np.arctan2(lat[2], lat[0])
c_, s_ = np.cos(ang), np.sin(ang)
Ry = np.array([[c_, 0, -s_], [0, 1, 0], [s_, 0, c_]], np.float32)
V = V @ Ry; N = N @ Ry; piv = {b: p @ Ry for b, p in piv.items()}
print('sabre bone', sabre, 'rotated by %.1f deg' % np.degrees(ang))

def gx(b): return (V[J == b][:, 0].mean() if (J == b).any() else 0)
def side(b): return 'L' if gx(b) + piv[b][0] > 0 else 'R'
names = {root: 'pelvis'}
for c in bkids[root]:
    if not bkids[c]:
        names[c] = ('booster_' if any(sets.get(keys[i]) == 'boosters_closed' for i in G[c]) else 'skirt_flap_') + side(c)
    elif len(bkids[c]) >= 2:
        names[c] = 'torso'
    else:
        s = side(c); names[c] = 'thigh_' + s; sh = bkids[c][0]; names[sh] = 'shin_' + s
        if bkids[sh]: names[bkids[sh][0]] = 'foot_' + s
torso = [c for c in bkids[root] if names.get(c) == 'torso'][0]
for c in bkids[torso]:
    if bkids[c] and all(not bkids[x] for x in bkids[c]) and len(bkids[c]) == 2:
        names[c] = 'head'
        for t in bkids[c]: names[t] = 'twintail_' + side(t)
    else:
        s = side(c); names[c] = 'shoulder_' + s
        if bkids[c]:
            ua = bkids[c][0]; names[ua] = 'upper_arm_' + s
            if bkids[ua]:
                fa = bkids[ua][0]; names[fa] = 'forearm_' + s
                if bkids[fa]: names[bkids[fa][0]] = 'hand_' + s
for p in range(len(G)):
    by = {}
    for c in bkids[p]: by.setdefault(names.get(c, '').rsplit('_', 1)[0], []).append(c)
    for bn, cs in by.items():
        if bn and len(cs) == 2 and names[cs[0]] == names[cs[1]]:
            cs.sort(key=lambda c: gx(c) + piv[c][0]); names[cs[0]] = bn + '_R'; names[cs[1]] = bn + '_L'
# swapped-in bones
lab = {b: [labels[i] for i in G[b]] for b in range(len(G))}
def bone_of(l): return [b for b in range(len(G)) if l in lab[b]][0]
if '58.0' in labels:                    # open pieces present
    names[bone_of('58.0')] = 'vconverter_door'; names[bone_of('57.0')] = 'v_disk'
    palm = bone_of('72.0'); hs = side(parent[palm]); names[palm] = 'hand_open_' + hs
    names[bone_of('76.0')] = 'hand_open_76_' + hs
    for j, l in enumerate(['73.0', '74.0', '75.0']): names[bone_of(l)] = 'thumb_%d_%s' % (j + 1, hs)
    for f, fn in enumerate(['index', 'middle', 'ring', 'pinky']):      # 77.0 sits next to the thumb
        for j, l in enumerate(['77.%d' % f, '78.%d' % f, '79.%d' % f if f < 3 else '82.0']): names[bone_of(l)] = '%s_%d_%s' % (fn, j + 1, hs)
    bsets = {}
    for setn in ('booster_open_a', 'booster_open_b'):
        bs = [b for b in range(len(G)) if all(sets.get(keys[i]) == setn for i in G[b])]
        top = [b for b in bs if parent[b] not in bs][0]; order_ = [top]
        for b in order_: order_ += [c for c in kids[b] if c in bs]
        bsets[setn] = order_
    xa, xb = (np.mean([gx(b) for b in bsets[s]]) for s in ('booster_open_a', 'booster_open_b'))
    for setn, s in zip(('booster_open_a', 'booster_open_b'), ('L', 'R') if xa > xb else ('R', 'L')):
        for j, b in enumerate(bsets[setn]): names[b] = 'booster_open_%s_%d' % (s, j + 1)
for b in range(len(G)): names.setdefault(b, 'bone%d' % b)

# Optional re-posing (apose2.py): returns new rest geometry and joint pivots.
if 'POSER' in globals():
    V, N, piv = POSER(V, N, J, piv, parent, kids, names, root, G)
buf = io.BytesIO(); views = []; accs = []
def add_view(data, target=None):
    while buf.tell() % 4: buf.write(b'\0')
    o = buf.tell(); buf.write(data); v = dict(buffer=0, byteOffset=o, byteLength=len(data))
    if target: v['target'] = target
    views.append(v); return len(views) - 1
def add_acc(arr, ctype, typ, target=None, minmax=False):
    vi = add_view(arr.tobytes(), target)
    a = dict(bufferView=vi, componentType=ctype, count=len(arr), type=typ)
    if minmax: a['min'] = arr.min(0).tolist(); a['max'] = arr.max(0).tolist()
    accs.append(a); return len(accs) - 1
def png_bytes(img):
    h, w, _ = img.shape; raw = b''.join(b'\0' + img[y].tobytes() for y in range(h))
    def ch(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b'')

aP = add_acc(V, 5126, 'VEC3', 34962, True); aN = add_acc(N, 5126, 'VEC3', 34962); aT = add_acc(UV, 5126, 'VEC2', 34962)
J4 = np.zeros((len(J), 4), np.uint8); J4[:, 0] = J; W4 = np.zeros((len(J), 4), np.float32); W4[:, 0] = 1
aJ = add_acc(J4, 5121, 'VEC4', 34962); aW = add_acc(W4, 5126, 'VEC4', 34962)
images = []; textures = []; materials = []; texmap = {}; matmap = {}; meshes = {}
for (mesh, mat), idx in sorted(IDX.items()):
    if mat not in matmap:
        t = mats[mat][4]
        m = dict(name='mat%02d' % mat, pbrMetallicRoughness=dict(metallicFactor=0.0, roughnessFactor=1.0))
        if t < len(TEX):
            if t not in texmap:
                vi = add_view(png_bytes(TEX[t]['rgba'])); images.append(dict(bufferView=vi, mimeType='image/png', name='tex%02d' % t))
                textures.append(dict(source=len(images) - 1, sampler=0)); texmap[t] = len(textures) - 1
            m['pbrMetallicRoughness']['baseColorTexture'] = dict(index=texmap[t])
            if (TEX[t]['rgba'][..., 3] < 255).any(): m['alphaMode'] = 'MASK'; m['alphaCutoff'] = 0.5
        materials.append(m); matmap[mat] = len(materials) - 1
    ai = add_acc(np.array(idx, np.uint32), 5125, 'SCALAR', 34963)
    meshes.setdefault(mesh, []).append(dict(attributes=dict(POSITION=aP, NORMAL=aN, TEXCOORD_0=aT, JOINTS_0=aJ, WEIGHTS_0=aW), indices=ai, material=matmap[mat]))

order = [root]
for b in order: order += kids[b]
nodes = []; bone_node = {}
for b in order:
    p = parent[b]; tr = piv[b] - (piv[p] if p is not None else 0)
    bone_node[b] = len(nodes); nodes.append(dict(name=names[b], translation=[float(x) for x in tr]))
for b in order:
    if kids[b]: nodes[bone_node[b]]['children'] = [bone_node[c] for c in kids[b]]
IBM = np.array([[[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [-piv[b][0], -piv[b][1], -piv[b][2], 1]] for b in range(len(G))], np.float32)
aI = add_acc(IBM.reshape(len(G), 16), 5126, 'MAT4')
mesh_names = ['FeiYen'] + sorted(m for m in meshes if m != 'FeiYen')
mesh_nodes = []
for mi, m in enumerate(mesh_names):
    mesh_nodes.append(len(nodes)); nodes.append(dict(name=m, mesh=mi, skin=0))
arm_node = len(nodes); nodes.append(dict(name='FeiYen_rig', children=[bone_node[root]] + mesh_nodes))
gl = dict(asset=dict(version='2.0', generator='Oratan-recomp model export'), scene=0, scenes=[dict(nodes=[arm_node])], nodes=nodes,
          meshes=[dict(name=m, primitives=meshes[m]) for m in mesh_names],
          skins=[dict(joints=[bone_node[b] for b in range(len(G))], inverseBindMatrices=aI, skeleton=bone_node[root])],
          materials=materials, textures=textures, images=images,
          samplers=[dict(magFilter=9728, minFilter=9728, wrapS=10497, wrapT=10497)],
          accessors=accs, bufferViews=views, buffers=[dict(byteLength=0)])
data = buf.getvalue()
while len(data) % 4: data += b'\0'
gl['buffers'][0]['byteLength'] = len(data)
js = json.dumps(gl).encode()
while len(js) % 4: js += b' '
out = sys.argv[1]
with open(out, 'wb') as f:
    f.write(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(data)))
    f.write(struct.pack('<II', len(js), 0x4E4F534A)); f.write(js)
    f.write(struct.pack('<II', len(data), 0x004E4942)); f.write(data)
print('wrote', out, len(V) // 3, 'triangles', len(G), 'bones', len(mesh_names), 'meshes', mesh_names)
for b in order: print('  %-20s parent %-18s pivot %s' % (names[b], names[parent[b]] if parent[b] is not None else '-', np.round(piv[b], 2)))
