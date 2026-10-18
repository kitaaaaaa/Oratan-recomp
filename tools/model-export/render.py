import numpy as np, zlib, struct
def write_png(path, img):
    h,w,_=img.shape
    raw=b''.join(b'\0'+img[y].astype(np.uint8).tobytes() for y in range(h))
    def chunk(t,data): return struct.pack('>I',len(data))+t+data+struct.pack('>I',zlib.crc32(t+data)&0xffffffff)
    open(path,'wb').write(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw,6))+chunk(b'IEND',b''))
def rasterize(tris, W=400, H=600, view='front', bounds=None, colors=None):
    """tris: (N,3,3) world coords. Orthographic view, flat shaded."""
    t=np.asarray(tris,float)
    if view=='front':  P=t[...,[0,1]]; D=t[...,2]
    elif view=='side': P=t[...,[2,1]]; D=-t[...,0]
    elif view=='back': P=t[...,[0,1]]*[-1,1]; D=-t[...,2]
    elif view=='top':  P=t[...,[0,2]]; D=t[...,1]
    lo,hi=bounds if bounds else (P.reshape(-1,2).min(0),P.reshape(-1,2).max(0))
    s=min((W-20)/max(hi[0]-lo[0],1e-6),(H-20)/max(hi[1]-lo[1],1e-6))
    S=(P-lo)*s+10; S[...,1]=H-S[...,1]
    img=np.full((H,W,3),40.0); zb=np.full((H,W),-1e9)
    n=np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0]); n/=np.linalg.norm(n,axis=1,keepdims=True)+1e-9
    light=np.array([0.3,0.5,0.8]); light/=np.linalg.norm(light)
    shade=0.25+0.75*np.abs(n@light)
    for i in range(len(t)):
        a,b,c=S[i]; xs=[a[0],b[0],c[0]]; ys=[a[1],b[1],c[1]]
        x0,x1=int(max(min(xs),0)),int(min(max(xs),W-1)); y0,y1=int(max(min(ys),0)),int(min(max(ys),H-1))
        if x1<x0 or y1<y0: continue
        X,Y=np.meshgrid(np.arange(x0,x1+1)+0.5,np.arange(y0,y1+1)+0.5)
        den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(den)<1e-9: continue
        l0=((b[1]-c[1])*(X-c[0])+(c[0]-b[0])*(Y-c[1]))/den
        l1=((c[1]-a[1])*(X-c[0])+(a[0]-c[0])*(Y-c[1]))/den
        l2=1-l0-l1
        m=(l0>=0)&(l1>=0)&(l2>=0)
        z=l0*D[i,0]+l1*D[i,1]+l2*D[i,2]
        sub=zb[y0:y1+1,x0:x1+1]; m&=z>sub
        sub[m]=z[m]
        col=np.array(colors[i] if colors is not None else (200,200,210),float)*shade[i]
        img[y0:y1+1,x0:x1+1][m]=col
    return img
