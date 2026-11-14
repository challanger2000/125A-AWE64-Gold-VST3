#!/usr/bin/env python3
"""P002 strict original Win95 Creative vs independently generated NATIVE REGISTER comparison.

Compares original EMU8K_TRACE raw port events, including same-frame order,
read/write kind, exact values AND precise chip-clock frame. No gain matching,
clock shift, tolerance, sorting, missing events or invented driver equivalence.
This tool does not produce native driver registers or claim soundbank parity.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

MAX_EVENTS=1_000_000
TRACE=re.compile(r'(?:(R) )?([0-9]+) ([0-9A-Fa-f]{1,4}) ([0-9A-Fa-f]{1,4})\Z')


def sha(path:Path)->str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_trace(path:Path):
    if not path.is_file() or path.stat().st_size>80_000_000:
        raise ValueError("missing or oversize original/native EMU8K trace")
    events=[]
    last_frame=-1
    with path.open("r",encoding="ascii",errors="strict") as f:
        for n,line in enumerate(f,1):
            line=line.strip()
            if not line or line.startswith("#"):continue
            m=TRACE.fullmatch(line)
            if m is None:
                raise ValueError(f"invalid register trace syntax at line {n}")
            read,fr,port,value=m.groups()
            frame=int(fr)
            if frame<last_frame or frame>44100*3600:
                raise ValueError(f"non-monotonic original chip clock at line {n}")
            last_frame=frame
            events.append((frame, "READ" if read else "WRITE", int(port,16),int(value,16)))
            if len(events)>MAX_EVENTS:raise ValueError("trace exceeds bounded event budget")
    if not events:raise ValueError("empty EMU8K trace: no P002 evidence")
    return events


def compare(reference, candidate):
    if not reference or not candidate:raise ValueError("no events, NO-GO")
    for n in range(max(len(reference),len(candidate))):
        a=reference[n] if n<len(reference) else None
        b=candidate[n] if n<len(candidate) else None
        if a!=b:
            field=(
                "missing_event" if a is None or b is None
                else next(name for name,aa,bb in zip(
                    ("frame","direction","port","value"),a,b) if aa!=bb)
            )
            return {"status":"FAIL","index":n,"field":field,
                    "original":a,"native":b,
                    "comparison":"strict frame/direction/port/value and order"}
    return {"status":"REGISTER_SEQUENCE_EQUAL","events":len(reference),
            "comparison":"strict frame/direction/port/value and order"}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original",type=Path,required=True)
    p.add_argument("--native",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True)
    a=p.parse_args()
    ref=read_trace(a.original)
    native=read_trace(a.native)
    result=compare(ref,native)
    result.update({
        "schema":"125a-p002-creative-original-register-oracle-v1",
        "source":"86Box-AWE32 original Win95 Creative captured trace",
        "candidate":"native P002 driver output, must have independent production provenance",
        "original_sha256":sha(a.original),
        "native_sha256":sha(a.native),
        "original_event_count":len(ref),
        "native_event_count":len(native),
        "identical_input_midi_rom_bank_and_clock":"MUST_BE_PROVEN_SEPARATELY",
        "native_trace_independently_generated":"MUST_BE_PROVEN_SEPARATELY",
        "driver_semantics_approved":False,
        "full_SBK_SF2_oracle":"NOT RUN",
        "physical_ASIO_Studio_One":"DEFERRED_TO_FINAL_GUI",
    })
    a.out.parent.mkdir(parents=True,exist_ok=True)
    if a.out.exists():raise ValueError("refuse to overwrite oracle evidence")
    a.out.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    if result["status"]!="REGISTER_SEQUENCE_EQUAL":
        print("P002_REGISTER_ORACLE_FAIL",json.dumps(result),file=sys.stderr)
        return 1
    print("P002_REGISTER_SEQUENCE_EQUAL",len(ref),
          "independent_native_generation=NOT_PROVEN_BY_COMPARATOR")
    return 0


if __name__=="__main__":
    try:sys.exit(main())
    except (ValueError,OSError,UnicodeError) as e:
        print("P002_REGISTER_ORACLE_NO_GO",e,file=sys.stderr)
        sys.exit(2)
