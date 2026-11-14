#!/usr/bin/env python3
"""Extract only source-authenticated private 820-byte Creative VXD tables.

Consumes the owner's actual original 77,370-byte SBAWE.VXD. Never
embeds/downloads/distributes the source binaries in the VST3 or GitHub.
Together with the private original SF2, these are the ONLY inputs for
awe64_p002_private_original_ram_descriptor_probe.exe on Windows x64.
This produces original-driver *candidate descriptors*, NOT DSP/PCM parity.
"""
from __future__ import annotations
import argparse
import hashlib
import os
from pathlib import Path
import struct
try:
    from .p002_extract_original_vxd_sf2_resources import extract
except ImportError:
    from p002_extract_original_vxd_sf2_resources import extract

EXPECTED_TABLE_SHA256="0703643c6df3a8af8aa3ee94a272b40ff349462245f6e2491c27dc3344a1761a"

def make_original_table_bytes(resources:dict)->bytes:
    # These are actual original Win95 Creative PCOD/LCOD/PDAT resource
    # formats, not arbitrary emulator tables or inferred approximations.
    value=(
        struct.pack("<59h",*resources["template_first_59_signed_words"]) +
        struct.pack("<128I",*resources["pitch_table_a_unsigned_32"]) +
        struct.pack("<128b",*resources["pitch_table_b_signed_8"]) +
        struct.pack("<15H",*resources["volume_attack128_negative_table_unsigned_16"]) +
        struct.pack("<16H",*resources["volume_attack128_table_unsigned_16"])
    )
    if len(value)!=820:
        raise ValueError("Original SBAWE.VXD private source table geometry invalid")
    return value

def private_table_from_original_vxd(source:bytes)->bytes:
    # Authenticates whole original VXD SHA and LE object/page mappings;
    # validates a SECOND, independent SHA over the extracted 820 bytes.
    resources=extract(source)
    table=make_original_table_bytes(resources)
    if hashlib.sha256(table).hexdigest()!=EXPECTED_TABLE_SHA256:
        raise ValueError("Original Creative PCOD table SHA mismatch")
    return table

def main(argv=None)->int:
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--private-original-vxd",required=True,type=Path)
    cli.add_argument("--private-output",required=True,type=Path)
    a=cli.parse_args(argv)
    root=Path(__file__).resolve().parents[1]
    output=a.private_output.resolve()
    if (output.exists() or output==a.private_original_vxd.resolve() or
            output==root or root in output.parents):
        cli.error("Original Creative private output must be NEW and outside Git")
    table=private_table_from_original_vxd(a.private_original_vxd.read_bytes())
    output.parent.mkdir(parents=True,exist_ok=True)
    fd=os.open(output,os.O_CREAT|os.O_WRONLY|os.O_EXCL,0o600)
    with os.fdopen(fd,"wb") as stream:stream.write(table)
    print("P002_AUTHENTIC_1996_CREATIVE_VXD_PRIVATE_TABLES_READY",
          "byte_count",len(table),"sha256",
          hashlib.sha256(table).hexdigest(),"original_vs_native_pcm=NOT_PROVEN")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
