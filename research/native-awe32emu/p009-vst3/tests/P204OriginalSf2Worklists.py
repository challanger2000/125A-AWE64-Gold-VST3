#!/usr/bin/env python3
"""Original-independent 3-mono and mono-first/linked-stereo SF2 banks."""
import math,struct,sys
from pathlib import Path
from P199LinkedStereoPairs import ck,ls,ph,it,bg,g,sh
def make(d,label,order,linked):
    n=22050
    sounds=[
      [round(8600*(.66*math.sin(2*math.pi*i/79)+.34*math.sin(2*math.pi*i/211))) for i in range(n)],
      [round(10300*(.65*math.sin(2*math.pi*i/143)+.35*math.sin(2*math.pi*i/57))) for i in range(n)],
      [round(11800*(.53*math.sin(2*math.pi*i/191)+.47*math.sin(2*math.pi*i/83))) for i in range(n)]]
    smpl=b"".join(struct.pack("<%dh"%n,*a)+bytes(92) for a in sounds)
    span=n+46
    hdr=b"".join(sh(("MONO","LEFT","RIGHT")[i],span*i,span*i+n,
        span*i+192,span*i+n-192,
        2 if linked and i==1 else 1 if linked and i==2 else 0,
        4 if linked and i==1 else 2 if linked and i==2 else 1)
        for i in range(3))
    hdr+=sh("EOS",3*span,3*span,3*span,3*span,0,1)
    zones=[[g(34,-12000),g(37,0),g(38,1200),g(54,1),g(53,i)]
           for i in order]
    pdta=(ck(b"phdr",ph("P204",0,0,0)+ph("EOP",0,0,1))+
          ck(b"pbag",bg(0)+bg(1))+ck(b"pmod",bytes(10))+
          ck(b"pgen",g(41,0)+g(0,0))+
          ck(b"inst",it("P204",0)+it("EOI",len(zones)))+
          ck(b"ibag",b"".join(bg(i*5) for i in range(len(zones)+1)))+
          ck(b"imod",bytes(10))+
          ck(b"igen",b"".join(b"".join(z) for z in zones)+g(0,0))+
          ck(b"shdr",hdr))
    data=ck(b"RIFF",b"sfbk"+
       ls(b"INFO",ck(b"ifil",struct.pack("<HH",2,1))+
          ck(b"INAM",b"P204 public synthetic\0"))+
       ls(b"sdta",ck(b"smpl",smpl))+ls(b"pdta",pdta))
    (d/("p204-"+label+".sf2")).write_bytes(data)
    print("P204_PUBLIC_SF2",label,len(data))
if __name__=="__main__":
    if len(sys.argv)!=2:raise SystemExit("fixture.py <output-dir>")
    out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
    make(out,"three-mono",(0,1,2),False)
    make(out,"mixed-mono-first",(0,2,1),True)
    make(out,"mixed-right-first",(2,0,1),True)
