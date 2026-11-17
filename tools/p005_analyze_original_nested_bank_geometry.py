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
    # Cross the source boundary from the authentic 26-byte VXD
    # program rows into nested region #2. Offset +4 of every row is
    # a little-endian u16 referencing a real entry of the 32-bit
    # index array, NOT merely a guessed SoundFont/PHDR relation.
    root=struct.unpack_from("<I",nested,20)[0]
    pointers=struct.unpack_from("<7I",nested,24)
    check(pointers[0]>=root and pointers[1]>=pointers[0],
          "invalid original program-row pointers")
    prefix=pointers[0]-root
    row_span=pointers[1]-pointers[0]
    check(row_span>=26 and row_span%26==0,
          "original program rows are not 26-byte aligned")
    primary_offset=36+0x1592
    check(primary_offset+8<=len(primary),"missing original root")
    listed_root,length=struct.unpack_from("<2I",primary,primary_offset)
    check(listed_root==root and length>=prefix+row_span,
          "original program bytes missing")
    rows=primary[primary_offset+8+prefix:
                 primary_offset+8+prefix+row_span]
    program_indices=[
        struct.unpack_from("<H",rows,i*26+4)[0]
        for i in range(len(rows)//26)
    ]
    program_markers=[
        struct.unpack_from("<H",rows,i*26)[0]
        for i in range(len(rows)//26)
    ]
    index_count=len(regions[0])//4
    check(len(regions[0])%4==0 and index_count>=2,
          "nested program lookup array is not u32 aligned")
    check(len(program_indices)>=2 and
          all(program_markers[i] not in (0,0xffff)
              for i in range(len(program_markers)-1)) and
          program_markers[-1]==0xffff,
          "original program list sentinel misplaced")
    check(program_indices[0]==0 and
          all(a<b for a,b in zip(program_indices,program_indices[1:])),
          "original program-to-index references not strictly ascending")
    check(program_indices[-1]==index_count-1,
          "original sentinel does not point to terminal index")
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
            "source_region":origin+2,
            "source_integer_width_bytes":width,
            "source_index_count":len(offsets),
            "target_region":target+2,
            "target_record_bytes":element_size,
            "target_record_count":target_count,
            "terminal_index":offsets[-1],
            "index_boundary_model":
                "exclusive_count" if end_is_count
                else "last_valid_record_index",
            "relation":"EXACT_MATCH"
        })
    # The real bank stores two 3-byte generator-like arrays, u8
    # operation + u16 value. Their operation numbers fit SF2 generator
    # IDs (0..58) and the original project's SF2 reader uses 41 for
    # instrument and 53 for sampleID. This remains structural evidence:
    # a 1996 Creative runtime instruction-level interpretation is needed.
    preset_triples=[
        regions[1][i:i+3] for i in range(0,len(regions[1]),3)]
    instrument_triples=[
        regions[4][i:i+3] for i in range(0,len(regions[4]),3)]
    check(all(rec[0]<59 for rec in preset_triples+instrument_triples),
          "nested 3-byte records contain non-SF2 generator numbers")
    preset_offsets=unpack(regions[0],4)
    preset_bag_last_operations=[
        preset_triples[end-1][0] if end>start else None
        for start,end in zip(preset_offsets,preset_offsets[1:])
    ]
    # For the specific owner-observed GM source, ALL 181 nonempty
    # preset bag groups end with instrument generator op 41. Do not
    # enforce this on unrelated SF2 banks which can have global zones.
    owner_gm_shape=(len(program_indices)==130 and
                    len(preset_offsets)==182)
    if owner_gm_shape:
        check(all(op==41 for op in preset_bag_last_operations),
              "owner GM program groups do not end in instrument opcode")
    instrument_refs=[
        int.from_bytes(rec[1:3],"little")
        for rec in preset_triples if rec[0]==41
    ]
    instrument_table_length=len(regions[2])//2
    check(all(ref<instrument_table_length for ref in instrument_refs),
          "preset instrument reference exceeds captured instrument index table")
    sample_refs=[
        int.from_bytes(rec[1:3],"little")
        for rec in instrument_triples if rec[0]==53
    ]
    packed_opcodes={
        "three_byte_record_layout_candidate":"u8 opcode + u16 little-endian amount",
        "all_generator_ids_within_sf2_range":True,
        "preset_instrument_opcode_41_count":len(instrument_refs),
        "preset_groups_ending_in_opcode_41":sum(
            op==41 for op in preset_bag_last_operations),
        "preset_group_count":len(preset_bag_last_operations),
        "instrument_references_within_source_index_table":True,
        "instrument_sample_id_opcode_53_count":len(sample_refs),
        "sample_id_max_seen":max(sample_refs) if sample_refs else None,
        "actual_creative_instruction_semantics_proven":False
    }
    return {
        "status":"ORIGINAL_NESTED_INDEX_GEOMETRY_MATCH",
        "capture_sha256":verified["nested_capture_sha256"],
        "primary_capture_sha256":verified["primary_capture_sha256"],
        "original_program_to_nested_index":{
            "program_rows_including_sentinel":len(program_indices),
            "first_original_row_index":program_indices[0],
            "last_original_sentinel_index":program_indices[-1],
            "nested_u32_index_count":index_count,
            "strictly_increasing":True,
            "relation":"EXACT_MATCH"
        },
        "observed_structural_relations":audit,
        "packed_generator_evidence":packed_opcodes,
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
