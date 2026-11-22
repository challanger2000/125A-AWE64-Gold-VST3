#!/usr/bin/env python3
"""P196 private-asset-free original Creative sample END offsets SF2 oracle.

All soundfonts share identical 44100 Hz 16-bit sample data, the same program,
loop flags and envelope. Their only differences are standard generators 1/12:
  omitted and both explicit zero MUST match;
  coarse -1 and fine -32768 MUST match (both -32768 sample words);
  fine -30000 and coarse -1 MUST differ audibly from omitted baseline.
The latter two are adversarial discriminators: the original AWE32Emu source
ignored both generators, so all five formerly produced identical PCM.
"""
from pathlib import Path
import math
import struct
import sys

def ck(tag,data):
    return tag+struct.pack("<I",len(data))+data+(b"\0" if len(data)&1 else b"")
def ls(tag,data):
    return ck(b"LIST",tag+data)
def nm(x):
    return x.encode("ascii")[:20].ljust(20,b"\0")
def phdr(x,program,bank,bag):
    return struct.pack("<20sHHHIII",nm(x),program,bank,bag,0,0,0)
def inst(x,bag):
    return struct.pack("<20sH",nm(x),bag)
def bag(n):
    return struct.pack("<HH",n,0)
def gen(op,n):
    return struct.pack("<HH",op,n&0xffff)
def shdr(x,start,end,lstart,lend):
    return struct.pack("<20sIIIIIBbHH",nm(x),start,end,lstart,lend,44100,60,0,0,1)
def main():
    if len(sys.argv)!=2:raise SystemExit("usage: P196CreativeEndOffsetPair.py <output-dir>")
    dest=Path(sys.argv[1]);dest.mkdir(parents=True,exist_ok=True)
    frames=44100
    # Unique, non-silent transient-sensitive source. Include the 46 guard
    # words mandated by SF2; soundfonts are independent of licensed assets.
    values=[]
    for i in range(frames):
        phase=2*math.pi*i/197
        values.append(round(10000*(0.67*math.sin(phase)+
                                    0.33*math.sin(3*phase))))
    smpl=struct.pack("<%dh"%frames,*values)+bytes(46*2)
    variants={
        # preset-zone generators, instrument-zone generators
        "omitted":([],[]),
        "explicit-zero":([],[gen(1,0),gen(12,0)]),
        "fine-minus30000":([],[gen(1,-30000)]),
        "fine-split":([gen(1,-20000)],[gen(1,-10000)]),
        "coarse-minus1":([],[gen(12,-1)]),
        "coarse-split":([gen(12,-1)],[gen(12,0)]),
        "fine-minus32768":([],[gen(1,-32768)]),
    }
    for ident,(preset_offsets,offsets) in variants.items():
        # SampleModes=0: one-shot. 8-second release ensures the sample's
        # adjusted end is reached while the MIDI voice is still active.
        region=offsets+[gen(34,-12000),gen(37,0),gen(38,3600),gen(54,0),gen(53,0)]
        pdta=(
            ck(b"phdr",phdr("P196 EndOffset",0,0,0)+phdr("EOP",0,0,1))+
            ck(b"pbag",bag(0)+bag(len(preset_offsets)+1))+
            ck(b"pmod",bytes(10))+
            ck(b"pgen",b"".join(preset_offsets)+gen(41,0)+gen(0,0))+
            ck(b"inst",inst("EndOffset",0)+inst("EOI",1))+
            ck(b"ibag",bag(0)+bag(len(region)))+
            ck(b"imod",bytes(10))+
            ck(b"igen",b"".join(region)+gen(0,0))+
            ck(b"shdr",shdr("Harmonic",0,frames,100,frames-128)+
                shdr("EOS",frames,frames,frames,frames)))
        bank=ck(b"RIFF",b"sfbk"+ls(b"INFO",
                 ck(b"ifil",struct.pack("<HH",2,4))+
                 ck(b"INAM",b"P196 Public Creative End Offset\0"))+
                 ls(b"sdta",ck(b"smpl",smpl))+ls(b"pdta",pdta))
        out=dest/("p196-"+ident+".sf2")
        out.write_bytes(bank)
        print("P196_PUBLIC_CREATIVE_END_OFFSET",ident,len(bank))
if __name__=="__main__":
    main()
