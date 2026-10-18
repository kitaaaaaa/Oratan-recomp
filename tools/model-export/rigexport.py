"""Export rigged Fei-Yen as glTF binary (.glb): one bone per rigid group, game-derived joints."""
import pickle, json, struct, numpy as np, io, zlib, sys
from skeleton import D, keys
from pvr import mg2_textures
from feiexport import strip_tris, unpack_normal

tree = pickle.load(open('tree.pkl', 'rb'))
G, parent, info, root = tree['groups'], tree['parent'], tree['info'], tree['root']
mats, TEX = mg2_textures('farc_data/fei1c.mg2')
rest = D['rest']; objs = D['objs']
S = np.diag([1, 1, -1.0])                      # game left-handed -> glTF right-handed

part_bone = {i: b for b, g in enumerate(G) for i in g}
V = []; N = []; UV = []; J = []; IDX = {}
for i, k in enumerate(keys):
    o = objs[k]; M = rest[k]; mat = o['part']; tex = mats[mat][4]
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
                IDX.setdefault(mat, []).append(len(V))
                V.append(W[t[0]] @ S); N.append(Nn[t[0]] @ S); UV.append((t[1] / tw, t[2] / th)); J.append(part_bone[i])
V = np.array(V, np.float32); N = np.array(N, np.float32); UV = np.array(UV, np.float32); J = np.array(J)

kids = {b: [c for c, p in parent.items() if p == b] for b in range(len(G))}
piv = {}
for b in range(len(G)):
    if b == root:
        piv[b] = V[J == b].mean(0) @ S        # undo mirror; converted below with the rest
    else:
        piv[b] = info[b][0]
piv = {b: (p @ S).astype(np.float32) for b, p in piv.items()}
off = np.array([piv[root][0], V[:, 1].min(), piv[root][2]], np.float32)   # feet on the ground
V -= off; piv = {b: p - off for b, p in piv.items()}

# face +Z: the hand holding the sabre (the longest part) is her LEFT hand (user-confirmed)
ext = {b: np.ptp(V[J == b], 0).max() for b in range(len(G)) if (J == b).any()}
sabre = max(ext, key=ext.get)
def chain_to_torso(b):
    path = [b]
    while parent[path[-1]] is not None: path.append(parent[path[-1]])
    return path
path = chain_to_torso(sabre)
left_shoulder = path[-3]                       # torso's child on the sabre arm
others = [c for c in kids[parent[left_shoulder]] if c != left_shoulder and kids[c] and not (len(kids[c]) == 2 and all(not kids[x] for x in kids[c]))]
right_shoulder = others[0]
lat = piv[left_shoulder] - piv[right_shoulder]; lat[1] = 0; lat /= np.linalg.norm(lat)
ang = np.arctan2(lat[2], lat[0])                # rotate about Y so lat -> +X
c_, s_ = np.cos(ang), np.sin(ang)
Ry = np.array([[c_, 0, -s_], [0, 1, 0], [s_, 0, c_]], np.float32)   # row-vector: v @ Ry
V = V @ Ry; N = N @ Ry; piv = {b: p @ Ry for b, p in piv.items()}
print('sabre bone', sabre, 'rotated by %.1f deg' % np.degrees(ang))

def side(b):
    gx = (V[J == b][:, 0].mean() if (J == b).any() else 0) + piv[b][0]
    return 'L' if gx > 0 else 'R'
names = {root: 'pelvis'}
for c in kids[root]:
    if not kids[c]:
        names[c] = 'skirt_flap_' + side(c)
    elif len(kids[c]) >= 2:
        names[c] = 'torso'
    else:
        s = side(c); names[c] = 'thigh_' + s; sh = kids[c][0]; names[sh] = 'shin_' + s
        if kids[sh]: names[kids[sh][0]] = 'foot_' + s
torso = [c for c in kids[root] if names.get(c) == 'torso'][0]
for c in kids[torso]:
    if kids[c] and all(not kids[x] for x in kids[c]) and len(kids[c]) == 2:
        names[c] = 'head'
        for t in kids[c]: names[t] = 'twintail_' + side(t)
    else:
        s = side(c); names[c] = 'shoulder_' + s
        if kids[c]:
            ua = kids[c][0]; names[ua] = 'upper_arm_' + s
            if kids[ua]:
                fa = kids[ua][0]; names[fa] = 'forearm_' + s
                if kids[fa]: names[kids[fa][0]] = 'hand_' + s
for b in range(len(G)): names.setdefault(b, 'bone%d' % b)
# sibling pairs that got the same name: the one further to her left (+x) is L
for p in range(len(G)):
    by = {}
    for c in kids[p]: by.setdefault(names[c].rsplit('_', 1)[0], []).append(c)
    for base, cs in by.items():
        if len(cs) == 2 and names[cs[0]] == names[cs[1]]:
            cs.sort(key=lambda c: (V[J == c][:, 0].mean() if (J == c).any() else 0) + piv[c][0])
            names[cs[0]] = base + '_R'; names[cs[1]] = base + '_L'

# Optional re-posing (apose.py): returns new rest geometry and joint pivots.
if 'POSER' in globals():
    V, N, piv = POSER(V, N, J, IDX, piv, parent, kids, names, root, G)
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
images = []; textures = []; materials = []; texmap = {}; prims = []
for mat, idx in sorted(IDX.items()):
    t = mats[mat][4]
    m = dict(name='mat%02d' % mat, pbrMetallicRoughness=dict(metallicFactor=0.0, roughnessFactor=1.0))
    if t < len(TEX):
        if t not in texmap:
            vi = add_view(png_bytes(TEX[t]['rgba'])); images.append(dict(bufferView=vi, mimeType='image/png', name='tex%02d' % t))
            textures.append(dict(source=len(images) - 1, sampler=0)); texmap[t] = len(textures) - 1
        m['pbrMetallicRoughness']['baseColorTexture'] = dict(index=texmap[t])
        if (TEX[t]['rgba'][..., 3] < 255).any(): m['alphaMode'] = 'MASK'; m['alphaCutoff'] = 0.5
    materials.append(m)
    ai = add_acc(np.array(idx, np.uint32), 5125, 'SCALAR', 34963)
    prims.append(dict(attributes=dict(POSITION=aP, NORMAL=aN, TEXCOORD_0=aT, JOINTS_0=aJ, WEIGHTS_0=aW), indices=ai, material=len(materials) - 1))

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
mesh_node = len(nodes); nodes.append(dict(name='FeiYen', mesh=0, skin=0))
arm_node = len(nodes); nodes.append(dict(name='FeiYen_rig', children=[bone_node[root], mesh_node]))
gl = dict(asset=dict(version='2.0', generator='Oratan-recomp model export'), scene=0, scenes=[dict(nodes=[arm_node])], nodes=nodes,
          meshes=[dict(name='FeiYen', primitives=prims)],
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
print('wrote', out, len(V) // 3, 'triangles', len(G), 'bones')
for b in order: print('  %-14s parent %-12s pivot %s' % (names[b], names[parent[b]] if parent[b] is not None else '-', np.round(piv[b], 2)))
