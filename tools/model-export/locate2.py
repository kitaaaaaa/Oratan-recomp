"""Map loaded object addresses to fei1c objects by comparing whole objects (UVs included)."""
import numpy as np, struct, pickle, sys, os, collections
from mrg import parse
def regions(p):
    f=open(p,'rb')
    while True:
        h=f.read(8)
        if len(h)<8: break
        a,s=struct.unpack('<II',h); yield a,f.read(s)
dump=sys.argv[1]; name=sys.argv[2] if len(sys.argv)>2 else 'fei1c'
objs=parse('farc_data/%s.mrg'%name); d=open('farc_data/%s.mrg'%name,'rb').read()
mem=list(regions(dump))
def rd(addr,n):
    for a,b in mem:
        if a<=addr and addr+n<=a+len(b): return b[addr-a:addr-a+n]
# candidate addresses by header signature (as before), grouped by signature
sig_hits={}
def find(sig):
    if sig in sig_hits: return sig_hits[sig]
    out=[]
    for a,b in mem:
        i=b.find(sig)
        while i!=-1: out.append(a+i); i=b.find(sig,i+1)
    sig_hits[sig]=out; return out
addr2obj={}; ambiguous=0
for i,h in enumerate(objs):
    p=h['at']; raw=d[p+4:p+0x38]
    sig=b''.join(raw[k:k+4][::-1] for k in range(0,len(raw),4))
    # polygon index/UV halfwords, file LE vs memory BE, relative to object start
    q=p+56+16*h['nv']+4; spans=[]
    for poly in h['polys']:
        n=len(poly['v']); nw=(n*3+1)//2
        spans.append((q+16-p, d[q+16:q+16+4*nw])); q+=16+4*nw
    for a in find(sig):
        ok=True; start=a-4
        for off,fb in spans:
            mb=rd(start+off,len(fb))
            if mb is None or mb!=b''.join(fb[k:k+2][::-1] for k in range(0,len(fb),2)): ok=False; break
        if ok: addr2obj.setdefault(start,[]).append(i)
dups=collections.Counter(len(v) for v in addr2obj.values())
print('addresses',len(addr2obj),'objects per address',dict(dups))
pickle.dump(addr2obj,open('addr2obj_%s.pkl'%name,'wb'))
