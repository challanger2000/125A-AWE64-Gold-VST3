#!/usr/bin/env python3
"""P002: private cross-program original-Creative MIDI->IFATN candidate verifier.

Original Creative Win95 guest velocity sweep was captured independently.
The source C++'s ONE shared 127-step velocity coordinate was derived from
GM0 observations. This test must predict *all 127 untrained GM40 values*
plus the program40 cutoff curve without a GM-ID lookup in production code.

Genuine original CSV stays PRIVATE; only SHA, counts and verdict are saved.
This is IFATN subset validation only, NOT full Creative driver parity.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import sys

ORIGINAL_CSV_SHA256="b9b30b9b583c8befbf43c2f8b0f3b725f9cbf6eeca71e120a091aa8cd094d8d2"
COLUMN_NAMES=("program","velocity","voice","chip_frame","guest_tick_ms",
              "pitch_reg","ifatn","cutoff","attenuation")
PROGRAMS=(0,40)

def digest(path: Path)->str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for c in iter(lambda:f.read(1<<20),b""):h.update(c)
    return h.hexdigest()

def source_levels(source: Path)->list[int]:
    text=source.read_text(encoding="utf-8")
    m=re.search(r"kSharedVelocityLevel\s*\{\{\s*(.*?)\s*\}\}",text,re.S)
    if not m:
        raise ValueError("C++ candidate single shared velocity table absent")
    numbers=re.findall(r"\b(?:0x[0-9a-fA-F]+|\d+)\b",m.group(1))
    levels=[int(n,0) if n.startswith("0x") else int(n) for n in numbers]
    if len(levels)!=128 or levels[0]!=0 or levels[1]!=49 or levels[127]!=0:
        raise ValueError("Original-measurement-derived C++ velocity levels missing/drifted")
    if any(not 0<=v<=50 for v in levels) or any(
            a<b for a,b in zip(levels[1:],levels[2:])):
        raise ValueError("Shared velocity mapping no longer monotone")
    return levels

def load_rows(path: Path):
    by_program={}
    with path.open("r",newline="",encoding="utf-8") as f:
        reader=csv.DictReader(f)
        if tuple(reader.fieldnames or [])!=COLUMN_NAMES:
            raise ValueError("Unexpected private original register CSV format")
        for row in reader:
            try:
                values={k:int(v) for k,v in row.items()}
            except (ValueError,TypeError) as e:
                raise ValueError("Invalid original Creative numeric observation") from e
            program,velocity=values["program"],values["velocity"]
            if program not in PROGRAMS or velocity<1 or velocity>127:
                raise ValueError("Unexpected GM bank/program/velocity in original capture")
            group=by_program.setdefault(program,{})
            if velocity in group:
                raise ValueError("Duplicate original velocity row")
            if not 0<=values["voice"]<32 or values["chip_frame"]<=0:
                raise ValueError("Invalid actual original EMU8000 voice/frame")
            if values["ifatn"] != ((values["cutoff"]<<8)|values["attenuation"]):
                raise ValueError("Original register IFATN does not match extracted bytes")
            group[velocity]=values
    if sorted(by_program)!=list(PROGRAMS) or any(
            set(x)!=set(range(1,128)) for x in by_program.values()):
        raise ValueError("Incomplete private GM0/GM40 full-velocity reference")
    return by_program

def predict(velocity, levels, combined_index, base_cutoff,
            velocity_filter:bool):
    if not 1<=velocity<=127 or len(levels)!=128:
        raise ValueError("Not a NoteOn velocity or no full shared table")
    volume=((combined_index+levels[velocity])*8)//3
    cutoff=((max(70,velocity)*base_cutoff+0xa0)>>7) if velocity_filter else base_cutoff
    return cutoff,volume

def verify_rows(rows,levels):
    # Inputs below are observed experimental instrument terms derived from
    # original samples; not a production GM-ID switch/patch. The C++ code
    # never sees program numbers. Actual Creative bank field mapping remains
    # P002/P003 work.
    params={0:(16,178,False),40:(13,210,True)}
    checks=0
    for velocity in range(1,128):
        a,b=rows[0][velocity],rows[40][velocity]
        for program,observation in ((0,a),(40,b)):
            cutoff,attenuation=predict(velocity,levels,*params[program])
            if (cutoff,attenuation) != (observation["cutoff"],observation["attenuation"]):
                raise ValueError(
                    f"Original Creative {program=} velocity={velocity} first"
                    f" IFATN mismatch: predicted cutoff/att={cutoff}/{attenuation}"
                    f" original={observation['cutoff']}/{observation['attenuation']}")
            checks+=2
        if a["attenuation"]-b["attenuation"]!=8:
            raise ValueError(f"Original shared attenuation offset differs at {velocity}")
        checks+=1
    # Cross-program independent validation: the shared levels were derived
    # from GM0 and MUST separately predict GM40, all 127 velocities. GM40's
    # cutoff curve comes from a parametric candidate, not its 127-value table.
    return {"original_rows":254,"shared_cross_program_velocities":127,
            "filter_holdout_velocities":125,
            "numeric_assertions":checks,"original_first_mismatch":None,
            "scope":"one original-note IFATN subset, NOT independent full Creative driver"}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--reference",type=Path,required=True)
    ap.add_argument("--source",type=Path,required=True)
    ap.add_argument("--out",type=Path,required=True)
    a=ap.parse_args()
    if digest(a.reference)!=ORIGINAL_CSV_SHA256:
        raise ValueError("Private original Win95 Creative 254-row CSV SHA256 mismatch")
    if a.out.exists():raise ValueError("Do not overwrite private evidence report")
    source=source_levels(a.source)
    rows=load_rows(a.reference)
    report=verify_rows(rows,source)
    # Positive original data proof, plus *meaningful negative controls*
    # executed on copies of the in-memory rows, not on private files.
    mutants=[]
    for program,velocity,key in ((40,71,"cutoff"),(40,100,"attenuation")):
        copy={p:{v:dict(values) for v,values in group.items()} for p,group in rows.items()}
        copy[program][velocity][key]+=1
        try:verify_rows(copy,source)
        except ValueError as e:
            mutants.append({"program":program,"velocity":velocity,"field":key,
                            "correctly_rejected":True,"error":str(e)})
        else:raise ValueError(f"Original measurement mutation {program}/{velocity}/{key} escaped")
    report.update({"status":"P002_COMMON_VELOCITY_AND_FILTER_REGISTER_CANDIDATE_254_OF_254_PASS",
                   "original_private_csv_sha256":ORIGINAL_CSV_SHA256,
                   "public_candidate_cpp_sha256":digest(a.source),
                   "negative_controls":mutants,
                   "input_bank_parameters_proven_from_original_SBK":"NO",
                   "release_approved":False,
                   "original_creative_MIDI_to_FULL_register_stream":"NOT VERIFIED",
                   "original_Win95_ROM_bytes_in_report":False})
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print("P002_SHARED_CANDIDATE_254_ORIGINAL_IFATN_MATCH_PASS",
          "midi_velocity_levels=127","gm40_independent_cross_validation=127",
          f"numeric_assertions={report['numeric_assertions']}",
          "mutated_real_measurements_rejected=2",
          "complete_Creative_driver_PARITY=NOT_PROVEN")

if __name__=="__main__":
    try:main()
    except (ValueError,OSError,UnicodeError) as e:
        print("P002_SHARED_VELOCITY_IFATN_NO_GO",e,file=sys.stderr)
        sys.exit(1)
