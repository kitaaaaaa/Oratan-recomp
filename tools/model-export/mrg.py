import struct, sys
def parse(path):
    d=open(path,'rb').read()
    U=lambda o: struct.unpack_from('<I',d,o)[0]
    F=lambda o: struct.unpack_from('<f',d,o)[0]
    n=0; offs=[]
    o=0x10
    while True:
        off,ident=struct.unpack_from('<II',d,o)
        if offs and (off<=offs[-1][0] or off+0x10>=len(d)): break
        offs.append((off,ident)); o+=8
    objs=[]
    todo=[(off+0x10,ident,None) for off,ident in offs]
    seen=set()
    while todo:
        p,ident,parent=todo.pop(0)
        if p<o or p in seen or p+56>len(d): continue   # entries pointing into the table are not objects
        seen.add(p)
        h=dict(ident=ident,at=p,link=U(p),pos=[F(p+4),F(p+8),F(p+12)],
               rot=[struct.unpack_from('<i',d,p+16+4*k)[0] for k in range(3)],
               scale=[F(p+28),F(p+32),F(p+36)],part=U(p+40),flags=U(p+44),radius=F(p+48),nv=U(p+52))
        q=p+56; verts=[]
        for k in range(h['nv']):
            x,y,z,w=struct.unpack_from('<fffI',d,q); verts.append((x,y,z,w)); q+=16
        nt=U(q); q+=4; polys=[]
        for k in range(nt):
            col,a,b,fl=struct.unpack_from('<IIII',d,q); nvp=(fl>>16)&0xFF
            nw=(nvp*3+1)//2
            hw=struct.unpack_from('<%dH'%(nw*2),d,q+16)
            polys.append(dict(color=col,flags=fl,v=[hw[3*i:3*i+3] for i in range(nvp)]))
            q+=16+4*nw
        h.update(verts=verts,polys=polys,end=q,alt_of=parent)
        objs.append(h)
        if h['link']:                       # alternate version of this part (file offset + 0x10)
            todo.insert(0,(h['link']+0x10,ident,len(objs)-1))
    return objs
if __name__=='__main__':
    objs=parse(sys.argv[1])
    for i,h in enumerate(objs):
        nxt=objs[i+1]['at'] if i+1<len(objs) else None
        bad=max((max(t[0] for t in p['v']) for p in h['polys']),default=-1)
        print('%3d at %05X end %05X next %s part %3d link %X nv %3d np %3d maxidx %d pos %s rot %s'%(i,h['at'],h['end'],
              '%05X'%nxt if nxt else '-',h['part'],h['link'],h['nv'],len(h['polys']),bad,
              ['%.2f'%v for v in h['pos']],['%.1f'%(r*360/2**32) for r in h['rot']]))
