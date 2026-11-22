#!/usr/bin/env python3
"""P199 four causal SF2 banks: identical left PCM, change only right and link."""
import math,struct,sys
from pathlib import Path
def ck(t,b):return t+struct.pack("<I",len(b))+b+(b"\0" if len(b)&1 else b"")
def ls(t,b):return ck(b"LIST",t+b)
def nm(t):return t.encode("ascii")[:20].ljust(20,b"\0")
def ph(t,p,b,i):return struct.pack("<20sHHHIII",nm(t),p,b,i,0,0,0)
def it(t,i):return struct.pack("<20sH",nm(t),i)
def bg(n):return struct.pack("<HH",n,0)
def g(op,v):return struct.pack("<HH",op,v&65535)
def sh(t,sta,end,lo,hi,link,typ):
    return struct.pack("<20sIIIIIBbHH",nm(t),sta,end,lo,hi,44100,60,0,link,typ)
def build(folder,label,explicit=False,zero=False,mono=False):
    n=22050
    left=[round(12000*(.6*math.sin(2*math.pi*i/143)+.4*math.sin(2*math.pi*i/79))) for i in range(n)]
    right=[round(11000*(.55*math.sin(2*math.pi*i/191)+.45*math.sin(2*math.pi*i/61))) for i in range(n)]
    if zero:right=[0]*n
    offset=n+46
    eos=offset+n+46
    smpl=(struct.pack("<%dh"%n,*left)+bytes(92)
          +struct.pack("<%dh"%n,*right)+bytes(92))
    assert len(smpl)==2*eos
    headers=(sh("LEFT",0,n,192,n-192,0 if mono else 1,1 if mono else 4)
        +sh("RIGHT",offset,offset+n,offset+192,offset+n-192,0,1 if mono else 2)
        +sh("EOS",eos,eos,eos,eos,0,1))
    l=[g(34,-12000),g(37,0),g(38,1200),g(54,1),g(53,0)]
    r=[g(34,-12000),g(37,0),g(38,1200),g(54,1),g(53,1)]
    zones=[l,r] if explicit else [l]
    pdta=(ck(b"phdr",ph("P199 Stereo",0,0,0)+ph("EOP",0,0,1))
      +ck(b"pbag",bg(0)+bg(1))+ck(b"pmod",bytes(10))
      +ck(b"pgen",g(41,0)+g(0,0))
      +ck(b"inst",it("P199",0)+it("EOI",len(zones)))
      +ck(b"ibag",b"".join(bg(j*len(l)) for j in range(len(zones)+1)))
      +ck(b"imod",bytes(10))
      +ck(b"igen",b"".join(b"".join(z) for z in zones)+g(0,0))
      +ck(b"shdr",headers))
    data=ck(b"RIFF",b"sfbk"+ls(b"INFO",ck(b"ifil",struct.pack("<HH",2,1))
      +ck(b"INAM",b"P199 test\0"))+ls(b"sdta",ck(b"smpl",smpl))+ls(b"pdta",pdta))
    (folder/("p199-"+label+".sf2")).write_bytes(data)
    print("P199_LINKED_STEREO_INPUT",label,len(data))
def main():
    if len(sys.argv)!=2:raise SystemExit("usage: P199LinkedStereoPairs.py <output>")
    d=Path(sys.argv[1]);d.mkdir(parents=True,exist_ok=True)
    build(d,"linked-left")
    build(d,"explicit-both",explicit=True)
    build(d,"linked-silent-right",zero=True)
    build(d,"mono-left",mono=True)
if __name__=="__main__":main()
