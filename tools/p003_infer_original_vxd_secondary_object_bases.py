#!/usr/bin/env python3
"""P003 private: infer non-LCOD original SBAWE.VXD code-object address
hypotheses from the *recorded* guest EBP return hints.

This is NOT proof of true x86 architectural return EIP. The original
86Box V4 guest-stack trace systematically gives an address pointing
to the LAST BYTE of an original E8 rel32 instruction rather than the
usual next-instruction EIP. The byte-offset cause is UNKNOWN. Do not
silently normalize it or report any live CALL as architecturally
validated. Independently infer original LE object 3/4 candidate bases
from many DISTINCT return hints; do not use prior session addresses,
MIDI keys, sound data, or a fixed inter-object gap.

All proprietary VXD bytes and original guest CPU trace remain PRIVATE.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import struct
import sys

from p003_join_original_guest_cpu_chip_clocks import guest_events, sha256
from p003_private_vxd_static_callers import original_lcod
from p003_verify_original_vxd_live_callers import choose_lcod_bias, UNKNOWN

def original_executable_objects(vxd:bytes)->dict[int,bytes]:
    # Pin and verify the exact original 1996 LE image FIRST, including
    # the known 12 original 32-bit I/O opcode positions in LCOD.
    original_lcod(vxd)
    le=struct.unpack_from("<I",vxd,0x3c)[0]
    def u32(o:int)->int: return struct.unpack_from("<I",vxd,le+o)[0]
    count=u32(0x44)
    if count!=5:
        raise ValueError("Unexpected original VXD LE object count")
    page_offset=le+u32(0x48)
    page_size=u32(0x28)
    datapages=u32(0x80)
    out={}
    for obj in (0,2,3):
        size,_,flags,start,num,_=struct.unpack_from(
            "<IIIIII",vxd,le+u32(0x40)+obj*24)
        if not flags&0x2000 or size<1024 or num<1 or num>256:
            raise ValueError("Unexpected original executable VXD object")
        buf=bytearray()
        for i in range(num):
            at=page_offset+4*(start+i-1)
            if at<0 or at+4>len(vxd):
                raise ValueError("LE page map truncated")
            page=int.from_bytes(vxd[at:at+3],"big")
            if page<1 or vxd[at+3]!=0:
                raise ValueError("Invalid original LE direct page")
            base=datapages+(page-1)*page_size
            if base+page_size>len(vxd):
                raise ValueError("LE code page truncated")
            buf.extend(vxd[base:base+page_size])
        out[obj]=bytes(buf[:size])
    if tuple(map(len,(out[0],out[2],out[3])))!=(31200,7640,23125):
        raise ValueError("Original LE object sizes changed")
    return out

def candidate_bases(code:bytes,return_counts:Counter)->list[dict]:
    # Compare a V4 [EBP+4] hint to the ORIGINAL five-byte E8
    # instruction's LAST BYTE, not the normal next-instruction EIP.
    # This is an explicitly UNVERIFIED observation in this probe.
    e8={i for i in range(len(code)-4) if code[i]==0xe8}
    matches=defaultdict(set)
    for ret in return_counts:
        for at in e8:
            matches[ret-(at+4)].add(ret)
    ranking=sorted(matches,
                   key=lambda base:(len(matches[base]),
                                    sum(return_counts[x] for x in matches[base])),
                   reverse=True)
    return [{"base":base,"distinct_return_hints":len(matches[base]),
             "event_count":sum(return_counts[x] for x in matches[base]),
             "matched_returns":matches[base]} for base in ranking[:5]]

def find_object_bases(objects:dict[int,bytes], eips:Counter,
                      returns:Counter)->dict:
    bias0,supported,runner=choose_lcod_bias(eips)
    original0=objects[0]
    outside=Counter({v:n for v,n in returns.items()
                     if v!=UNKNOWN and not bias0<=v<bias0+len(original0)})
    if len(outside)<20:
        raise ValueError("Not enough non-LCOD guest return evidence")
    a=candidate_bases(objects[2],outside)
    b=candidate_bases(objects[3],outside)
    if len(a)<2 or len(b)<2:
        raise ValueError("Insufficient unique load-bias candidates")
    first,second=a[0],b[0]
    if (first["distinct_return_hints"]<30 or
        second["distinct_return_hints"]<30 or
        first["distinct_return_hints"]<=2*a[1]["distinct_return_hints"] or
        second["distinct_return_hints"]<=2*b[1]["distinct_return_hints"]):
        raise ValueError("LCOD secondary object bases ambiguous")
    overlap=first["matched_returns"]&second["matched_returns"]
    if overlap:raise ValueError("Competing original VXD executable object overlap")
    union=first["matched_returns"]|second["matched_returns"]
    coverage=sum(outside[x] for x in union)
    all_outside=sum(outside.values())
    if coverage*100<99*all_outside:
        raise ValueError("Insufficient original guest return-object coverage")
    gap=second["base"]-(first["base"]+len(objects[2]))
    def concise(e:dict)->dict:
        return {k:e[k] for k in
                ("base","distinct_return_hints","event_count")}
    return {
        "schema":"125A-P003-PRIVATE-VXD-LE-OTHER-OBJECT-LOAD-BASE-HYPOTHESES-V1",
        "original_cpu_opcode_lcod_object1_base_hex":f"0x{bias0:08X}",
        "original_cpu_opcode_object1_events":supported,
        "original_cpu_opcode_object1_second_best":runner,
        "original_le_object3_size_bytes":len(objects[2]),
        "original_le_object4_size_bytes":len(objects[3]),
        "original_le_object3_possible_base_hex":f"0x{first['base']:08X}",
        "original_le_object4_possible_base_hex":f"0x{second['base']:08X}",
        "object3_unique_return_hints":first["distinct_return_hints"],
        "object4_unique_return_hints":second["distinct_return_hints"],
        "object3_weighted_return_events":first["event_count"],
        "object4_weighted_return_events":second["event_count"],
        "other_object_candidate_byte_gap":gap,
        "other_objects_return_event_candidates":coverage,
        "other_objects_return_event_total":all_outside,
        "unmatched_other_object_return_events":all_outside-coverage,
        "secondary_candidate_runner_up_unique_hints":
            [a[1]["distinct_return_hints"],
             b[1]["distinct_return_hints"]],
        "object_bias_memory_map_independently_verified":False,
        "observed_return_hints_architecturally_valid":False,
        "one_byte_return_hint_discrepancy_explained":False,
        "original_cross_object_CALL_relocations_checked":False,
        "original_live_midi_caller_proven":False,
        "original_emu8000_core_modified":False,
        "autonomous_pcm_parity_proven":False,
        "caveat":"Matching E8 last-byte code positions identifies strong guest load-layout hypotheses, NOT independently executed x86 caller targets or original function timing."
    }

def audit(vxd:bytes,cpu_lines)->dict:
    code=original_executable_objects(vxd)
    eips=Counter()
    returns=Counter()
    seen=0
    unknown=0
    for _,_,_,_,_,_,eip,stack in guest_events(iter(cpu_lines)):
        if len(stack)!=9:
            raise ValueError("Only original V4 stack trace accepted")
        seen+=1
        eips[eip]+=1
        returns[stack[1]]+=1
        if stack[1]==UNKNOWN:unknown+=1
    if seen<1000:
        raise ValueError("No complete original V4 CPU trace")
    result=find_object_bases(code,eips,returns)
    result["guest_v4_cpu_io_events"]=seen
    result["guest_return_unknown_events"]=unknown
    return result

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-original-vxd",required=True,type=Path)
    p.add_argument("--guest-cpu-v4-trace",required=True,type=Path)
    p.add_argument("--private-json",required=True,type=Path)
    args=p.parse_args()
    try:
        if args.private_json.exists():
            raise ValueError("Do not overwrite prior private research")
        raw=args.private_original_vxd.read_bytes()
        with args.guest_cpu_v4_trace.open(encoding="utf-8") as f:
            result=audit(raw,f)
        result["guest_v4_trace_sha256"]=sha256(args.guest_cpu_v4_trace)
        with args.private_json.open("x",encoding="utf-8") as f:
            json.dump(result,f,indent=2,sort_keys=True)
            f.write("\n")
        print("P003_VXD_OTHER_OBJECTS_HYPOTHESES",
              result["other_objects_return_event_candidates"],"/",
              result["other_objects_return_event_total"],
              "not_yet_executed_call_parity=TRUE")
        return 0
    except (OSError,ValueError,UnicodeError,struct.error) as e:
        print("P003_VXD_OTHER_OBJECTS_NO_GO:",e,file=sys.stderr)
        return 2

if __name__=="__main__":
    sys.exit(main())
