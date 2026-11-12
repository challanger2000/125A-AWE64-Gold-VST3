#!/usr/bin/env python3
"""Create a PRIVATE fixed-note original SF2 diagnostic fixture for VST3.

This DOES NOT create an all-program bank or claim 86Box/Win95 PCM parity.
It checks independently executed original driver x86 trace handoff against
the pinned original VXD and actual original ROM-only SoundFont bank ID.
Output contains original Creative descriptor/table bytes: keep it PRIVATE,
never add or upload the file to GitHub.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
from p002_extract_original_vxd_sf2_resources import extract, EXPECTED_VXD_SHA256

BANK_SHA="78f8739a25d49e519b3fdc92a7f03d4ddd68588fd0260578a880296e304b7b5f"
SCHEMA="125a-original-bank-to-register-handoff-v1"
MAGIC=b"125AP2D1"
STATE_BYTES=0x1592

def generate(handoff:dict, source:dict, program:int,
             original_release_records:dict|None=None)->bytes:
    if handoff.get("schema")!=SCHEMA or (
        handoff.get("original_vxd_sha256")!=EXPECTED_VXD_SHA256 or
        handoff.get("original_bank_sha256")!=BANK_SHA or
        source.get("source_vxd_sha256")!=EXPECTED_VXD_SHA256
    ):
        raise ValueError("Original x86 source identity mismatch")
    cases=[x["case"] for x in handoff.get("cases",[])
           if x["case"].get("program")==program and
              x["case"].get("bank")==0]
    if len(cases)!=1:
        raise ValueError("Original source case must exist exactly once")
    case=cases[0]
    key,velocity=case["key"],case["velocity"]
    voice_ids=case["physical_voices"]
    after=case["after"]
    if not (0<=key<=127 and 1<=velocity<=127 and
            1<=len(voice_ids)<=6 and len(after)==len(voice_ids) and
            len(set(voice_ids))==len(voice_ids)):
        raise ValueError("Source MIDI/physical voice count invalid")
    # This image is NOT asserted to have been collected from an actual
    # Win95 boot: the x86 handoff was executed under expressly authored
    # zero channel, dry-1MGM conditions. Preserve that limitation.
    context=case["state"]
    if (context.get("rom")!="1MGM" or
        context.get("effects_flag")!=0 or
        context.get("device_attenuation")!=0 or
        context.get("cc7")!=100 or
        context.get("cc11")!=127):
        raise ValueError("Only the verified zero-channel dry ROM x86 state supported")
    state=bytearray(STATE_BYTES)
    struct.pack_into("<I",state,0x158e,0x4d474d31)
    release_version=1
    if original_release_records is not None:
        evidence=original_release_records
        if (evidence.get("schema") !=
            "125a-original-win95-vxd-release-records-v1" or
            evidence.get("original_vxd_sha256")!=EXPECTED_VXD_SHA256 or
            evidence.get("original_bank_sha256")!=BANK_SHA or
            evidence.get("program")!=program or
            evidence.get("channel")!=0 or
            evidence.get("key")!=key or
            evidence.get("velocity")!=velocity):
            raise ValueError("Private original NoteOff source identity mismatch")
        records=evidence.get("records",[])
        if len(records)!=len(voice_ids):
            raise ValueError("Original NoteOff voice count mismatch")
        observed=set()
        for record in records:
            physical=record.get("physical_voice")
            if physical not in voice_ids or physical in observed:
                raise ValueError("Original NoteOff physical voice mismatch")
            observed.add(physical)
            try:
                raw=bytes.fromhex(record["original_vxd_voice_32_hex"])
            except (KeyError,ValueError,TypeError) as ex:
                raise ValueError("Invalid original NoteOff release bytes") from ex
            if (len(raw)!=32 or struct.unpack_from("<H",raw)[0]!=key or
                raw[0x15]>127 or raw[0x16]>127 or
                struct.unpack_from("<I",raw,6)[0]>0xfffffb):
                raise ValueError("Original NoteOff voice marker/rates/end invalid")
            at=0x42+0x20*physical
            state[at:at+32]=raw
        release_version=2
    # Original channel+0x0a/+0x0b/+0x0e/+0x10/+0x12/+0x14 are
    # zero in the independently executed private source test.
    negative=source["volume_attack128_negative_table_unsigned_16"]
    positive=source["volume_attack128_table_unsigned_16"]
    if len(negative)!=15 or len(positive)!=16:
        raise ValueError("Original full signed 31-word table incomplete")
    body=bytearray(MAGIC)
    body+=bytes([release_version,program,0,key,velocity,len(voice_ids)])
    body+=bytes.fromhex(EXPECTED_VXD_SHA256)
    body+=bytes.fromhex(BANK_SHA)
    body+=state
    body+=struct.pack("<31H",*negative,*positive)
    for physical,h in zip(voice_ids,after):
        b=bytes.fromhex(h)
        if not (0<=physical<30 and len(b)==148 and
                struct.unpack_from("<H",b,0x8c)[0]==1):
            raise ValueError("Original x86 post-start voice invalid")
        body.append(physical)
        body+=b
    return bytes(body)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-vxd",required=True,type=Path)
    p.add_argument("--private-handoff",required=True,type=Path)
    p.add_argument("--private-output",required=True,type=Path)
    p.add_argument("--program",required=True,type=int,choices=(0,40,100))
    p.add_argument("--private-original-noteoff-records",type=Path,
                   help="Separate independently checked 32-byte original VXD voice records; "
                        "enables v2 source-authenticated NoteOff. Never guess envelope rates.")
    a=p.parse_args()
    root=Path(__file__).resolve().parent.parent
    output=a.private_output.resolve()
    if output==root or root in output.parents:
        raise ValueError("Never write private Creative source bytes inside Git repository")
    if output.exists():
        raise ValueError("Refusing to overwrite an existing private original-source fixture")
    resource=extract(a.private_vxd.read_bytes())
    handoff=json.loads(a.private_handoff.read_text(encoding="utf8"))
    release=(json.loads(a.private_original_noteoff_records.read_text(encoding="utf8"))
             if a.private_original_noteoff_records else None)
    fixture=generate(handoff,resource,a.program,release)
    output.parent.mkdir(parents=True,exist_ok=True)
    fd=os.open(output,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,"wb") as f:f.write(fixture)
    print("P002_PRIVATE_ORIGINAL_SF2_DIAGNOSTIC_READY",
          "program",a.program,"bytes",len(fixture),
          "sha256",hashlib.sha256(fixture).hexdigest(),
          "original_noteoff_release_records_required_for_v2" if release is None
          else "original_noteoff_records_EXPLICITLY_SUPPLIED_NOT_AUTHENTICATED_BY_PARSER",
          "synthetic_original_x86_channel_state_NOT_actual_Win95_runtime",
          "PCM_PARITY_NOT_PROVEN")

if __name__=="__main__":
    try: main()
    except (OSError,ValueError,KeyError,TypeError,struct.error) as e:
        print("P002_PRIVATE_DIAGNOSTIC_REJECTED",e,file=sys.stderr)
        sys.exit(1)
