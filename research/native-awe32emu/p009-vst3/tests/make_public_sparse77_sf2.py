#!/usr/bin/env python3
"""P180 public synthetic SF2: 77 sparse melodic presets, MIDI program 63 absent.
No user/Creative sample, name or metadata is copied.
"""
import math
import struct
import sys
from pathlib import Path

def chunk(tag, data):
    return tag + struct.pack('<I', len(data)) + data + (b'\0' if len(data)&1 else b'')
def sub(tag, data):
    return chunk(b'LIST', tag+data)
def label(s):
    return s.encode('ascii')[:20].ljust(20,b'\0')
def phdr(name,program,bank,bag):
    return struct.pack('<20sHHHIII',label(name),program,bank,bag,0,0,0)
def inst(name,bag):
    return struct.pack('<20sH',label(name),bag)
def bag(n):
    return struct.pack('<HH',n,0)
def gen(kind,value):
    return struct.pack('<HH',kind,value&0xffff)
def shdr(name,a,b,ls,le):
    return struct.pack('<20sIIIIIBbHH',label(name),a,b,ls,le,44100,60,0,0,1)

programs=[p for p in range(78) if p != 63]
assert len(programs)==77
# One original-chip sample; 77 distinct sound engines/patch attenuations.
count=44100
samples=[round(8500*math.sin(2*math.pi*241*i/44100)) for i in range(count)]
smpl=struct.pack('<%dh'%count,*samples)
presets=b''.join(phdr('Public Program %03d'%p,p,0,i) for i,p in enumerate(programs))+phdr('EOP',0,0,len(programs))
pbags=b''.join(bag(i) for i in range(len(programs)+1))
pgens=b''.join(gen(41,i) for i in range(len(programs)))+gen(0,0)
instruments=b''.join(inst('Public Inst %03d'%p,i) for i,p in enumerate(programs))+inst('EOI',len(programs))
ibags=b''.join(bag(4*i) for i in range(len(programs)+1))
igens=b''.join(gen(37,0)+gen(48,i*8)+gen(53,0)+gen(54,1) for i in range(len(programs)))+gen(0,0)
headers=shdr('Public Sine',0,count,64,count-128)+shdr('EOS',count,count,count,count)
pdta=b''.join([
  chunk(b'phdr',presets),chunk(b'pbag',pbags),chunk(b'pmod',bytes(10)),
  chunk(b'pgen',pgens),chunk(b'inst',instruments),chunk(b'ibag',ibags),
  chunk(b'imod',bytes(10)),chunk(b'igen',igens),chunk(b'shdr',headers)
])
info=chunk(b'ifil',struct.pack('<HH',2,1))+chunk(b'INAM',b'Public 77 with one gap\0')
output=chunk(b'RIFF',b'sfbk'+sub(b'INFO',info)+sub(b'sdta',chunk(b'smpl',smpl))+sub(b'pdta',pdta))
path=Path(sys.argv[1]);path.write_bytes(output)
print('P180_PUBLIC_SPARSE_77_SF2_PRESETS=77', len(output))
