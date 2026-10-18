import numpy as np, zlib, struct
def write_png_rgba(path,img):
    h,w,_=img.shape
    raw=b''.join(b'\0'+img[y].tobytes() for y in range(h))
    def chunk(t,data): return struct.pack('>I',len(data))+t+data+struct.pack('>I',zlib.crc32(t+data)&0xffffffff)
    open(path,'wb').write(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw,6))+chunk(b'IEND',b''))
