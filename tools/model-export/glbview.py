"""Render a .glb's rest mesh (front/side/back) to PNG: python glbview.py in.glb out.png"""
import json, struct, sys, numpy as np
from render import rasterize, write_png
def load_glb(path):
    b = open(path, 'rb').read()
    jl = struct.unpack_from('<I', b, 12)[0]; js = json.loads(b[20:20 + jl])
    bl = struct.unpack_from('<I', b, 20 + jl)[0]; bin_ = b[28 + jl:28 + jl + bl]
    def acc(i):
        a = js['accessors'][i]; v = js['bufferViews'][a['bufferView']]
        dt = {5126: np.float32, 5125: np.uint32, 5121: np.uint8}[a['componentType']]
        n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}[a['type']]
        return np.frombuffer(bin_, dt, a['count'] * n, v['byteOffset']).reshape(a['count'], n)
    return js, acc
if __name__ == '__main__':
    js, acc = load_glb(sys.argv[1])
    P = acc(js['meshes'][0]['primitives'][0]['attributes']['POSITION']).astype(float)
    idx = np.concatenate([acc(p['indices']).ravel() for p in js['meshes'][0]['primitives']])
    tris = P[idx].reshape(-1, 3, 3) * [1, 1, -1]      # renderer is left-handed
    lo = tris.reshape(-1, 3)[:, [0, 1]].min(0); hi = tris.reshape(-1, 3)[:, [0, 1]].max(0)
    write_png(sys.argv[2], np.concatenate([rasterize(tris, W=420, H=600, view=v) for v in ('front', 'side', 'back')], 1))
