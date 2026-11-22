#!/usr/bin/env python3
"""P195: two PUBLIC SF2 pairs that MUST yield the same original Creative voice.

Pair 1: coarseTune 3 in instrument +1 in preset VS 4 in instrument.
Pair 2: initialAttenuation 100 cB instrument +50 cB preset VS 150 cB
        instrument. Raw 16-bit sample, mapping, loop and all other params
        are identical. An erroneous SF1-style FillFrom produces differing
        pitch/level, whereas the original Creative VXD SF2 merge adds them.
"""
import math
import struct
import sys
from pathlib import Path

def ck(tag,data):
    return tag+struct.pack("<I",len(data))+data+(b"\0" if len(data)&1 else b"")
def ls(tag,data):
    return ck(b"LIST",tag+data)
def nm(s):
    return s.encode("ascii")[:20].ljust(20,b"\0")
def phdr(name,preset,bank,bag):
    return struct.pack("<20sHHHIII",nm(name),preset,bank,bag,0,0,0)
def inst(name,bag):
    return struct.pack("<20sH",nm(name),bag)
def bag(g):
    return struct.pack("<HH",g,0)
def gen(op,v):
    return struct.pack("<HH",op,v&0xffff)
def shdr(name,start,end,ls,le):
    return struct.pack("<20sIIIIIBbHH",nm(name),start,end,ls,le,44100,60,0,0,1)

def main():
    if len(sys.argv)!=2:raise SystemExit("usage: P195PresetMergePair.py <out-dir>")
    dst=Path(sys.argv[1]);dst.mkdir(parents=True,exist_ok=True)
    n=44100
    smpl=struct.pack("<%dh"%n,*(
        int(9000*(.7*math.sin(2*math.pi*i/220)+
                   .3*math.sin(2*math.pi*i/73.3333333))) for i in range(n)))
    info=ck(b"ifil",struct.pack("<HH",2,4))+ck(b"INAM",b"P195 Creative SF2 Generator Sum\0")
    for kind in ("tune","fine","atten"):
        for variant in ("split","combined","contrast"):
            split=(variant=="split")
            contrast=(variant=="contrast")
            if kind=="tune":
                preset=[gen(51,1)] if split else []
                instrument=gen(51,3 if split else (-8 if contrast else 4))
            elif kind=="fine":
                preset=[gen(52,15)] if split else []
                instrument=gen(52,20 if split else (-70 if contrast else 35))
            else:
                preset=[gen(48,50)] if split else []
                instrument=gen(48,100 if split else (420 if contrast else 150))
            preset+= [gen(41,0)]
            # sampleID terminates instrument zone; sampleModes precedes it.
            zone=[gen(34,-12000),gen(37,0),instrument,gen(54,1),gen(53,0)]
            pdta=(
                ck(b"phdr",phdr("OriginalSF2Sum",0,0,0)+phdr("EOP",0,0,1))+
                ck(b"pbag",bag(0)+bag(len(preset)))+
                ck(b"pmod",bytes(10))+
                ck(b"pgen",b"".join(preset)+gen(0,0))+
                ck(b"inst",inst("Sine",0)+inst("EOI",1))+
                ck(b"ibag",bag(0)+bag(len(zone)))+
                ck(b"imod",bytes(10))+
                ck(b"igen",b"".join(zone)+gen(0,0))+
                ck(b"shdr",shdr("HarmonicSine",0,n,128,n-128)+
                         shdr("EOS",n,n,n,n)))
            data=ck(b"RIFF",b"sfbk"+ls(b"INFO",info)+
                    ls(b"sdta",ck(b"smpl",smpl))+ls(b"pdta",pdta))
            out=dst/("p195-%s-%s.sf2"%(kind,variant))
            out.write_bytes(data)
            print("P195_PUBLIC_GENERATED_SF2_ORIGINAL_MERGE_PAIR",out.name,len(data))
if __name__=="__main__":
    main()
