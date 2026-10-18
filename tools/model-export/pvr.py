import struct, numpy as np
def untwiddle_index(x,y):
    i=0; b=0
    while (1<<b)<=max(x,y) or b<1:
        i|=((y>>b)&1)<<(2*b) | ((x>>b)&1)<<(2*b+1); b+=1
        if b>12: break
    return i
def to_rgba(px,fmt):
    px=px.astype(np.uint32)
    if fmt==0:   # ARGB1555
        a=((px>>15)&1)*255; r=((px>>10)&31)*255//31; g=((px>>5)&31)*255//31; b=(px&31)*255//31
    elif fmt==1: # RGB565
        a=np.full_like(px,255); r=((px>>11)&31)*255//31; g=((px>>5)&63)*255//63; b=(px&31)*255//31
    elif fmt==2: # ARGB4444
        a=((px>>12)&15)*17; r=((px>>8)&15)*17; g=((px>>4)&15)*17; b=(px&15)*17
    else: raise ValueError(fmt)
    return np.stack([r,g,b,a],-1).astype(np.uint8)
def textures(path):
    d=open(path,'rb').read(); out=[]
    i=d.find(b'GBIX')
    while i!=-1:
        gl=struct.unpack_from('<I',d,i+4)[0]; gid=struct.unpack_from('<I',d,i+8)[0]
        j=i+8+gl; sz,fmt,w,h=struct.unpack_from('<IIHH',d,j+4)
        pf,df=fmt&0xFF,(fmt>>8)&0xFF
        raw=np.frombuffer(d[j+16:j+16+w*h*2],dtype='<u2')
        if df in (0x01,0x02):     # square twiddled
            idx=np.array([[untwiddle_index(x,y) for x in range(w)] for y in range(h)])
            px=raw[idx]
        elif df in (0x09,0x0A):   # rectangle, linear
            px=raw.reshape(h,w)
        else: raise ValueError('data format %x'%df)
        out.append(dict(gbix=gid,w=w,h=h,pf=pf,df=df,rgba=to_rgba(px,pf)))
        i=d.find(b'GBIX',j+8+sz)
    return out

def untwiddle_map(w,h):
    # square twiddle over min(w,h) blocks, blocks laid out along the longer axis
    s=min(w,h); base=np.array([[untwiddle_index(x,y) for x in range(s)] for y in range(s)])
    idx=np.zeros((h,w),int)
    for by in range(h//s):
        for bx in range(w//s):
            idx[by*s:(by+1)*s,bx*s:(bx+1)*s]=base+(by*(w//s)+bx)*s*s
    return idx
def mg2_textures(path):
    """All textures in material-table order, paletted ones decoded with the file's PVPL."""
    m=open(path,'rb').read()
    sec1=struct.unpack_from('<I',m,8)[0]
    assert m[sec1:sec1+4]==b'PVPL'
    pal_n=struct.unpack_from('<H',m,sec1+14)[0]
    pal=to_rgba(np.frombuffer(m[sec1+16:sec1+16+2*pal_n],dtype='<u2'),0)
    nmat=struct.unpack_from('<I',m,0x10)[0]
    mats=[struct.unpack_from('<5I',m,0x14+20*k) for k in range(nmat)]
    tab=0x14+20*nmat; n=struct.unpack_from('<I',m,tab)[0]
    out=[]
    for o in struct.unpack_from('<%dI'%n,m,tab+4):
        a=o+0x10
        if m[a:a+4]==b'GBIX': a+=8+struct.unpack_from('<I',m,a+4)[0]
        sz,fmt,w,h=struct.unpack_from('<IIHH',m,a+4)
        pf,df=fmt&0xFF,(fmt>>8)&0xFF
        if df in (0x07,0x08):
            raw=np.frombuffer(m[a+16:a+16+w*h],dtype=np.uint8)
            idx=raw[untwiddle_map(w,h)]
            rgba=pal[idx].copy()
            rgba[idx==0,3]=0      # palette entry 0 (pure green) is the transparency key
        else:
            raw=np.frombuffer(m[a+16:a+16+w*h*2],dtype='<u2')
            px=raw[untwiddle_map(w,h)] if df in (0x01,0x02) else raw.reshape(h,w)
            rgba=to_rgba(px,pf)
        out.append(dict(w=w,h=h,pf=pf,df=df,rgba=rgba))
    return mats,out
