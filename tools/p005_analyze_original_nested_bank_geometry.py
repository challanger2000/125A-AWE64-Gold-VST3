#!/usr/bin/env python3
"""P005 source-only analyzer of original nested Creative bank table geometry.

The names of these arrays and their exact role in 1996 Creative driver
routines are NOT inferred. This tool proves byte widths, bounded offsets,
and cross-region cardinality for the owner-observed bank layout, without
copying any private bytes or enabling production source attestation.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path
from p005_verify_nested_guest_regions import parse_nested

class GeometryError(ValueError):
    pass

def check(ok: bool, message: str) -> None:
    if not ok:
        raise GeometryError(message)

def unpack(raw: bytes, width: int) -> tuple[int,...]:
    check(width in (2,4) and len(raw)%width==0,
          "index region alignment invalid")
    fmt="<"+("H" if width==2 else "I")*(len(raw)//width)
    return struct.unpack(fmt,raw)

def analyze(primary:bytes, nested:bytes)->dict:
    verified=parse_nested(nested,primary)
    regions=[]
    at=56
    for index in range(6):
        address,length,flag=struct.unpack_from("<3I",nested,at)
        at+=12
        data=nested[at:at+length]
        at+=length
        regions.append(data)
    check(at==len(nested),"trailing unclassified input")
    # The source observed on 17 Nov has three exact hierarchical
    # cardinality relations. Do not label arrays as PHDR/IBAG/IGEN etc:
    # their semantic identity must be proven from Creative VXD code.
    links=(
        (0,4,1,3,True),   # 182 u32 offsets -> 466 3-byte entries
        (2,2,3,4,False),  # 180 u16 offsets -> 744 dwords (last 743)
        (3,4,4,3,True),   # 744 u32 offsets -> 3644 3-byte entries
    )
    audit=[]
    for origin,width,target,element_size,end_is_count in links:
        offsets=unpack(regions[origin],width)
        target_bytes=len(regions[target])
        check(target_bytes%element_size==0,
              "target segment is not an integer number of records")
        target_count=target_bytes//element_size
        check(len(offsets)>=2 and offsets[0]==0,
              "index array missing offset zero")
        check(all(a<=b for a,b in zip(offsets,offsets[1:])),
              "index offsets not monotonic")
        check(all(value<=target_count for value in offsets),
              "index array exceeds target region")
        check(offsets[-1]==(target_count if end_is_count
                            else target_count-1),
              "index terminal does not match next original region")
        audit.append({
            "source_region":origin+1,
            "source_integer_width_bytes":width,
            "source_index_count":len(offsets),
            "target_region":target+1,
            "target_record_bytes":element_size,
            "target_record_count":target_count,
            "terminal_index":offsets[-1],
            "index_boundary_model":
                "exclusive_count" if end_is_count
                else "last_valid_record_index",
            "relation":"EXACT_MATCH"
        })
    return {
        "status":"ORIGINAL_NESTED_INDEX_GEOMETRY_MATCH",
        "capture_sha256":verified["nested_capture_sha256"],
        "primary_capture_sha256":verified["primary_capture_sha256"],
        "observed_structural_relations":audit,
        "last_region_4096_byte_sample_only":True,
        "table_semantics_independently_proven":False,
        "win95_creative_driver_execution_proven":False,
        "vst3_production_authorized":False
    }

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--primary",type=Path,required=True)
    p.add_argument("--nested",type=Path,required=True)
    p.add_argument("--report",type=Path)
    a=p.parse_args()
    try:
        report=analyze(a.primary.read_bytes(),a.nested.read_bytes())
        data=json.dumps(report,indent=2)+"\n"
        if a.report:
            a.report.write_text(data,encoding="utf-8")
        print(data,end="")
        return 0
    except (OSError,GeometryError,ValueError) as err:
        print("P005_NESTED_INDEX_GEOMETRY_NO_GO:",err)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
