#!/usr/bin/env python3
"""Private P003 source-only static caller candidate audit of original SBAWE.VXD.

Find direct x86 near CALL rel32 (E8) instructions targeting the six
independently opcode-verified original Creative EMU8000 I/O helpers.
This does NOT determine which callers executed in the Win95 session,
does NOT recover the 1996 MIDI engine control flow, and does NOT
constitute standalone VST3 timing/audio equivalence.

Original proprietary VXD is input-only. Never publish source bytes.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys

PINNED_VXD_SHA256="d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a"
# Validated 12 opcode sites inside the original 31,200-byte LCOD object.
# These function STARTS are independently reconstructed from x86 stack
# prologues. No executable instruction bytes from the VXD are embedded.
EMU8K_IO_HELPERS = {
    "out_byte": 0x3c49,
    "in_byte": 0x3c7d,
    "out_word": 0x3cb1,
    "in_word": 0x3ce6,
    "out_dword": 0x3d1b,
    "in_dword": 0x3d44,
}
OPCODE_ANCHORS = {
    0x3c5e: 0xee, 0x3c76: 0xee,
    0x3c92: 0xee, 0x3caa: 0xec,
    0x3cc6: 0xee, 0x3cdf: 0xef,
    0x3cfb: 0xee, 0x3d14: 0xed,
    0x3d2d: 0xee, 0x3d3e: 0xef,
    0x3d5a: 0xee, 0x3d68: 0xed,
}

def original_lcod(vxd_bytes: bytes) -> bytes:
    digest=hashlib.sha256(vxd_bytes).hexdigest()
    if digest != PINNED_VXD_SHA256:
        raise ValueError("Original SBAWE.VXD SHA256 differs from pinned private 1996 driver")
    if len(vxd_bytes)<0x100 or vxd_bytes[:2]!=b'MZ':
        raise ValueError("Not a Windows LE/VxD executable")
    le=struct.unpack_from("<I",vxd_bytes,0x3c)[0]
    if le<0x40 or le+0xa0>len(vxd_bytes) or vxd_bytes[le:le+2]!=b'LE':
        raise ValueError("Invalid LE header")
    def u32(o:int)->int:
        return struct.unpack_from("<I",vxd_bytes,le+o)[0]
    object_table=le+u32(0x40)
    if object_table+24>len(vxd_bytes):
        raise ValueError("Object table out of bounds")
    size,_,flags,page_start,pages,_=struct.unpack_from("<IIIIII",vxd_bytes,object_table)
    if not (flags&0x2000) or size<0x3d70 or pages<1 or pages>256:
        raise ValueError("Missing expected original executable code object")
    page_map=le+u32(0x48)
    page_bytes=u32(0x28)
    data_pages=u32(0x80)
    if page_bytes<256 or page_bytes>65536:
        raise ValueError("Invalid LE page size")
    result=bytearray()
    for i in range(pages):
        at=page_map+4*(page_start+i-1)
        if at<0 or at+4>len(vxd_bytes):
            raise ValueError("LE page map outside file")
        page=int.from_bytes(vxd_bytes[at:at+3],"big")
        if page<1 or vxd_bytes[at+3]!=0:
            raise ValueError("Unexpected non-direct LE code page")
        offset=data_pages+page_bytes*(page-1)
        if offset<0 or offset+page_bytes>len(vxd_bytes):
            raise ValueError("LE code page truncated")
        result+=vxd_bytes[offset:offset+page_bytes]
    obj=bytes(result[:size])
    if len(obj)!=size or size!=31200:
        raise ValueError("Original LCOD size changed")
    for off,opcode in OPCODE_ANCHORS.items():
        if obj[off]!=opcode:
            raise ValueError("Pinned original I/O opcode mismatch at "+hex(off))
    for name,off in EMU8K_IO_HELPERS.items():
        if obj[off:off+3]!=b"\x55\x8b\xec":
            raise ValueError("Original helper does not start with x86 stack prologue: "+name)
    return obj

def direct_callers(code:bytes,targets:dict[str,int])->dict[str,list[int]]:
    """Raw E8 rel32 candidates. False positives from embedded data are possible.

    Match the call destination arithmetically. Verifying that byte E8 is
    at an actual disassembled instruction boundary needs separate
    control-flow reconstruction. Do NOT report these as live calls.
    """
    inverse={off:name for name,off in targets.items()}
    if len(inverse)!=len(targets):
        raise ValueError("Duplicate target address")
    result={name:[] for name in targets}
    for at in range(len(code)-4):
        if code[at]!=0xe8:
            continue
        displacement=struct.unpack_from("<i",code,at+1)[0]
        dest=at+5+displacement
        name=inverse.get(dest)
        if name is not None:
            result[name].append(at)
    return result

def nearest_prologue_hint(code:bytes,at:int,max_scan:int=2048)->int|None:
    """Heuristic only; a prior PUSH EBP/MOV EBP,ESP may not be entry."""
    candidate=code.rfind(b"\x55\x8b\xec",max(0,at-max_scan),at+1)
    return candidate if candidate>=0 else None

def audit(code:bytes)->dict:
    callers=direct_callers(code,EMU8K_IO_HELPERS)
    total=sum(map(len,callers.values()))
    if (len(callers["out_word"])<100 or
        len(callers["out_dword"])<100 or
        total<240 or total>400):
        raise ValueError("Original helper call xref population implausible")
    clusters=Counter()
    entries=[]
    for kind,locations in callers.items():
        for at in locations:
            hint=nearest_prologue_hint(code,at)
            if hint is not None:
                clusters[f"LCOD+0x{hint:04X}"]+=1
            entries.append({
                "helper":kind,
                "call_rel32_opcode_offset_hex":f"0x{at:04X}",
                "nearest_push_ebp_prologue_hint_hex":
                    f"0x{hint:04X}" if hint is not None else None,
                "instruction_boundary_verified":False,
                "observed_in_owner_cpu_trace":False,
            })
    return {
        "schema":"125A-P003-PRIVATE-VXD-STATIC-CALLER-CANDIDATES-V1",
        "source_sha256":PINNED_VXD_SHA256,
        "original_lcod_bytes":len(code),
        "known_io_opcode_anchors":len(OPCODE_ANCHORS),
        "direct_near_call_candidates":total,
        "count_by_helper":{n:len(loc) for n,loc in callers.items()},
        "candidate_prologue_clusters":dict(clusters.most_common()),
        "candidate_callers":sorted(entries,key=lambda x:int(x["call_rel32_opcode_offset_hex"],16)),
        "warning":"E8 byte scans yield STATIC POSSIBLE callers, NOT validated instruction boundaries, executed guest call stacks or port timing",
        "missing_live_callers":"CPU trace records I/O helper EIP only; exact return address/stack is required for direct dynamic caller attribution",
        "proprietary_code_bytes_in_report":False,
        "emu8000_core_modified":False,
        "autonomous_pcm_parity":False
    }

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original-vxd",type=Path,required=True)
    p.add_argument("--private-json",type=Path,required=True)
    a=p.parse_args()
    try:
        if a.private_json.exists() or a.private_json.resolve()==a.original_vxd.resolve():
            raise ValueError("Private report must be a new file distinct from VXD")
        code=original_lcod(a.original_vxd.read_bytes())
        report=audit(code)
        with a.private_json.open("x",encoding="utf-8") as f:
            json.dump(report,f,sort_keys=True,indent=2)
            f.write("\n")
        print("P003_ORIGINAL_VXD_STATIC_CALLERS_INDEXED",
              report["direct_near_call_candidates"],
              "live_x86_caller_attribution=NOT_YET_PROVEN",
              "emu8000_core_unchanged=TRUE")
        return 0
    except (OSError,ValueError,struct.error) as e:
        print("P003_ORIGINAL_VXD_STATIC_CALLER_AUDIT_NO_GO:",e,file=sys.stderr)
        return 2

if __name__=="__main__":
    sys.exit(main())
