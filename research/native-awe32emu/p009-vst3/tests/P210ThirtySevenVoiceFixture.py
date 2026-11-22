#!/usr/bin/env python3
"""Synthetic only: 31/32/33/37 simultaneously selected SF2 mono zones.
37 zones with 24 reused sample IDs model a real private 37-layer boundary,
but no owner's names, sample bytes, settings or metadata are distributed.
"""
import math
import struct
import sys
from pathlib import Path

def chunk(tag, payload):
    return tag+struct.pack('<I', len(payload))+payload+(b'\0' if len(payload)&1 else b'')
def list_chunk(tag,payload): return chunk(b'LIST',tag+payload)
def name(s): return s.encode('ascii')[:20].ljust(20,b'\0')
def ph(s,p,b,i): return struct.pack('<20sHHHIII',name(s),p,b,i,0,0,0)
def it(s,i): return struct.pack('<20sH',name(s),i)
def bg(i): return struct.pack('<HH',i,0)
def gen(op,v): return struct.pack('<HH',op,v&0xFFFF)
def sh(s,st,en,lo,hi):
    return struct.pack('<20sIIIIIBbHH',name(s),st,en,lo,hi,44100,60,0,0,1)

def make(folder,count):
    num_samples=24
    size=8192
    guard=46
    samples=[]
    for s in range(num_samples):
        wave=[round(3200*(.67*math.sin(2*math.pi*i/(97+2*s))+
                          .33*math.sin(2*math.pi*i/(251+3*s))))
              for i in range(size)]
        samples.append(struct.pack('<%dh'%size,*wave)+bytes(2*guard))
    smpl=b''.join(samples)
    span=size+guard
    headers=b''.join(sh('S%02d'%i,i*span,i*span+size,
                         i*span+128,i*span+size-128)
                     for i in range(num_samples))
    headers+=sh('EOS',num_samples*span,num_samples*span,
                num_samples*span,num_samples*span)
    zones=[]
    for i in range(count):
        # Key 84 / velocity 1..127. All zones are mono and no exclusive class.
        zones.append([gen(43,84|(84<<8)),gen(34,-12000),gen(37,0),
                      gen(38,1200),gen(54,1),gen(53,i%num_samples)])
    zone_width=6
    pdta=(chunk(b'phdr',ph('P210 Synthetic',0,0,0)+ph('EOP',0,0,1))+
          chunk(b'pbag',bg(0)+bg(1))+chunk(b'pmod',bytes(10))+
          chunk(b'pgen',gen(41,0)+gen(0,0))+
          chunk(b'inst',it('P210 Layers',0)+it('EOI',count))+
          chunk(b'ibag',b''.join(bg(k*zone_width) for k in range(count+1)))+
          chunk(b'imod',bytes(10))+
          chunk(b'igen',b''.join(b''.join(zone) for zone in zones)+gen(0,0))+
          chunk(b'shdr',headers))
    riff=(b'sfbk'+list_chunk(b'INFO',chunk(b'ifil',struct.pack('<HH',2,1))+
               chunk(b'INAM',b'P210 public synthetic 32 voice limit\0'))+
               list_chunk(b'sdta',chunk(b'smpl',smpl))+list_chunk(b'pdta',pdta))
    f=folder/('p210-%02d-layers.sf2'%count)
    f.write_bytes(chunk(b'RIFF',riff))
    print('P210_PUBLIC_SYNTHETIC_SF2',count,f.stat().st_size)

def main():
    if len(sys.argv)!=2: raise SystemExit('usage: fixture.py <output-dir>')
    folder=Path(sys.argv[1]);folder.mkdir(parents=True,exist_ok=True)
    for n in (31,32,33,37):make(folder,n)
if __name__=='__main__':main()
