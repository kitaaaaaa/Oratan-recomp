import numpy as np, struct, pickle, collections, bisect, sys, os
from mrg import parse
def regions(p):
    f=open(p,'rb')
    while True:
        h=f.read(8)
        if len(h)<8: break
        a,s=struct.unpack('<II',h); yield a,f.read(s)
import os
frame=os.environ.get('FRAME','f0')
mem=sorted((a,b) for a,b in regions(os.environ.get('MEM','frames7/%s_mem.bin'%frame)) if a<0xA7800000 and a+len(b)>0xA7400000)
def rd(addr,n):
    out=b''
    for a,b in mem:
        if a<=addr<a+len(b):
            take=b[addr-a:addr-a+n]; out+=take; addr+=len(take); n-=len(take)
            if n==0: return out
    return None
d=open(os.environ.get('DATA','frames7/%s_data.bin'%frame),'rb').read()
base=struct.unpack_from('>I',d,0x839EFB78-0x82890000)[0]; cnt=struct.unpack_from('>I',d,0x839EFB7C-0x82890000)[0]
R=0x15C
recs=[np.frombuffer(rd(base+i*R,R),dtype='>u4') for i in range(cnt)]
if __name__=='__main__':
    objs=parse('farc_data/fei1c.mrg'); addrs=pickle.load(open('loc_fei1c.pkl','rb'))
    starts={}
    for i,h in enumerate(addrs):
        for a in h: starts[a-4]=i
    ss=sorted(starts); rel=collections.Counter(); hit=0
    for r in recs:
        p=int(r[0x4C//4]); k=bisect.bisect_right(ss,p)-1
        if k>=0 and p-ss[k]<0x4000: hit+=1; rel[p-ss[k]]+=1
    print('drawlist %08X count %d; records into fei1c objects: %d'%(base,cnt,hit), rel.most_common(8))
