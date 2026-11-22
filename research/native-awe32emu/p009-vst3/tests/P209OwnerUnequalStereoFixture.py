#!/usr/bin/env python3
"""P209: only PUBLIC synthetic PCM; reproduces linked SF2 asymmetry
detected in four independently uploaded owner SoundFont banks.
No owner audio, ROM, metadata or preset files enter source or CI."""
import sys,math,struct
from pathlib import Path
from P199LinkedStereoPairs import ck,ls,ph,it,bg,g,sh
def make(folder,label,order,nL,nR):
    lengths=[22050,nL,nR]
    sounds=[]
    for k,n in enumerate(lengths):
        wave=[round(8500*(.64*math.sin(2*math.pi*i/(79+31*k))+
                           .36*math.sin(2*math.pi*i/(207-14*k))))
              for i in range(n)]
        sounds.append(struct.pack("<%dh"%n,*wave)+bytes(92))
    starts=[0]
    for n in lengths: starts.append(starts[-1]+n+46)
    smpl=b"".join(sounds)
    assert len(smpl)==2*starts[-1]
    headers=b"".join(
      sh(("MONO","LEFT","RIGHT")[i],starts[i],starts[i]+lengths[i],
         starts[i]+192,starts[i]+lengths[i]-192,
         (0,2,1)[i],(1,4,2)[i])
      for i in range(3))
    eos=starts[-1]
    headers+=sh("EOS",eos,eos,eos,eos,0,1)
    zones=[[g(34,-12000),g(37,0),g(38,1200),g(54,1),g(53,i)]
          for i in order]
    pdta=(ck(b"phdr",ph("P209",0,0,0)+ph("EOP",0,0,1))+
          ck(b"pbag",bg(0)+bg(1))+ck(b"pmod",bytes(10))+
          ck(b"pgen",g(41,0)+g(0,0))+
          ck(b"inst",it("P209",0)+it("EOI",len(zones)))+
          ck(b"ibag",b"".join(bg(k*5) for k in range(len(zones)+1)))+
          ck(b"imod",bytes(10))+
          ck(b"igen",b"".join(b"".join(z) for z in zones)+g(0,0))+
          ck(b"shdr",headers))
    body=b"sfbk"+ls(b"INFO",ck(b"ifil",struct.pack("<HH",2,1))+
         ck(b"INAM",b"P209 public stereo lengths\0"))+ls(b"sdta",ck(b"smpl",smpl))+ls(b"pdta",pdta)
    path=folder/("p209-"+label+".sf2")
    path.write_bytes(ck(b"RIFF",body))
    print("P209_PUBLIC_INDEPENDENT_STEREO_LENGTHS",label,tuple(lengths),path.stat().st_size)
def main():
    if len(sys.argv)!=2:raise SystemExit("P209 <public synthetic output folder>")
    d=Path(sys.argv[1]);d.mkdir(parents=True,exist_ok=True)
    make(d,"mono-first-unequal1",(0,1,2),22050,22051)
    make(d,"right-first-unequal18",(2,0,1),22050,22068)
if __name__=="__main__":main()
