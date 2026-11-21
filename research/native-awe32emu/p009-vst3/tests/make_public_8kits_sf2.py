#!/usr/bin/env python3
"""P183: eight ORIGINAL uncopyrighted SF2 drum-kit patches in bank128.
No copyrighted user/Creative samples, banks or program names.
"""
from pathlib import Path
import math,struct,sys
def ck(k,b):return k+struct.pack('<I',len(b))+b+(b'\0' if len(b)&1 else b'')
def ls(k,b):return ck(b'LIST',k+b)
def nm(x):return x.encode('ascii')[:20].ljust(20,b'\0')
def phdr(name,p,b,i):return struct.pack('<20sHHHIII',nm(name),p,b,i,0,0,0)
def inst(name,i):return struct.pack('<20sH',nm(name),i)
def bag(i):return struct.pack('<HH',i,0)
def gen(op,v):return struct.pack('<HH',op,v&65535)
def shdr(name,s,e,l0,l1):return struct.pack('<20sIIIIIBbHH',nm(name),s,e,l0,l1,44100,60,0,0,1)
count=44100
pcm=struct.pack('<%dh'%count,*[int(8500*math.sin(2*math.pi*256*i/44100)) for i in range(count)])
pdta=b''.join([
 ck(b'phdr',b''.join(phdr('Public Drum %d'%p,p,128,p) for p in range(8))+phdr('EOP',0,0,8)),
 ck(b'pbag',b''.join(bag(p) for p in range(9))),
 ck(b'pmod',bytes(10)),
 ck(b'pgen',b''.join(gen(41,p) for p in range(8))+gen(0,0)),
 ck(b'inst',b''.join(inst('Public Kit %d'%p,p) for p in range(8))+inst('EOI',8)),
 ck(b'ibag',b''.join(bag(p*4) for p in range(9))),
 ck(b'imod',bytes(10)),
 ck(b'igen',b''.join(gen(37,0)+gen(48,p*16)+gen(53,0)+gen(54,1) for p in range(8))+gen(0,0)),
 ck(b'shdr',shdr('PublicKitSine',0,count,64,count-128)+shdr('EOS',count,count,count,count))
])
info=ck(b'ifil',struct.pack('<HH',2,1))+ck(b'INAM',b'P183 public 8 drumkits\0')
data=ck(b'RIFF',b'sfbk'+ls(b'INFO',info)+ls(b'sdta',ck(b'smpl',pcm))+ls(b'pdta',pdta))
Path(sys.argv[1]).write_bytes(data)
print('P183_PUBLIC_EIGHT_DRUMKITS_P125_SOURCE',len(data))
