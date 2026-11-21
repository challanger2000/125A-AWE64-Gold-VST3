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
    return struct.pack("<HH",op,val & 0xffff)
def shdr(name,start,end,loop0,loop1):
    return struct.pack("<20sIIIIIBbHH",nm(name),start,end,loop0,loop1,
                       44100,60,0,0,1)
alternate="--alternate" in sys.argv[2:]
wet="--wet" in sys.argv[2:]
sustain="--sustain" in sys.argv[2:]
expressive="--expressive" in sys.argv[2:]
bank_number=128 if "--drums" in sys.argv[2:] else 0
# A specially authored, owner-independent two-zone soundfont containing
# genuine audible volume and modulation envelopes, filter routing/cutoff,
# resonance and long looping samples. This supplies an INDEPENDENT oracle
# for every one of the ten non-FX P165 hardware control gestures. Ordinary
# fixture modes remain byte-identical.
expression_ops=[
    (8,6000),    # original low-pass cutoff, cents
    (9,100),     # original resonance, cB
    (11,1200),   # real modulation envelope -> original hardware filter
    (26,0),      # modulation attack 1 s
    (28,0),      # modulation decay 1 s
    (29,250),    # modulation sustain attenuation
    (30,0),      # modulation release 1 s
    (34,0),      # volume attack 1 s
    (36,0),      # volume decay 1 s
    (37,250),    # volume sustain attenuation
    (38,0),      # volume release 1 s
    (54,1),      # original SF2 continuously looping voice
]
expression_count=len(expression_ops)+1
expression_zone=b"".join(gen(op,val) for op,val in expression_ops)

# A short 768-frame one-shot ends after 17 ms even when a note is HELD.
# Creative's authentic SF2 mapping also gives a *missing* sustainVolEnv
# generator zero register sustain (decay to silence). Explicit SF2 gen37=0
# is therefore essential: it maps to 0x7F full-volume sustain, and gen54=1
# instructs the original chip to loop. Both are needed for an honest test.
# For voice-recovery QA, provide at least one full second of real PCM,
# so a buggy stuck-note path cannot pass through natural sample exhaustion.
sample_length=44100 if (sustain or expressive) else 768
samples=[]
for kind in range(2):
    for i in range(sample_length):
        phase=2*math.pi*(i%256)/256
        v=math.sin(phase) if kind==0 else math.sin(phase)*math.cos(phase)
        # Identical RIFF bank/preset structure with genuinely different PCM
        # so a wrong-bank fallback MUST change the original chip's audio.
        samples.append(int((3400 if alternate else 11000)*v))
smpl=struct.pack("<%dh"%len(samples),*samples)
# P174 hardware-boundary fixture: preserve the SAME audible first 1536
# samples and real SoundFont headers, but add silent PCM words to fill the
# physical EMU8000 RAM to the exact calculated boundary. Unlike file-size
# heuristics, the original driver loader counts every PCM word and 8-word
# bank guard. This fixture also permits precisely ONE additional PCM word.
word_sizes=[a for a in sys.argv[2:] if a.startswith("--sample-words=")]
if len(word_sizes)>1:
    raise SystemExit("one --sample-words option allowed")
if word_sizes:
    wanted=int(word_sizes[0].split("=",1)[1])
    if wanted<len(samples) or wanted>28*1024*1024//2:
        raise SystemExit("invalid requested PCM word count")
    smpl+=bytes((wanted-len(samples))*2)
if "--oversized" in sys.argv[2:]:
    # Valid sample header, but the actual PCM exceeds hardware capacity.
    smpl+=bytes(28*1024*1024)
# Both instrument zones are active simultaneously, MIDI velocity 1..127.
pdta=(
    ck(b"phdr",phdr("PublicTwoLayers",0,bank_number,0)+phdr("EOP",0,0,1))+
    ck(b"pbag",bag(0)+bag(1))+
    ck(b"pmod",bytes(10))+
    ck(b"pgen",gen(41,0)+gen(0,0))+
    ck(b"inst",inst("TwoLayerInstrument",0)+inst("EOI",2))+
    ck(b"ibag",bag(0)+bag(expression_count if expressive else
                        ((3 if wet else 1)+(2 if sustain else 0)))+
        bag(2*(expression_count if expressive else
                        ((3 if wet else 1)+(2 if sustain else 0)))))+
    ck(b"imod",bytes(10))+
    ck(b"igen",(
        expression_zone+gen(53,0)+expression_zone+gen(53,1)+gen(0,0)
      ) if expressive else (
        gen(15,1000)+gen(16,1000)+
        (gen(37,0)+gen(54,1) if sustain else b"")+gen(53,0)+
        gen(15,1000)+gen(16,1000)+
        (gen(37,0)+gen(54,1) if sustain else b"")+gen(53,1)+gen(0,0)
      ) if wet else ((gen(37,0)+gen(54,1) if sustain else b"")+gen(53,0)+
                      (gen(37,0)+gen(54,1) if sustain else b"")+gen(53,1)+gen(0,0)))+
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
