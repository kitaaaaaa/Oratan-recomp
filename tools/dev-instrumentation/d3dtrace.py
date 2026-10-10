import struct, sys, collections, pickle
def load(p):
    d=open(p,'rb').read(); n=struct.unpack_from('<I',d,0)[0]
    addrs=struct.unpack_from('<%dI'%n,d,4); o=4+4*n; m=struct.unpack_from('<I',d,o)[0]; o+=4
    recs=[struct.unpack_from('<HHIIIIf',d,o+24*i) for i in range(m)]
    return addrs, recs
if __name__=='__main__':
    addrs,recs=load(sys.argv[1])
    callees=pickle.load(open('callgraph.pkl','rb'))
    marks=[i for i,r in enumerate(recs) if r[0]==0xFFFF]
    frames=[recs[a+1:b] for a,b in zip(marks,marks[1:])]
    print('frames recorded',len(frames),'calls/frame',[len(f) for f in frames[:8]])
    cnt=collections.Counter(r[0] for f in frames for r in f)
    nf=max(1,len(frames))
    print('\nfunction     calls/frame  callers  sample args (r3 r4 r5 r6 f1)')
    for fn,c in cnt.most_common():
        a='sub_%08X'%addrs[fn]
        ex=[r for f in frames for r in f if r[0]==fn][:2]
        print('%s %8.1f   %s'%(a,c/nf,'  |  '.join('%08X %08X %08X %08X %.3g'%r[2:] for r in ex)))
    unused=[('sub_%08X'%a) for i,a in enumerate(addrs) if i not in cnt]
    print('\nnot called in these frames:',len(unused))
