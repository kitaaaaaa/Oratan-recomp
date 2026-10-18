"""Read a .glb back, pose some bones with standard glTF skinning, render rest vs posed."""
import json, struct, numpy as np, sys
from render import rasterize, write_png

def load(path):
    b = open(path, 'rb').read()
    jl = struct.unpack_from('<I', b, 12)[0]; js = json.loads(b[20:20 + jl])
    bl = struct.unpack_from('<I', b, 20 + jl)[0]; bin_ = b[28 + jl:28 + jl + bl]
    def acc(i):
        a = js['accessors'][i]; v = js['bufferViews'][a['bufferView']]
        dt = {5126: np.float32, 5125: np.uint32, 5121: np.uint8}[a['componentType']]
        n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}[a['type']]
        return np.frombuffer(bin_, dt, a['count'] * n, v['byteOffset']).reshape(a['count'], n)
    return js, acc

def rot(axis, deg):
    t = np.radians(deg); c, s = np.cos(t), np.sin(t)
    x, y, z = axis
    return np.array([[c + x*x*(1-c), x*y*(1-c) - z*s, x*z*(1-c) + y*s],
                     [y*x*(1-c) + z*s, c + y*y*(1-c), y*z*(1-c) - x*s],
                     [z*x*(1-c) - y*s, z*y*(1-c) + x*s, c + z*z*(1-c)]])

def skin(js, acc, pose):
    nodes = js['nodes']; skin = js['skins'][0]
    prim = js['meshes'][0]['primitives'][0]['attributes']
    P = acc(prim['POSITION']).astype(float); Jn = acc(prim['JOINTS_0'])[:, 0]
    IBM = acc(skin['inverseBindMatrices']).reshape(-1, 4, 4).transpose(0, 2, 1)   # column-major -> math
    glob = {}
    def world(n, parentM):
        node = nodes[n]; L = np.eye(4); L[:3, 3] = node.get('translation', [0, 0, 0])
        if node.get('name') in pose: L[:3, :3] = pose[node['name']]
        M = parentM @ L; glob[n] = M
        for c in node.get('children', []): world(c, M)
    world(js['scenes'][0]['nodes'][0], np.eye(4))
    out = np.zeros_like(P)
    for j, n in enumerate(skin['joints']):
        m = Jn == j
        K = glob[n] @ IBM[j]
        out[m] = (np.c_[P[m], np.ones(m.sum())] @ K.T)[:, :3]
    return out

js, acc = load(sys.argv[1])
idx = np.concatenate([acc(p['indices']).ravel() for p in js['meshes'][0]['primitives']])
restP = skin(js, acc, {})
pose = {'forearm_L': rot((1, 0, 0), -80), 'upper_arm_R': rot((0, 0, 1), -70),
        'thigh_R': rot((1, 0, 0), -60), 'shin_R': rot((1, 0, 0), 70), 'head': rot((0, 1, 0), 40)}
poseP = skin(js, acc, pose)
imgs = []
for P in (restP, poseP):
    tris = P[idx].reshape(-1, 3, 3) * [1, 1, -1]        # renderer is left-handed: flip z for display
    imgs.append(np.concatenate([rasterize(tris, W=300, H=420, view='front'), rasterize(tris, W=300, H=420, view='side')], 1))
write_png(sys.argv[2], np.concatenate(imgs, 1))
print('bones:', [n['name'] for n in js['nodes'] if 'mesh' not in n])
