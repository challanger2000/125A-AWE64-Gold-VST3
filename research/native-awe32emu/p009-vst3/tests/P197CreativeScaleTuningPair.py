#!/usr/bin/env python3
"""P197 original Creative SF2 scaleTuning public differential fixtures.

Original Win95 x86 code directly multiplies (key-rootKey) by generator56.
These otherwise-byte-equivalent SF2 files test scale=0,1,50,100 and absent.
At root MIDI 60 all must match. At note72, scale50 matches note66/scale100
(+600 cents); scale0 matches note60/scale100. A wrong SF1-only conversion
makes 50 indistinguishable from 100 and fails both positive/negative tests.
"""
from pathlib import Path
import math
import struct
import sys
def ck(tag,data):
    return tag+struct.pack("<I",len(data))+data+(b"\0" if len(data)&1 else b"")
def ls(tag,data):
    return ck(b"LIST",tag+data)
def nm(s):
    return s.encode("ascii")[:20].ljust(20,b"\0")
def phdr(s,p,b,idx):
    return struct.pack("<20sHHHIII",nm(s),p,b,idx,0,0,0)
def inst(s,idx):
    return struct.pack("<20sH",nm(s),idx)
def bag(idx):
    return struct.pack("<HH",idx,0)
def gen(op,value):
    return struct.pack("<HH",op,value&0xffff)
def shdr(s,start,end,ls,le):
    return struct.pack("<20sIIIIIBbHH",nm(s),start,end,ls,le,44100,60,0,0,1)
def main():
    if len(sys.argv)!=2:raise SystemExit("usage: P197CreativeScaleTuningPair.py <output-dir>")
    dest=Path(sys.argv[1]);dest.mkdir(parents=True,exist_ok=True)
    frames=44100
    # Nontrivial harmonic public audio, no proprietary user soundbanks.
    values=[int(9000*(.66*math.sin(2*math.pi*i/301)+
                       .34*math.sin(4*math.pi*i/301))) for i in range(frames)]
    smpl=struct.pack("<%dh"%frames,*values)+bytes(46*2)
    variants={"omitted":None,"default100":100,"half50":50,"flat0":0,"near1":1}
    for name,value in variants.items():
        layer=([gen(56,value)] if value is not None else [])+[
            gen(34,-12000),gen(37,0),gen(38,0),gen(54,1),gen(53,0)]
        pdta=(
            ck(b"phdr",phdr("ScaleTuning",0,0,0)+phdr("EOP",0,0,1))+
            ck(b"pbag",bag(0)+bag(1))+
            ck(b"pmod",bytes(10))+
            ck(b"pgen",gen(41,0)+gen(0,0))+
            ck(b"inst",inst("ScaleTuning",0)+inst("EOI",1))+
            ck(b"ibag",bag(0)+bag(len(layer)))+
            ck(b"imod",bytes(10))+
            ck(b"igen",b"".join(layer)+gen(0,0))+
            ck(b"shdr",shdr("Harmonic",0,frames,192,frames-192)+
                shdr("EOS",frames,frames,frames,frames)))
        sf=ck(b"RIFF",b"sfbk"+
              ls(b"INFO",ck(b"ifil",struct.pack("<HH",2,4))+
                 ck(b"INAM",b"P197 Public Original Creative ScaleTuning\0"))+
              ls(b"sdta",ck(b"smpl",smpl))+ls(b"pdta",pdta))
        file=dest/("p197-scale-"+name+".sf2")
        file.write_bytes(sf)
        print("P197_PUBLIC_CREATIVE_SF2_SCALE_TUNING",name,len(sf))
if __name__=="__main__":
    main()
