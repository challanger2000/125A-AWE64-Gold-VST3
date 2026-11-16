#!/usr/bin/env python3
"""Read-only, SHA-pinned Creative LE/V4 cross-object I/O caller auditor.

This validates observed I/O-time E8 caller evidence, NOT driver/PCM parity.
Private source VXD, CPU/chip traces, SF2, ROM and WAV are read locally;
output stores only source hashes, verified counters and original offsets.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
from hashlib import sha256
import json
from pathlib import Path
import struct
import zipfile

VXD_SHA = "d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a"
ANCHORS = {0x3c5e:"out_byte",0x3c76:"out_byte",0x3c92:"in_byte",0x3caa:"in_byte",
           0x3cc6:"out_word",0x3cdf:"out_word",0x3cfb:"in_word",0x3d14:"in_word",
           0x3d2d:"out_dword",0x3d3e:"out_dword",0x3d5a:"in_dword",0x3d68:"in_dword"}
ANCHOR_OPCODES = {0x3c5e:0xee,0x3c76:0xee,0x3c92:0xee,0x3caa:0xec,
                  0x3cc6:0xee,0x3cdf:0xef,0x3cfb:0xee,0x3d14:0xed,
                  0x3d2d:0xee,0x3d3e:0xef,0x3d5a:0xee,0x3d68:0xed}
HELPERS = {"out_byte":0x3c49,"in_byte":0x3c7d,"out_word":0x3cb1,
           "in_word":0x3ce6,"out_dword":0x3d1b,"in_dword":0x3d44}
UNKNOWN = 0xffffffff

def decode_le(vxd:bytes):
    if sha256(vxd).hexdigest() != VXD_SHA:
        raise ValueError("Unrecognized original Creative VXD SHA256")
    le=struct.unpack_from("<I",vxd,0x3c)[0]
    if vxd[le:le+2] != b"LE": raise ValueError("Not an LE VXD")
    u=lambda off: struct.unpack_from("<I",vxd,le+off)[0]
    objects={}; names={}; mapped=le+u(0x48); data=u(0x80)
    objtab=le+u(0x40); sizepage=u(0x28); pages={}
    for oid in range(1,u(0x44)+1):
        size,_,_,start,npages,name=struct.unpack_from("<IIIIII",vxd,objtab+(oid-1)*24)
        names[oid]=struct.pack("<I",name).decode("ascii")
        blob=bytearray()
        for p in range(npages):
            page=start+p;ix=mapped+4*(page-1)
            source=int.from_bytes(vxd[ix:ix+3],"big")
            if vxd[ix+3] != 0: raise ValueError("Unsupported non-direct LE page")
            blob.extend(vxd[data+sizepage*(source-1):data+sizepage*source] if source else bytes(sizepage))
            pages[page]=(oid,p)
        objects[oid]=bytes(blob[:size])
    if names != {1:"LCOD",2:"MCOD",3:"PNPC",4:"PCOD",5:"PDAT"}:
        raise ValueError("Unexpected original Creative code objects")
    ptable=le+u(0x68);rtable=le+u(0x6c)
    offsets=[struct.unpack_from("<I",vxd,ptable+i*4)[0] for i in range(max(pages)+1)]
    if offsets!=sorted(offsets) or rtable+offsets[-1]>len(vxd):
        raise ValueError("Invalid LE fixup offsets")
    relocs={};count=0
    for page,(oid,pindex) in pages.items():
        at=rtable+offsets[page-1];stop=rtable+offsets[page]
        while at<stop:
            kind,flag=vxd[at:at+2]
            if flag: raise ValueError("Unsupported LE relocation flags")
            if kind in (0x07,0x08):
                src=[struct.unpack_from("<h",vxd,at+2)[0]]
                target_oid=vxd[at+4];target=struct.unpack_from("<H",vxd,at+5)[0];at+=7
            elif kind in (0x27,0x28):
                n=vxd[at+2];target_oid=vxd[at+3]
                target=struct.unpack_from("<H",vxd,at+4)[0]
                src=list(struct.unpack_from("<"+"h"*n,vxd,at+6));at+=6+n*2
            else:raise ValueError("Unsupported LE relocation kind")
            for pos in src:
                count+=1;abs_off=pindex*sizepage+pos
                if (kind&0x0f)==8 and abs_off>0 and abs_off+4<=len(objects[oid]):
                    call=abs_off-1
                    if objects[oid][call]==0xe8:
                        relocs[(oid,call)]=(target_oid,target)
        if at!=stop:raise ValueError("LE fixup page overrun")
    return objects,names,relocs,count

def load_base(eips:Counter)->tuple[int,int]:
    candidates=Counter()
    for pc,count in eips.items():
        for off in ANCHORS:candidates[(pc-off-1)&0xffffffff]+=count
    if len(candidates)<2:raise ValueError("Insufficient original CPU I/O anchors")
    (base,matched),(_,runner)=candidates.most_common(2)
    if matched*100<eips.total()*98 or runner*100>=matched*95:
        raise ValueError("LCOD base ambiguous")
    return base,matched

def external_sites(objects:dict,relocs:dict)->dict:
    found=defaultdict(list)
    for (obj,offset),target in relocs.items():
        if obj not in (3,4) or offset+5>len(objects[obj]):continue
        for helper,entry in HELPERS.items():
            if target==(1,entry):found[helper].append((obj,offset))
    return dict(found)

def external_bases(return_helpers:Counter,sites:dict)->dict:
    out={}
    for obj in (3,4):
        candidates=defaultdict(set)
        for (ret,helper),count in return_helpers.items():
            if ret==UNKNOWN:continue
            for src_obj,site in sites.get(helper,()):
                if src_obj==obj:candidates[(ret-site-5)&0xffffffff].add(site)
        best=sorted(((len(v),base) for base,v in candidates.items()),reverse=True)
        if not best or best[0][0]<20 or (len(best)>1 and best[0][0]<=best[1][0]*2):
            raise ValueError("Original LE module base ambiguous for "+str(obj))
        out[obj]=best[0][1]
    return out

def direct_call(objects:dict,relocs:dict,bases:dict,ret:int,helper:str):
    if ret==UNKNOWN:return None
    for obj,base in bases.items():
        end=(ret-base)&0xffffffff;call=end-5;code=objects[obj]
        if call<0 or call+5>len(code) or code[call]!=0xe8:continue
        if obj==1:dst=(1,end+struct.unpack_from("<i",code,call+1)[0])
        else:dst=relocs.get((obj,call))
        if dst==(1,HELPERS[helper]):return obj,call
    return None

def parse_cpu(line:bytes):
    if not line.strip() or line.startswith(b"#"):return None
    row=line.split()
    if len(row)!=17 or row[0] not in (b"W",b"R",b"r"):
        raise ValueError("Invalid V4 CPU trace row")
    return row[0],int(row[1]),int(row[4],16),int(row[5],16),int(row[6],16),int(row[7]),int(row[9],16)

def compare_chip(cpu,chip:bytes)->bool:
    r,_,_,p,v,_,_=cpu
    row=chip.split();is_read=row[:1]==[b"R"]
    if is_read:row=row[1:]
    if len(row)!=3:return False
    return p==int(row[1],16) and is_read==(r in (b"R",b"r")) and (r==b"r" or v==int(row[2],16))

def audit(vxd_path:Path,result_path:Path):
    vxd=vxd_path.read_bytes();objects,names,relocs,fixup_count=decode_le(vxd)
    if any(objects[1][off]!=opcode for off,opcode in ANCHOR_OPCODES.items()):
        raise ValueError("Original I/O opcode anchor mismatch")
    with zipfile.ZipFile(result_path) as z:
        required={"Original-CPU-IO.trace","Original-Register.trace","P003-BERICHT.json"}
        if z.testzip() or not required.issubset(z.namelist()):
            raise ValueError("Private V4 archive invalid")
        meta=json.loads(z.read("P003-BERICHT.json"))
        if not meta.get("cpu_chip_event_join",{}).get("guest_return_stack_v4_enabled"):
            raise ValueError("Original guest V4 stack not present")
        eips=Counter();pairs=Counter();events=0;tsc_prev=None
        with z.open("Original-CPU-IO.trace") as cpu,z.open("Original-Register.trace") as chip:
            if b"V4_STACK" not in cpu.readline():
                raise ValueError("Not the original V4 CPU trace")
            chips=(row for row in chip if row.strip() and not row.startswith(b"#"))
            for raw in cpu:
                row=parse_cpu(raw)
                if row is None:continue
                c=next(chips,None)
                if c is None or not compare_chip(row,c):
                    raise ValueError(f"Original physical CPU/EMU8000 mismatch at {events}")
                _,tsc,pc,_,_,seq,ret=row
                if seq!=events or (tsc_prev is not None and tsc<tsc_prev):
                    raise ValueError("CPU sequence/TSC mismatch")
                events+=1;tsc_prev=tsc;eips[pc]+=1;pairs[(pc,ret)]+=1
            if next(chips,None) is not None:
                raise ValueError("Unmatched EMU8000 chip events")
        if meta.get("cpu_chip_event_join",{}).get("matched_port_events")!=events:
            raise ValueError("Original source attestation event count mismatch")
        base,matched=load_base(eips);by_return=Counter();unmapped=Counter()
        for (pc,ret),n in pairs.items():
            helper=ANCHORS.get((pc-base-1)&0xffffffff)
            if helper is None:unmapped[(pc,ret)]+=n
            else:by_return[(ret,helper)]+=n
        bases={1:base,**external_bases(by_return,external_sites(objects,relocs))}
        verified=Counter();nonverified=0
        for (ret,helper),n in by_return.items():
            found=direct_call(objects,relocs,bases,ret,helper)
            if found is None:nonverified+=n
            else:verified[(found[0],found[1],helper)]+=n
        if nonverified:
            raise ValueError("Original V4 return E8 or relocations did not match")
        calls=sum(verified.values());unknown_events=sum(unmapped.values())
        if calls!=matched or calls+unknown_events!=events:
            raise ValueError("Original V4 caller count inconsistent")
        by_obj=Counter()
        for (obj,site,helper),n in verified.items():by_obj[names[obj]]+=n
        return {"schema":"P003-REA-THREE-OBJECT-V4-REPORT-V2",
                "original_vxd_sha256":VXD_SHA,
                "original_capture_sha256":sha256(result_path.read_bytes()).hexdigest(),
                "original_port_events":events,"direct_call_port_snapshots":calls,
                "unknown_io_program_counters":unknown_events,
                "distinct_verified_call_sites":len(verified),
                "runtime_bases":{names[obj]:f"0x{base:08X}" for obj,base in bases.items()},
                "direct_call_port_snapshots_by_object":dict(by_obj),
                "original_le_relocations":len(relocs),"expanded_fixup_sites":fixup_count,
                "all_cpu_chip_ports_sequence_tsc_checked":True,
                "autonomous_vst3_register_pcm_parity":"NOT_PROVEN"}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original-vxd",required=True,type=Path)
    p.add_argument("--original-v4-zip",required=True,type=Path)
    p.add_argument("--output",required=True,type=Path)
    a=p.parse_args()
    if a.output.exists():raise ValueError("Will not overwrite existing original evidence")
    report=audit(a.original_vxd,a.original_v4_zip)
    a.output.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n",encoding="utf8")
    print("P003_REA_THREE_OBJECTS_PASS",report["direct_call_port_snapshots"],"/",report["original_port_events"])

if __name__=="__main__":main()
