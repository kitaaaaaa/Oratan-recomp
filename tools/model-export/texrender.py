import numpy as np, pickle, sys
from pvr import mg2_textures
from png4 import write_png_rgba
def render(groups, T, yaw_deg, W=420, H=560, pitch_deg=0):
    tris=[]; uvs=[]; tex=[]
    for g in groups:
        for tri in g['tris']:
            tris.append([t[0] for t in tri]); uvs.append([t[1] for t in tri]); tex.append(g['tex'])
    P=np.array(tris,float); UV=np.array(uvs,float)
    c=P.reshape(-1,3).mean(0); P=P-c
    y=np.radians(yaw_deg); pz=np.radians(pitch_deg)
    Ry=np.array([[np.cos(y),0,np.sin(y)],[0,1,0],[-np.sin(y),0,np.cos(y)]])
    Rx=np.array([[1,0,0],[0,np.cos(pz),-np.sin(pz)],[0,np.sin(pz),np.cos(pz)]])
    P=P@Ry.T@Rx.T
    S=P[...,:2]; D=-P[...,2]
    ext=np.abs(S).max()*1.05; sc=min(W,H)/2/ext
    X=S[...,0]*sc+W/2; Y=H/2-S[...,1]*sc
    img=np.full((H,W,3),45.0); zb=np.full((H,W),-1e9)
    n=np.cross(P[:,1]-P[:,0],P[:,2]-P[:,0]); n/=np.linalg.norm(n,axis=1,keepdims=True)+1e-9
    shade=0.55+0.45*np.abs(n@np.array([0.3,0.4,0.86]))
    for i in range(len(P)):
        xs,ys=X[i],Y[i]
        x0,x1=int(max(xs.min(),0)),int(min(xs.max(),W-1)); y0,y1=int(max(ys.min(),0)),int(min(ys.max(),H-1))
        if x1<x0 or y1<y0: continue
        gx,gy=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5)
        den=(ys[1]-ys[2])*(xs[0]-xs[2])+(xs[2]-xs[1])*(ys[0]-ys[2])
        if abs(den)<1e-9: continue
        l0=((ys[1]-ys[2])*(gx-xs[2])+(xs[2]-xs[1])*(gy-ys[2]))/den
        l1=((ys[2]-ys[0])*(gx-xs[2])+(xs[0]-xs[2])*(gy-ys[2]))/den; l2=1-l0-l1
        m=(l0>=0)&(l1>=0)&(l2>=0)
        z=l0*D[i,0]+l1*D[i,1]+l2*D[i,2]; sub=zb[y0:y1+1,x0:x1+1]
        t=tex[i]
        if t<len(T):
            tx=T[t]['rgba']; th,tw=tx.shape[:2]
            u=l0*UV[i,0,0]+l1*UV[i,1,0]+l2*UV[i,2,0]; v=l0*UV[i,0,1]+l1*UV[i,1,1]+l2*UV[i,2,1]
            px=tx[(np.floor(v*th).astype(int))%th,(np.floor(u*tw).astype(int))%tw].astype(float)
            m&=px[...,3]>8
        else: px=np.full(gx.shape+(4,),255.0)
        m&=z>sub; sub[m]=z[m]
        img[y0:y1+1,x0:x1+1][m]=px[m][:,:3]*shade[i]
    return img
if __name__=='__main__':
    groups,=pickle.load(open(sys.argv[1],'rb'))
    _,T=mg2_textures('farc_data/fei1c.mg2')
    yaws=[float(a) for a in sys.argv[3].split(',')] if len(sys.argv)>3 else [0,90,180,270]
    img=np.concatenate([render(groups,T,a) for a in yaws],axis=1)
    out=np.concatenate([img,np.full(img.shape[:2]+(1,),255.0)],-1).astype(np.uint8)
    write_png_rgba(sys.argv[2],out)
