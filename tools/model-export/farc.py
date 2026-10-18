import struct, zlib, sys, os, collections
src,out=sys.argv[1],sys.argv[2]
d=open(src,'rb').read()
assert d[:4]==b'FArC'
hsize,align=struct.unpack('>II',d[4:12]); p=12; end=8+hsize
ents=[]
while p<end:
    q=d.index(b'\0',p); name=d[p:q].decode(); p=q+1
    off,csz,usz=struct.unpack('>III',d[p:p+12]); p+=12
    ents.append((name,off,csz,usz))
ext=collections.Counter()
for name,off,csz,usz in ents:
    raw=d[off:off+csz]
    try: data=zlib.decompress(raw,47)
    except Exception as e: data=raw
    open(os.path.join(out,name),'wb').write(data); ext[name.rsplit('.',1)[-1]]+=1
print(len(ents),'files',dict(ext))
