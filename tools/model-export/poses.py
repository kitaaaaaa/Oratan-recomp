"""Per-capture map: loaded object address -> world matrix, for every drawn model part."""
import numpy as np, struct, sys, os
from memobj import Mem, read_object
def capture_parts(mem_path, data_path):
    mem=Mem(mem_path); d=open(data_path,'rb').read()
    base=struct.unpack_from('>I',d,0x839EFB78-0x82890000)[0]; cnt=struct.unpack_from('>I',d,0x839EFB7C-0x82890000)[0]
    out={}
    for i in range(cnt):
        r=np.frombuffer(mem.rd(base+i*0x15C,0x15C),dtype='>u4')
        s=int(r[0x4C//4])-0x2C; o=read_object(mem,s)
        if o is None or o['part']>=47: continue
        out[s]=(r[0x60//4:0xA0//4].view('>f4').reshape(4,4).astype(float),o)
    return out
if __name__=='__main__':
    A=capture_parts('caps/cap0_mem.bin','caps/cap0_data.bin')
    B=capture_parts('frames7/f0_mem.bin','frames7/f0_data.bin')
    print('cap0 parts',len(A),'battle parts',len(B),'common',len(set(A)&set(B)))
