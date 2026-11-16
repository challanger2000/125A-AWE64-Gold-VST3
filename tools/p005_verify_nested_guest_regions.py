#!/usr/bin/env python3
"""Verify private supplemental 125AVXR1 original-Win95 guest regions.

Read-only and offline. This does NOT attest the running driver, RAM timing,
or an unknown final region's end. No original Creative bytes in this source.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path
from p005_verify_original_vxd_guest_bank_capture import parse_capture

MAGIC=b"125AVXR1"
MAX_FILE=8*1024*1024
class NestedError(ValueError):
    pass

def check(ok:bool,what:str)->None:
    if not ok:
        raise NestedError(what)

def parse_nested(raw:bytes, primary:bytes)->dict:
    # Reuse existing independently tested primary schema validator.
    parse_capture(primary)
    check(52<=len(raw)<=MAX_FILE,"nested file length invalid")
    check(raw[:8]==MAGIC,"not a 125AVXR1 nested source file")
    version,device_va,cr3,root=struct.unpack_from("<4I",raw,8)
    check(version==1,"unsupported nested source version")
    pointers=struct.unpack_from("<7I",raw,24)
    count=struct.unpack_from("<I",raw,52)[0] if len(raw)>=56 else 0
    check(count==6 and len(raw)>=56,"expected six supplementary regions")
    check(pointers[0]>root and 28<=pointers[0]-root<=4096,
          "invalid first child pointer")
    check(all(pointers[i]<pointers[i+1] for i in range(6)),
          "nested pointers out of order")
    # Primary metadata and original seven-pointer header must match
    # byte-for-byte. No synthesized source-region offsets.
    p_dev_va,p_cr3=struct.unpack_from("<2I",primary,12)
    check((device_va,cr3)==(p_dev_va,p_cr3),
          "supplementary snapshot does not match primary guest")
    device=primary[36:36+0x1592]
    bank_roots=struct.unpack_from("<288I",device,0x6ca)
    check(root in bank_roots,"nested root not in original device")
    offset=36+0x1592
    found=False
    unique=struct.unpack_from("<I",primary,28)[0]
    for _ in range(unique):
        check(offset+8<=len(primary),"primary list record truncated")
        ptr,length=struct.unpack_from("<2I",primary,offset)
        offset+=8
        check(length<=len(primary)-offset,"primary list truncated")
        if ptr==root:
            check(length>=28 and
                  struct.unpack_from("<7I",primary,offset)==pointers,
                  "nested pointers disagree with original primary")
            found=True
        offset+=length
    check(found,"no matching primary bank header")
    offset=56
    sizes=[]
    for index in range(1,7):
        check(offset+12<=len(raw),"nested section header truncated")
        addr,size,flag=struct.unpack_from("<3I",raw,offset)
        offset+=12
        bounded=index<6
        check(addr==pointers[index],"nested region address mismatch")
        expected=pointers[index+1]-pointers[index] if bounded else 4096
        check(size==expected and flag==(1 if bounded else 0),
              "nested region size/completeness mismatch")
        check(size>0 and size<=MAX_FILE and size<=len(raw)-offset,
              "nested region payload truncated")
        sizes.append(size)
        offset+=size
    check(offset==len(raw),"unexpected nested trailing bytes")
    return {
        "status":"NESTED_REGIONS_STRUCTURALLY_VALID_SOURCE_NOT_ATTESTED",
        "nested_capture_sha256":hashlib.sha256(raw).hexdigest(),
        "primary_capture_sha256":hashlib.sha256(primary).hexdigest(),
        "original_device_virtual_address":f"0x{device_va:08x}",
        "original_root_guest_address":f"0x{root:08x}",
        "bounded_additional_regions_captured":5,
        "bounded_additional_region_bytes":sum(sizes[:5]),
        "last_region_partial_sample_bytes":sizes[5],
        "last_region_complete":False,
        "loaded_driver_and_bank_time_attested":False,
        "vst3_production_authorized":False
    }

def main()->int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--primary",type=Path,required=True)
    parser.add_argument("--nested",type=Path,required=True)
    a=parser.parse_args()
    try:
        print(json.dumps(parse_nested(a.nested.read_bytes(),
                                      a.primary.read_bytes()),indent=2))
        return 0
    except (OSError,NestedError,ValueError) as exc:
        print("P005_NESTED_CAPTURE_NO_GO:",exc)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
