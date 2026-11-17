#!/usr/bin/env python3
"""P108 publicly compiled Win95 3-bank stress PE32 binary provenance diff.

PUBLIC test executable only, NEVER owner's Creative VxD, guest VM, ROM,
private soundfont, x86 or TSC traces. The original public CI artifact is
SHA256 pinned, and rebuilt PE is compared by COFF metadata and raw sections.
Metadata-only differences may explain nonidentical hashes but are NOT a
bit-identical original guest proof. Never substitute rebuilt PE in owner run
without an explicit original SHA match.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ORIGINAL_PE_SHA256="3c9306261ed599073f80d9695a8d73140ea65e2de101dc431169c2e6b3f6bc9f"
MAX_SIZE=1_000_000

def sha(data:bytes)->str:return hashlib.sha256(data).hexdigest()

def parse_public_pe32(data:bytes):
    if not 256<=len(data)<=MAX_SIZE or data[:2]!=b"MZ":
        raise ValueError("public Win95 test is not a bounded MZ image")
    peoff=struct.unpack_from("<I",data,0x3c)[0]
    if not 64<=peoff<=len(data)-24 or data[peoff:peoff+4]!=b"PE\0\0":
        raise ValueError("public Win95 MZ image has no valid PE signature")
    machine,count,ts,psym,nsym,opt_size,flags=struct.unpack_from(
        "<HHIIIHH",data,peoff+4)
    if machine!=0x14c or count>32 or not 1<=count or opt_size<96:
        raise ValueError("public Win95 original stress is not i386 PE32")
    optional=peoff+24
    if struct.unpack_from("<H",data,optional)[0]!=0x10b:
        raise ValueError("public Win95 guest requires PE32")
    section_at=optional+opt_size
    if section_at+40*count>len(data):
        raise ValueError("truncated public PE32 section table")
    sections={}
    for n in range(count):
        start=section_at+n*40
        name=data[start:start+8].split(b"\0",1)[0].decode("ascii","strict")
        rawsize=struct.unpack_from("<I",data,start+16)[0]
        rawstart=struct.unpack_from("<I",data,start+20)[0]
        if rawstart>len(data) or rawsize>len(data)-rawstart:
            raise ValueError("public PE32 section not inside file")
        if name in sections:raise ValueError("duplicate PE32 section")
        sections[name]={"size":rawsize,"sha256":sha(data[rawstart:rawstart+rawsize])}
    return {"coff_timestamp":ts,
            "coff_symbol_table_offset":psym,
            "coff_symbols":nsym,
            "section_names":list(sections),
            "sections":sections,
            "optional_checksum":struct.unpack_from("<I",data,optional+64)[0]}

def compare(original:bytes,rebuild:bytes):
    if sha(original)!=ORIGINAL_PE_SHA256:
        raise ValueError("not the published P003 original 3bank SHA-pinned PE32")
    a=parse_public_pe32(original)
    b=parse_public_pe32(rebuild)
    names=set(a["sections"])|set(b["sections"])
    differing_sections=sorted(n for n in names
        if a["sections"].get(n)!=b["sections"].get(n))
    metadata_fields=("coff_timestamp","coff_symbol_table_offset",
                     "coff_symbols","optional_checksum")
    metadata={field:{"original":a[field],"rebuild":b[field]}
              for field in metadata_fields if a[field]!=b[field]}
    length=min(len(original),len(rebuild))
    offsets=[i for i in range(length) if original[i]!=rebuild[i]]
    intervals=[]
    for i in offsets:
        if not intervals or i!=intervals[-1][1]+1:
            intervals.append([i,i])
        else:intervals[-1][1]=i
    return {
      "schema":"P108-PUBLIC-PE32-ORIGINAL-VS-REBUILD-BINARY-REPRODUCIBILITY",
      "original_sha256":ORIGINAL_PE_SHA256,
      "rebuilt_sha256":sha(rebuild),
      "byte_identical":original==rebuild,
      "original_bytes":len(original),
      "rebuilt_bytes":len(rebuild),
      "different_raw_bytes":len(offsets)+abs(len(original)-len(rebuild)),
      "different_byte_intervals":intervals[:30],
      "different_section_hashes":differing_sections,
      "different_coff_metadata":metadata,
      "rebuilt_safe_substitute_for_sha_pinned_original":original==rebuild,
      "original_win95_guest_executed":False,
      "source_creative_dsp_modified":False,
    }

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--published-original-pe32",required=True,type=Path)
    p.add_argument("--recompiled-pe32",required=True,type=Path)
    args=p.parse_args()
    try:
        result=compare(args.published_original_pe32.read_bytes(),
                       args.recompiled_pe32.read_bytes())
        print("P108_PUBLIC_ORIGINAL_3BANK_PE32_DIFFERENTIAL "+json.dumps(result,sort_keys=True))
        return 0
    except (OSError,ValueError,struct.error,UnicodeError) as e:
        print("P108_PUBLIC_PE32_COMPARISON_NO_GO "+str(e),file=sys.stderr)
        return 2
if __name__=="__main__":sys.exit(main())
