#!/usr/bin/env python3
"""P002 read-only original Creative 1MGM/SYNTHGM.SBK ROM bank descriptor oracle.

Uses only standard-library RIFF parsing; does not load or synthesize samples,
does not modify the chip, and does NOT pretend full SF2/SBK P003 support.
It enforces the original Creative 48-byte shdr stride (not generic SF2 46).
The owner-supplied original SBK remains PRIVATE, never committed.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

OWNER_SYNTHGM_SHA="fd2e35a260fef22fe809918dda3c921c6d48d53a29c3515af8a41019ce72d586"
STRIDES={b"phdr":38,b"pbag":4,b"pgen":4,b"inst":22,b"ibag":4,b"igen":4,b"shdr":48}

def chunks(data: bytes,start:int,end:int):
    index=start
    while index+8<=end:
        name=data[index:index+4]
        length=struct.unpack_from("<I",data,index+4)[0]
        begin=index+8
        stop=begin+length
        if stop>end:raise ValueError("Creative RIFF chunk overruns bank")
        yield name,data[begin:stop]
        index=stop+(length&1)
    if index!=end:raise ValueError("Creative RIFF padding or trailing bytes invalid")

def descriptor(raw:bytes,program:int,bank:int,key:int):
    if not (0<=program<=127 and 0<=bank<=0xffff and 0<=key<=127):
        raise ValueError("invalid original MIDI program, bank or note key")
    if (len(raw)<12 or raw[:4]!=b"RIFF" or raw[8:12]!=b"sfbk" or
        struct.unpack_from("<I",raw,4)[0]!=len(raw)-8):
        raise ValueError("invalid original Creative 1MGM RIFF bank")
    pdta=None
    info=None
    for tag,payload in chunks(raw,12,len(raw)):
        if tag==b"LIST" and payload[:4]==b"pdta":
            if pdta is not None:raise ValueError("duplicate original pdta section")
            pdta=dict(chunks(payload,4,len(payload)))
        if tag==b"LIST" and payload[:4]==b"INFO":
            if info is not None:raise ValueError("duplicate original INFO section")
            info=dict(chunks(payload,4,len(payload)))
    if not info or not pdta:
        raise ValueError("original bank INFO/pdta missing")
    if (b"EMU8000" not in info.get(b"isng",b"") or
        b"SBAWE32" not in info.get(b"IPRD",b"")):
        raise ValueError("not original Creative EMU8000/SBAWE32 descriptor")
    for tag,stride in STRIDES.items():
        if not pdta.get(tag) or len(pdta[tag])%stride:
            raise ValueError(f"invalid original Creative 1MGM {tag!r} record stride")
    def count(tag):return len(pdta[tag])//STRIDES[tag]
    def u16(tag,idx,offset):
        return struct.unpack_from("<H",pdta[tag],idx*STRIDES[tag]+offset)[0]
    def zones(gen_name,bag_name,start,end):
        for bag in range(start,end):
            low=u16(bag_name,bag,0)
            high=u16(bag_name,bag+1,0)
            if low>high or high>count(gen_name):
                raise ValueError("original generator indexes invalid")
            yield bag,dict((u16(gen_name,i,0),u16(gen_name,i,2))
                           for i in range(low,high))
    matches=[]
    for pi in range(count(b"phdr")-1):
        if (u16(b"phdr",pi,20),u16(b"phdr",pi,22))!=(program,bank):
            continue
        pbeg=u16(b"phdr",pi,24)
        pend=u16(b"phdr",pi+1,24)
        if pbeg>=pend or pend>=count(b"pbag"):
            raise ValueError("invalid original preset bag boundaries")
        for _,pgen in zones(b"pgen",b"pbag",pbeg,pend):
            inst=pgen.get(41)
            if inst is None:continue
            if inst+1>=count(b"inst"):
                raise ValueError("original Creative instrument index out of range")
            begin=u16(b"inst",inst,20)
            end=u16(b"inst",inst+1,20)
            if begin>=end or end>=count(b"ibag"):
                raise ValueError("invalid original instrument zone boundaries")
            bagzones=list(zones(b"igen",b"ibag",begin,end))
            global_gen={}
            for _,g in bagzones:
                if 53 not in g:
                    if 43 in g:
                        raise ValueError("unsupported scoped global key range, fail closed")
                    global_gen.update(g)
            for bag,g in bagzones:
                if 53 not in g:continue
                lowhigh=g.get(43,0x7F00)
                low,high=lowhigh&255,lowhigh>>8
                if low>high or high>127:
                    raise ValueError("invalid Creative key-range generator")
                if low<=key<=high:
                    raw_cutoff=g.get(8,global_gen.get(8))
                    if raw_cutoff is None or raw_cutoff>127:
                        raise ValueError("unsupported Creative raw cutoff encoding")
                    inst_offset=inst*22
                    name=pdta[b"inst"][inst_offset:inst_offset+20].split(b"\x00")[0].decode("latin1")
                    matches.append({
                        "program":program,"bank":bank,"key":key,
                        "instrument_index":inst,"instrument_name":name,
                        "instrument_zone_bag":bag,"key_range":[low,high],
                        "global_raw_cutoff":global_gen.get(8),
                        "zone_raw_cutoff":g.get(8),
                        "effective_raw_cutoff":raw_cutoff,
                        # OBSERVED candidate conversion, NOT universal GM
                        "original_measured_chip_cutoff_candidate":raw_cutoff*2,
                        "original_preset_attenuation_raw":pgen.get(48),
                        "original_attack_vol_env_raw":global_gen.get(34),
                        "sample_id_raw":g[53],
                    })
    if len(matches)!=1:
        raise ValueError(f"unsupported original bank/zone key ambiguity ({len(matches)} matches)")
    return matches[0]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--sbk",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True)
    p.add_argument("--verify-original",action="store_true",
                   help="require exact private owner SYNTHGM.SBK SHA-256")
    a=p.parse_args()
    if a.out.exists():raise ValueError("refusing to overwrite private original descriptor report")
    data=a.sbk.read_bytes()
    digest=hashlib.sha256(data).hexdigest()
    if a.verify_original and digest!=OWNER_SYNTHGM_SHA:
        raise ValueError("authentic owner Creative SYNTHGM.SBK SHA-256 mismatch")
    piano=descriptor(data,0,0,60)
    violin=descriptor(data,40,0,60)
    if a.verify_original and not (
        piano["original_measured_chip_cutoff_candidate"]==178 and
        piano["zone_raw_cutoff"]==89 and
        piano["instrument_zone_bag"]==3 and
        violin["original_measured_chip_cutoff_candidate"]==210 and
        violin["global_raw_cutoff"]==105 and
        violin["instrument_zone_bag"]==219):
        raise ValueError("actual Creative 1MGM original key/zone field identities differ")
    result={"status":"CREATIVE_SBK_ROM_DESCRIPTOR_KEY_ZONE_PASS",
        "sbk_sha256":digest,"creative_special_shdr_stride":48,
        "original_pcm_register_compatibility":"ONLY NOTE60 CUTOFF BASE PARAMETER CROSS-CHECKED",
        "gm0_piano_note60":piano,"gm40_violin_note60":violin,
        "native_filter_bank_parameter_source":"PROVEN FOR THESE TWO ORIGINAL KEY ZONES",
        "complete_SBK_SF2_voice_translation":"NOT IMPLEMENTED",
        "private_data_in_json":False}
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print("P002_CREATIVE_ROM_SBK_RAW_KEY_ZONE_CUTOFF_PASS",
          "GM0_key60=89_to_IFATN178","GM40_key60=105_to_IFATN210",
          "full_bank_native_P003=NOT_CLAIMED")
if __name__=="__main__":
    try:main()
    except (OSError,ValueError,UnicodeError) as e:
        print("P002_CREATIVE_ROM_SBK_ORACLE_NO_GO",e,file=sys.stderr)
        sys.exit(1)
