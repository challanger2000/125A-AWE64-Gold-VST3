#!/usr/bin/env python3
"""P009 original PUBLIC synthetic two-voice SF2 generator; NO Creative assets."""
import math
import struct
import sys
from pathlib import Path

def ck(tag,data):
    assert len(tag)==4
    return tag+struct.pack("<I",len(data))+data+(b"\0" if len(data)%2 else b"")
def ls(tag,data):
    return ck(b"LIST",tag+data)
def nm(s):
    return s.encode("ascii")[:20].ljust(20,b"\0")
def phdr(name,preset,bank,bag):
    return struct.pack("<20sHHHIII",nm(name),preset,bank,bag,0,0,0)
def inst(name,bag):
    return struct.pack("<20sH",nm(name),bag)
def bag(gen):
    return struct.pack("<HH",gen,0)
def gen(op,val):
    return struct.pack("<HH",op,val)
def shdr(name,start,end,loop0,loop1):
    return struct.pack("<20sIIIIIBbHH",nm(name),start,end,loop0,loop1,
                       44100,60,0,0,1)
alternate=len(sys.argv)>2 and sys.argv[2]=="--alternate"
samples=[]
for kind in range(2):
    for i in range(768):
        phase=2*math.pi*(i%256)/256
        v=math.sin(phase) if kind==0 else math.sin(phase)*math.cos(phase)
        # Identical RIFF bank/preset structure with genuinely different PCM
        # so a wrong-bank fallback MUST change the original chip's audio.
        samples.append(int((3400 if alternate else 11000)*v))
smpl=struct.pack("<%dh"%len(samples),*samples)
# Both instrument zones are active simultaneously, MIDI velocity 1..127.
pdta=(
    ck(b"phdr",phdr("PublicTwoLayers",0,0,0)+phdr("EOP",0,0,1))+
    ck(b"pbag",bag(0)+bag(1))+
    ck(b"pmod",bytes(10))+
    ck(b"pgen",gen(41,0)+gen(0,0))+
    ck(b"inst",inst("TwoLayerInstrument",0)+inst("EOI",2))+
    ck(b"ibag",bag(0)+bag(1)+bag(2))+
    ck(b"imod",bytes(10))+
    ck(b"igen",gen(53,0)+gen(53,1)+gen(0,0))+
    ck(b"shdr",shdr("SineWave",0,768,64,640)+
                shdr("HarmonicWave",768,1536,832,1408)+
                shdr("EOS",1536,1536,1536,1536))
)
info=ck(b"ifil",struct.pack("<HH",2,1))+ck(b"INAM",b"P009 Public Two Layer QA\0")
bank=ls(b"INFO",info)+ls(b"sdta",ck(b"smpl",smpl))+ls(b"pdta",pdta)
data=ck(b"RIFF",b"sfbk"+bank)
out=Path(sys.argv[1])
out.write_bytes(data)
print("P009_PUBLIC_GENERATED_TWO_LAYER_SF2",len(data))
