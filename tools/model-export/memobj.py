"""Read a loaded model object straight from guest memory (layout as loaded by the game)."""
import struct
def regions(p):
    f=open(p,'rb')
    while True:
        h=f.read(8)
        if len(h)<8: break
        a,s=struct.unpack('<II',h); yield a,f.read(s)
class Mem:
    def __init__(self,path): self.r=sorted(regions(path))
    def rd(self,addr,n):
        for a,b in self.r:
            if a<=addr and addr+n<=a+len(b): return b[addr-a:addr-a+n]
        return None
def read_object(mem,s):
    """s = object start (draw-list pointer - 0x2C). Returns dict like mrg.parse objects, or None."""
    raw=mem.rd(s,56)
    if raw is None: return None
    pos=struct.unpack('>3f',raw[4:16]); mat,flags,rad,nv=struct.unpack('>IIfI',raw[40:56])
    if nv==0 or nv>5000 or mat>255: return None
    vb=mem.rd(s+56,16*nv+4)
    verts=[struct.unpack_from('>3fI',vb,16*k) for k in range(nv)]
    npoly=struct.unpack_from('>I',vb,16*nv)[0]
    q=s+56+16*nv+4; polys=[]
    for _ in range(npoly):
        hdr=mem.rd(q,16); col,w1,w2,fl=struct.unpack('>4I',hdr)
        n=fl&0xFF; nw=(n*3+1)//2
        hw=struct.unpack('>%dH'%(nw*2),mem.rd(q+16,4*nw))
        polys.append(dict(color=col,flags=fl,v=[hw[3*i:3*i+3] for i in range(n)]))
        q+=16+4*nw
    for p in polys:
        if any(t[0]>=nv for t in p['v']): return None
    return dict(at=s,part=mat,flags=flags,nv=nv,verts=verts,polys=polys,pos=pos)
