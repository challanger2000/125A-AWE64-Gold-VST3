#!/usr/bin/env python3
"""P198 public nonlinear Creative original-SF2 filter registers A/B/contrast.

The original VXD independently converts instrument and preset values before
summing. Two different SF2 generator pairs target the same hardware register.
Their actual native VST3 PCM must match, and contrasting register values
must not match. No private user ROM or SoundFont.
"""
import math,struct,sys
from pathlib import Path
def ck(tag,body):
    return tag+struct.pack("<I",len(body))+body+(b"\0" if len(body)&1 else b"")
def ls(tag,body):return ck(b"LIST",tag+body)
def nm(s):return s.encode("ascii")[:20].ljust(20,b"\0")
def ph(s,p,b,i):return struct.pack("<20sHHHIII",nm(s),p,b,i,0,0,0)
def inst(s,i):return struct.pack("<20sH",nm(s),i)
def bag(n):return struct.pack("<HH",n,0)
def gen(op,value):return struct.pack("<HH",op,value&0xffff)
def shdr(s,start,end,ls,le):
    return struct.pack("<20sIIIIIBbHH",nm(s),start,end,ls,le,44100,60,0,0,1)
def fc(x):return int((x+15)/29)-153
def q(x):return int(x/12)
def main():
    if len(sys.argv)!=2:raise SystemExit("usage: P198CreativeFilterScopePairs.py <out-dir>")
    dest=Path(sys.argv[1]);dest.mkdir(parents=True,exist_ok=True)
    assert fc(11921)+(fc(-4604)-fc(0))==100
    assert fc(7322)==100 and fc(7317)==99
    assert q(80)+q(40)==9 and q(108)==9 and q(120)==10
    n=44100
    samples=[max(-32768,min(32767,round(
        12000*(0.45*(2*((i%211)/211)-1)+
               .35*math.sin(2*math.pi*i/211)+
               .20*math.sin(6*math.pi*i/211))
    ))) for i in range(n)]
    smpl=struct.pack("<%dh"%n,*samples)+bytes(46*2)
    scenarios={
        "fc-split":([gen(8,-4604)],[gen(8,11921),gen(9,72)]),
        "fc-equivalent":([],[gen(8,7322),gen(9,72)]),
        "fc-contrast":([],[gen(8,11921),gen(9,72)]),
        "q-split":([gen(9,40)],[gen(8,7000),gen(9,80)]),
        "q-equivalent":([],[gen(8,7000),gen(9,108)]),
        "q-contrast":([],[gen(8,7000),gen(9,0)]),
        "fc-default":([],[gen(9,72)]),
        "fc-default-explicit":([],[gen(8,13500),gen(9,72)]),
        "q-default":([],[gen(8,7000)]),
        "q-default-explicit":([],[gen(8,7000),gen(9,0)]),
    }
    for label,(preset,instrument) in scenarios.items():
        preset += [gen(41,0)]
        instrument += [gen(34,-12000),gen(37,0),gen(38,1200),
                       gen(54,1),gen(53,0)]
        pdta=(
            ck(b"phdr",ph("P198 Filter",0,0,0)+ph("EOP",0,0,1))+
            ck(b"pbag",bag(0)+bag(len(preset)))+
            ck(b"pmod",bytes(10))+
            ck(b"pgen",b"".join(preset)+gen(0,0))+
            ck(b"inst",inst("P198",0)+inst("EOI",1))+
            ck(b"ibag",bag(0)+bag(len(instrument)))+
            ck(b"imod",bytes(10))+
            ck(b"igen",b"".join(instrument)+gen(0,0))+
            ck(b"shdr",shdr("Rich",0,n,192,n-192)+shdr("EOS",n,n,n,n)))
        blob=ck(b"RIFF",b"sfbk"+
              ls(b"INFO",ck(b"ifil",struct.pack("<HH",2,4))+
                 ck(b"INAM",b"P198 Public Creative Filter\0"))+
              ls(b"sdta",ck(b"smpl",smpl))+ls(b"pdta",pdta))
        path=dest/("p198-"+label+".sf2")
        path.write_bytes(blob)
        print("P198_ORIGINAL_FILTER_FIXTURE",path.name,len(blob))
if __name__=="__main__":
    main()
