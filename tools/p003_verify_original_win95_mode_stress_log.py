#!/usr/bin/env python3
"""P003 Original Win95 Creative 3-bank stress guest log, strict validator.

This attests ONLY that original AWEMAN32.AWE_SEND_MIDI accepted the exact
bounded stress sequence and Win95 guest finished. NEVER infer original
mode1/mode2 selection or audio parity from API success: only genuine
86Box V4 hardware CPU/chip traces with SHA-locked original VXD can do that.
"""
from __future__ import annotations
import argparse
import re
from pathlib import Path
import sys

HEADER="P003_ORIGINAL_WIN95_ALLOCATOR_MODE_STRESS_V1"
END="P003_ORIGINAL_WIN95_ALLOCATOR_STRESS_COMPLETED"
PER_BANK=16*(3*4+36+36+3) # 1392 original Creative AWE_SEND_MIDI calls
EXPECTED_KEYS=[
    "AWE_SEND_MIDI_OPCODE", "PROGRAMS_PER_BANK",
    "CHANNELS_PER_PROGRAM", "OVERLAPPING_NOTES_PER_PROGRAM",
    "GUEST_START_TICK_MS",
]
FINISH_KEYS=["ORIGINAL_MIDI_CALLS_TOTAL","ORIGINAL_MIDI_CALL_ERRORS",
             "ORIGINAL_BANK_PHASES_COMPLETED","GUEST_END_TICK_MS"]
WORD=re.compile(r"([A-Z][A-Z0-9_]*)=(0x[0-9A-F]{8})\Z")

def validate_original_stress_log(raw:bytes)->dict:
    if len(raw)>8192 or not raw or b"\x00" in raw:
        raise ValueError("P003 Win95 mode guest log missing/oversized/binary")
    try:
        lines=raw.decode("ascii").split("\r\n")
    except UnicodeError as e:
        raise ValueError("Non-ASCII original mode log") from e
    if lines[-1]!="":
        raise ValueError("Missing final original Win95 CRLF")
    lines.pop()
    if len(lines)!=20 or lines[0]!=HEADER or lines[-1]!=END:
        raise ValueError("Incomplete/duplicated original 3-bank mode probe")
    at=1
    def key(expected:str)->int:
        nonlocal at
        m=WORD.fullmatch(lines[at])
        if not m or m.group(1)!=expected:
            raise ValueError("Original mode log missing/reordered "+expected)
        at+=1
        return int(m.group(2),16)
    head={k:key(k) for k in EXPECTED_KEYS}
    if (head["AWE_SEND_MIDI_OPCODE"]!=401 or
        head["PROGRAMS_PER_BANK"]!=16 or
        head["CHANNELS_PER_PROGRAM"]!=3 or
        head["OVERLAPPING_NOTES_PER_PROGRAM"]!=36):
        raise ValueError("Original guest stress program parameters changed")
    bank_calls={}
    for bank in range(3):
        if key("BANK_PHASE_BEGIN")!=bank:
            raise ValueError("Original Win95 mode bank-phase order changed")
        n=key(f"BANK{bank}_ORIGINAL_CREATIVE_MIDI_CALLS")
        if n!=PER_BANK:
            raise ValueError(f"Original Win95 bank{bank} sent wrong MIDI count")
        if key("BANK_PHASE_END")!=bank:
            raise ValueError("Original Win95 mode bank-phase end mismatch")
        bank_calls[str(bank)]=n
    tail={k:key(k) for k in FINISH_KEYS}
    if (tail["ORIGINAL_MIDI_CALLS_TOTAL"]!=PER_BANK*3 or
        tail["ORIGINAL_MIDI_CALL_ERRORS"]!=0 or
        tail["ORIGINAL_BANK_PHASES_COMPLETED"]!=3 or
        tail["GUEST_END_TICK_MS"]<head["GUEST_START_TICK_MS"]):
        raise ValueError("Original Creative MIDI API stress failed or incomplete")
    if at!=len(lines)-1:
        raise ValueError("Unknown extra data in original stress guest log")
    return {
        "schema":"125A-P003-ORIGINAL-CREATIVE-WIN95-3-BANK-MODE-STRESS-LOG-V1",
        "original_creative_AWE_SEND_MIDI_opcode":401,
        "source_stress_programs_per_bank":16,
        "source_overlapping_notes_per_program":36,
        "original_midi_calls_by_bank":bank_calls,
        "original_win95_midi_send_calls":PER_BANK*3,
        "original_win95_midi_api_failures":0,
        "original_win95_guest_start_tick_ms":head["GUEST_START_TICK_MS"],
        "original_win95_guest_end_tick_ms":tail["GUEST_END_TICK_MS"],
        "source_driver_mode1_executed":"NOT_INFERRED_FROM_MIDI_API",
        "source_driver_mode2_executed":"NOT_INFERRED_FROM_MIDI_API",
        "independent_emu8000_register_evidence_required":True,
        "original_vxd_guest_callstack_evidence_required":True,
        "autonomous_vst3_pcm_parity":"NOT_PROVEN"
    }

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original-win95-mode-log",required=True,type=Path)
    args=p.parse_args()
    try:
        report=validate_original_stress_log(args.original_win95_mode_log.read_bytes())
        print("P003_ORIGINAL_CREATIVE_MODE_STRESS_SUBMISSION_VALIDATED",
              report["original_win95_midi_send_calls"],
              "hardware_source_modes=NOT_YET_VERIFIED")
        return 0
    except (OSError,ValueError) as exc:
        print("P003_ORIGINAL_CREATIVE_MODE_STRESS_NO_GO:",exc,file=sys.stderr)
        return 2

if __name__=="__main__":
    sys.exit(main())
