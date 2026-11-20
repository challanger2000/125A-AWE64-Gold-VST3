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
alternate="--alternate" in sys.argv[2:]
wet="--wet" in sys.argv[2:]
sustain="--sustain" in sys.argv[2:]
# A short 768-frame one-shot ends after 17 ms even when a note is HELD.
# For voice-recovery QA, provide at least one full second of real PCM,
# so a buggy stuck-note path cannot pass through natural sample exhaustion.
sample_length=44100 if sustain else 768
samples=[]
for kind in range(2):
    for i in range(sample_length):
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
    ck(b"ibag",bag(0)+bag((3 if wet else 1)+(1 if sustain else 0))+
        bag(2*((3 if wet else 1)+(1 if sustain else 0))))+
    ck(b"imod",bytes(10))+
    ck(b"igen",(
        gen(15,1000)+gen(16,1000)+(gen(54,1) if sustain else b"")+gen(53,0)+
        gen(15,1000)+gen(16,1000)+(gen(54,1) if sustain else b"")+gen(53,1)+gen(0,0)
      ) if wet else ((gen(54,1) if sustain else b"")+gen(53,0)+
                      (gen(54,1) if sustain else b"")+gen(53,1)+gen(0,0)))+
    ck(b"shdr",shdr("SineWave",0,sample_length,64,sample_length-128)+
                shdr("HarmonicWave",sample_length,2*sample_length,
                     sample_length+64,2*sample_length-128)+
                shdr("EOS",2*sample_length,2*sample_length,
                     2*sample_length,2*sample_length))
)
info=ck(b"ifil",struct.pack("<HH",2,1))+ck(b"INAM",b"P009 Public Two Layer QA\0")
bank=ls(b"INFO",info)+ls(b"sdta",ck(b"smpl",smpl))+ls(b"pdta",pdta)
data=ck(b"RIFF",b"sfbk"+bank)
out=Path(sys.argv[1])
out.write_bytes(data)
print("P009_PUBLIC_GENERATED_TWO_LAYER_SF2",len(data))
