"""Export Fei-Yen as drawn in a captured frame: OBJ + MTL + PNG textures."""
import numpy as np, pickle, os, sys, bisect
from mrg import parse
from pvr import mg2_textures
from png4 import write_png_rgba

def strip_tris(n):
    out=[]
    for i in range(n-2):
        out.append((i,i+1,i+2) if i%2==0 else (i+1,i,i+2))
    return out

def unpack_normal(w):
    def f(v): return (v-1024 if v>=512 else v)/512.0
    n=np.array([f(w&1023),f((w>>10)&1023),f((w>>20)&1023)])
    return n/(np.linalg.norm(n)+1e-9)

def drawn_parts(recs, addr2obj):
    starts={a:v[0] for a,v in addr2obj.items()}
    parts=[]
    for r in recs:
        p=int(r[0x4C//4])
        if p-0x2C in starts:
            M=r[0x60//4:0xA0//4].view('>f4').reshape(4,4).astype(float)
            parts.append((starts[p-0x2C],M))
    return parts

def build(objs, parts, mats, T):
    """World-space triangles with UVs, grouped per part."""
    groups=[]
    for oi,M in parts:
        h=objs[oi]; mat=h['part']; tex=mats[mat][4]
        tw,th=(T[tex]['w'],T[tex]['h']) if tex<len(T) else (1,1)
        V=np.array([v[:3] for v in h['verts']],float)
        W=(np.c_[V,np.ones(len(V))]@M)[:,:3]
        Nrm=np.array([unpack_normal(v[3]) for v in h['verts']])@M[:3,:3]
        Nrm/=np.linalg.norm(Nrm,axis=1,keepdims=True)+1e-9
        tris=[]
        for p in h['polys']:
            for a,b,c in strip_tris(len(p['v'])):
                tri=[p['v'][a],p['v'][b],p['v'][c]]
                if len({t[0] for t in tri})<3: continue      # degenerate strip joint
                i,j,k=(t[0] for t in tri)
                # wind each triangle to face the way its stored vertex normals point
                if np.dot(np.cross(W[j]-W[i],W[k]-W[i]),Nrm[i]+Nrm[j]+Nrm[k])<0: tri=[tri[0],tri[2],tri[1]]
                tris.append([(W[t[0]],(t[1]/tw,t[2]/th),Nrm[t[0]]) for t in tri])
        groups.append(dict(obj=oi,mat=mat,tex=tex,tris=tris))
    return groups

def write_obj(groups, mats, T, outdir, name):
    os.makedirs(outdir,exist_ok=True)
    used=sorted({g['mat'] for g in groups})
    with open(os.path.join(outdir,name+'.mtl'),'w') as f:
        for m in used:
            t=mats[m][4]
            f.write('newmtl mat%02d\nKd 1 1 1\nKa 0 0 0\nKs 0 0 0\nillum 1\n'%m)
            if t<len(T):
                fn='tex%02d.png'%t
                write_png_rgba(os.path.join(outdir,fn),T[t]['rgba'])
                f.write('map_Kd %s\n'%fn)
                if (T[t]['rgba'][...,3]<255).any(): f.write('map_d %s\n'%fn)
            f.write('\n')
    with open(os.path.join(outdir,name+'.obj'),'w') as f:
        f.write('# Fei-Yen (Virtual-On Oratorio Tangram), exported from game data\nmtllib %s.mtl\n'%name)
        vi=1
        for g in groups:
            f.write('o part%04d\nusemtl mat%02d\n'%(g['obj'],g['mat']))
            for tri in g['tris']:
                for (x,y,z),(u,v),(nx,ny,nz) in tri:
                    # game is left-handed (x right, y up, z forward): mirror z for right-handed OBJ
                    f.write('v %.5f %.5f %.5f\nvt %.6f %.6f\nvn %.5f %.5f %.5f\n'%(x,y,-z,u,1-v,nx,ny,-nz))
                # mirroring flips winding, so reverse it
                f.write('f %d/%d/%d %d/%d/%d %d/%d/%d\n'%((vi+2,)*3+(vi+1,)*3+(vi,)*3)); vi+=3

if __name__=='__main__':
    import drawlist as DL
    from memobj import Mem, read_object
    mem=Mem(os.environ['MEM'])
    mats,T=mg2_textures('farc_data/fei1c.mg2')
    # every draw-list record whose pointer is a model part: read the part as loaded in memory
    objs=[]; parts=[]
    for r in DL.recs:
        o=read_object(mem,int(r[0x4C//4])-0x2C)
        if o is None or o['part']>=len(mats): continue
        M=r[0x60//4:0xA0//4].view('>f4').reshape(4,4).astype(float)
        parts.append((len(objs),M)); objs.append(o)
    groups=build(objs,parts,mats,T)
    out=sys.argv[1]; write_obj(groups,mats,T,out,'feiyen')
    pickle.dump((groups,),open(os.path.join(out,'groups.pkl'),'wb'))
    print(len(parts),'parts',sum(len(g['tris']) for g in groups),'triangles ->',out)
