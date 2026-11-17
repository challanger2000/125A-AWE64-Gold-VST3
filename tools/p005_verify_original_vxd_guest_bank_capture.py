#!/usr/bin/env python3
"""Validate PRIVATE P005 Win95 guest VXD-bank capture structure (NOT provenance).

This reads the 125AVXD1 owner-side, read-only 86Box process capture. It does
NOT assume the source is genuine merely because its magic/driver hash is
correct. Genuine original Creative driver/session provenance, device image
identity and capture timing must be independently established before any
VST3 source-bound catalog activation.

Never upload real original Win95 / VXD / SF2 capture bytes to public GitHub.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct

MAGIC=b"125AVXD1"
DEVICE_BYTES=0x1592
SLOTS=288
SLOT_OFFSET=0x6ca
ROW_BYTES=26
ROW_LIMIT=65536
MAX_FILE_BYTES=8*1024*1024
EXPECTED_ORIGINAL_VXD_SHA256=(
    "d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a"
)

class CaptureError(ValueError):
    pass

def require(ok:bool,message:str)->None:
    if not ok:
        raise CaptureError(message)

def sha(data:bytes)->str:
    return hashlib.sha256(data).hexdigest()

def parse_capture(raw:bytes)->dict:
    require(36+DEVICE_BYTES<=len(raw)<=MAX_FILE_BYTES,
            "capture incomplete or too large")
    require(raw[:8]==MAGIC,"not an authentic P005 capture-file schema")
    version,device_va,cr3,device_size,slots,unique_count,pointer_count=(
        struct.unpack_from("<7I",raw,8))
    require(version==1 and device_size==DEVICE_BYTES and slots==SLOTS,
            "wrong original driver device size, slot count or version")
    require(device_va>=0xc0000000 and
            device_va+DEVICE_BYTES<=1<<32 and
            cr3%4096==0 and cr3<64*1024*1024,
            "invalid guest kernel device virtual address / page directory")
    require(1<=unique_count<=SLOTS and
            1<=pointer_count<=SLOTS and
            unique_count<=pointer_count,
            "no valid source bank or impossible alias count")
    device=raw[36:36+DEVICE_BYTES]
    require(struct.unpack_from("<I",device,0x158e)[0]==0x4d474d31 and
            struct.unpack_from("<H",device,6)[0]==32,
            "wrong original Creative VXD device identity/capacity")
    all_pointers=struct.unpack_from("<288I",device,SLOT_OFFSET)
    pointers={ptr for ptr in all_pointers if ptr!=0}
    require(len(pointers)==unique_count and
            sum(p!=0 for p in all_pointers)==pointer_count,
            "capture bank pointers do not match original VXD device image")
    at=36+DEVICE_BYTES
    seen={}
    rows_total=0
    for _ in range(unique_count):
        require(at+8<=len(raw),"truncated original pointer record header")
        address,size=struct.unpack_from("<II",raw,at)
        at+=8
        require(address in pointers and address not in seen,
                "unreferenced or duplicate original VXD guest pointer")
        require(ROW_BYTES<=size<=ROW_BYTES*ROW_LIMIT+4096 and
                at+size<=len(raw),
                "original Creative bank data truncated/oversized")
        blob=raw[at:at+size]
        at+=size
        indirect=False
        if len(blob)>=28:
            nested_header=struct.unpack_from("<7I",blob,0)
            first,second,third=nested_header[:3]
            looks_like_header=(first>address and
                               28<=first-address<=4096 and
                               first<second and
                               (first&0xff000000)==(address&0xff000000))
            if looks_like_header:
                require(all(nested_header[i]<nested_header[i+1]
                            for i in range(6)),
                        "out-of-order Creative bank header pointer")
            indirect=looks_like_header
        start=first-address if indirect else 0
        require((size-start)>=ROW_BYTES and
                (indirect or size%ROW_BYTES==0),
                "original guest program records misaligned")
        if indirect:
            span=second-first
            require(span>=ROW_BYTES and span%ROW_BYTES==0 and
                    start+span<=size and span//ROW_BYTES<=ROW_LIMIT,
                    "indirect Creative header has invalid child program boundary")
            count=span//ROW_BYTES
        else:
            count=size//ROW_BYTES
        for ordinal in range(count):
            pos=start+ordinal*ROW_BYTES
            marker=struct.unpack_from("<H",blob,pos)[0]
            require((marker==0xffff)==(ordinal==count-1),
                    "Creative source FFFF sentinel not at real program boundary")
        rows_total+=count
        require(rows_total<=ROW_LIMIT,"source row count exceeds capture bound")
        # Program-row validity is NOT full Creative bank completeness.
        # The owner-observed Win95 root contains seven distinct internal
        # guest pointers. Adjacent pointers bound six nested regions;
        # the final region's end is not known from these seven pointers.
        nested_inventory=None
        if indirect and size>=28:
            nested_ptrs=struct.unpack_from("<7I",blob,0)
            if all(nested_ptrs[i]<nested_ptrs[i+1] for i in range(6)) and (
                nested_ptrs[0]==first
            ):
                captured_end=address+size
                complete=sum(
                    int(nested_ptrs[i+1]<=captured_end)
                    for i in range(6)
                )
                nested_inventory={
                    "nested_pointers":len(nested_ptrs),
                    "bounded_regions":6,
                    "bounded_regions_fully_captured":complete,
                    "bounded_regions_not_fully_captured":6-complete,
                    "last_region_end_unknown":True,
                    "last_pointer_within_capture":nested_ptrs[6]<captured_end,
                    "entire_nested_bank_attested":False
                }
        seen[address]={
            "total_rows_including_sentinel":count,
            "active_rows":count-1,
            "bytes_sha256":sha(blob),
            "source_layout":"indirect_bank_header" if indirect else "direct_rows",
            "nested_program_guest_pointer":f"0x{first:08x}" if indirect else None,
            "legacy_trailing_unclassified_bytes":size-(start+count*ROW_BYTES),
            "additional_nested_regions":nested_inventory
        }
    require(set(seen)==pointers and at==len(raw),
            "incomplete, trailing or mismatched guest source regions")
    require(sum(row["active_rows"] for row in seen.values())>0,
            "no loaded/active original VXD preset rows captured")
    return {
        "schema":"125A-P005-PRIVATE-VXD-GUEST-CAPTURE-AUDIT-V1",
        "status":"STRUCTURE_VALID_SOURCE_PROVENANCE_STILL_REQUIRED",
        "capture_sha256":sha(raw),
        "device_image_sha256":sha(device),
        "guest_device_virtual_address":f"0x{device_va:08x}",
        "guest_page_directory_physical_address":f"0x{cr3:08x}",
        "device_bytes":DEVICE_BYTES,
        "original_vxd_program_slots":SLOTS,
        "nonzero_original_slot_pointers":pointer_count,
        "unique_guest_program_record_lists":unique_count,
        "normalized_source_rows_including_sentinels":rows_total,
        "source_rows_content_hashes":[
            {"original_guest_pointer":f"0x{addr:08x}",**seen[addr]}
            for addr in sorted(seen)
        ],
        "real_win95_driver_integrity":"NOT_CHECKED_BY_CAPTURE",
        "original_guest_time_and_source_correlation":"NOT_CHECKED_BY_CAPTURE",
        "production_vst3_activation_authorized":False
    }

def audit_file(path:Path,original_vxd:Path|None=None)->dict:
    require(path.is_file() and path.stat().st_size<=MAX_FILE_BYTES,
            "capture file unavailable or over size cap")
    result=parse_capture(path.read_bytes())
    if original_vxd is not None:
        require(original_vxd.is_file(),"original Creative VXD missing")
        digest=sha(original_vxd.read_bytes())
        require(digest==EXPECTED_ORIGINAL_VXD_SHA256,
                "original 1996 Creative VXD SHA256 mismatch")
        result["source_original_vxd_file_sha256"]=digest
        result["source_original_vxd_file_pinned_sha_verified"]=True
        # A verified copy of the ORIGINAL driver on the host is not
        # by itself proof that THIS GUEST used it during this capture.
        result["original_guest_time_and_source_correlation"]="NOT_CHECKED_BY_CAPTURE"
    return result

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--capture",type=Path,required=True,
                   help="private 125AVXD1 binary from the owner 86Box process")
    p.add_argument("--original-vxd",type=Path,
                   help="optional owner-only pinned SBAWE.VXD")
    p.add_argument("--report",type=Path,
                   help="optional NEW local JSON report, never public GitHub")
    args=p.parse_args()
    try:
        result=audit_file(args.capture,args.original_vxd)
        rendered=json.dumps(result,indent=2,sort_keys=True)+"\n"
        if args.report:
            with args.report.open("x",encoding="utf-8") as out:
                out.write(rendered)
        print(rendered)
        return 0
    except (OSError,CaptureError) as exc:
        print("P005_ORIGINAL_GUEST_BANK_SOURCE_NO_GO:",exc)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
