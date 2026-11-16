#!/usr/bin/env python3
"""P003: private-original V4 mode-0/1/2 execution-hint audit.

Requires the real SHA-pinned Creative SBAWE.VXD and owner's V4 86Box
result ZIP, not public synthetic traces. Checks every physical CPU
callback against the unchanged original EMU8000 trace, derives the
session LCOD base from *executed* x86 I/O EIP anchors, and classifies
all four captured guest EBP return-hint depths by original x86 code
regions. No proprietary driver, ROM, soundfont or audio is published.

These EBP hints are NOT a function-entry/exit CPU trace. In this V4
session the guest return hints have an unexplained one-byte discrepancy
against the architectural E8 rel32 next-EIP. No guessed correction.
Zero return hints does NOT prove a branch was never executed.
"""
from __future__ import annotations

import argparse
from collections import Counter
from hashlib import sha256
import io
import itertools
import json
import struct
from pathlib import Path
import sys
import zipfile

from p003_join_original_guest_cpu_chip_clocks import chip_events, guest_events
from p003_private_vxd_static_callers import original_lcod
from p003_verify_original_vxd_live_callers import choose_lcod_bias

# Half-open original verified LCOD RVA regions. The range classification
# is deliberately independent of MIDI key, voice ID or runtime preset.
LCOD_REGIONS = {
    "dispatcher": (0x3d6f, 0x3ea1),
    "mode_0_three_lane": (0x3ea1, 0x3fe8),
    "mode_1_adjacent_pair": (0x3fe8, 0x416c),
    "mode_2_packed_range": (0x416c, 0x42a7),
    "descriptor_caller_53f5": (0x53f5, 0x5700),
    "note_on": (0x5a00, 0x6072),
    "note_off": (0x6072, 0x6200),
}

def classify_original_hints(
    lcod_bias: int,
    observed_hints: list[Counter[int]],
) -> dict:
    """Classify recorded EBP return *hints*, without 1-byte correction."""
    if len(observed_hints) != 4:
        raise ValueError("Four raw V4 EBP return levels required")
    result = {}
    for name, (begin, end) in LCOD_REGIONS.items():
        depths: Counter[int] = Counter()
        offsets: Counter[int] = Counter()
        for depth, samples in enumerate(observed_hints):
            for address, count in samples.items():
                offset = (address - lcod_bias) & 0xffffffff
                # Keep one-byte-boundary ambiguities unclassified.
                if begin + 1 <= offset < end - 1:
                    if count < 1:
                        raise ValueError("Invalid original return-hint count")
                    depths[depth] += count
                    offsets[offset] += count
        result[name] = {
            "physical_port_events_with_return_hints_in_code_region":
                sum(depths.values()),
            "return_hint_depth_counts": dict(sorted(depths.items())),
            "distinct_return_hint_offsets": len(offsets),
            "top_return_hint_offsets": [
                {"lcod_offset_hex": f"0x{offset:04X}", "events": count}
                for offset, count in offsets.most_common(12)
            ],
            "function_execution_status":
                "HINT_ONLY_NOT_ARCHITECTURAL_CALLSTACK_PROOF",
        }
    return result

# The original SHA-pinned LCOD explicitly calls the mode dispatcher
# at these three sites. 0x5522 is the descriptor-flag-0x04 branch's
# pair-mode call; 0x5560 is its alternate single-mode call.
# Original return EIP should be CALL+5, but V4 hint seems CALL+4.
ORIGINAL_DISPATCH_CALLS={
    "mono_caller_4BB4":(0x4c40,0),
    "descriptor_flag04_branch_pair_mode1":(0x5522,1),
    "descriptor_flag04_branch_single_mode0":(0x5560,0),
}
def classify_original_descriptor_dispatch_calls(
    authentic_lcod:bytes, lcod_bias:int,
    observed_hints:list[Counter[int]],
) -> dict:
    if len(observed_hints)!=4:
        raise ValueError("Original V4 EBP return-depth data required")
    result={}
    for name,(call,mode) in ORIGINAL_DISPATCH_CALLS.items():
        if call+5>len(authentic_lcod) or authentic_lcod[call]!=0xe8:
            raise ValueError("Original dispatcher CALL missing at "+hex(call))
        rel=struct.unpack_from("<i",authentic_lcod,call+1)[0]
        if call+5+rel!=0x3d6f:
            raise ValueError("Original descriptor dispatch CALL wrong target at "+hex(call))
        expected_call_end=(lcod_bias+call+5)&0xffffffff
        observed_minus_one=(lcod_bias+call+4)&0xffffffff
        actual=sum(c.get(expected_call_end,0) for c in observed_hints)
        one_less=sum(c.get(observed_minus_one,0) for c in observed_hints)
        result[name]={
            "original_lcod_call_offset_hex":f"0x{call:04X}",
            "statically_verified_x86_call_target_hex":"0x3D6F",
            "original_dispatch_mode":mode,
            "architectural_next_pc_return_hint_events":actual,
            "last_byte_of_call_return_hint_events_DIAGNOSTIC_ONLY":one_less,
            "one_byte_displacement_cause":"UNKNOWN",
            "full_function_execution_proven":False,
        }
    return result

def file_sha(f) -> str:
    h = sha256()
    while True:
        chunk = f.read(1024 * 1024)
        if not chunk:
            return h.hexdigest()
        h.update(chunk)

def audit(private_vxd: bytes, result_zip: Path) -> dict:
    # Includes original Creative driver SHA256, LE LCOD and 12 genuine
    # hardware opcode anchors. There is no original binary in this repo.
    code=original_lcod(private_vxd)
    with zipfile.ZipFile(result_zip) as z:
        required = ("P003-BERICHT.json", "Original-Register.trace",
                    "Original-CPU-IO.trace")
        if not set(required).issubset(z.namelist()):
            raise ValueError("Missing authentic owner V4 evidence files")
        attested = json.loads(z.read("P003-BERICHT.json"))
        if (attested.get("status") !=
                "SOURCE_CREATIVE_TWO_BANK_API_TRACE_WAV_CPU_IO_CAPTURED" or
            not attested.get("cpu_chip_event_join",{}).get(
                "guest_return_stack_v4_enabled") or
            attested.get("cpu_chip_event_join",{}).get(
                "matched_port_events",0)<1000):
            raise ValueError("Owner V4 CPU/chip attestation missing")
        for item in required[1:]:
            with z.open(item) as f:
                digest = file_sha(f)
            if digest != attested["evidence_sha256"][item]:
                raise ValueError("Private original source SHA256 differs: "+item)

        eips: Counter[int] = Counter()
        returns: list[Counter[int]] = [Counter() for _ in range(4)]
        events = 0
        writes = 0
        byte_reads = 0
        unrecorded_top = 0
        last_tsc = None
        with z.open("Original-Register.trace") as chip_raw, \
             z.open("Original-CPU-IO.trace") as cpu_raw:
            chip = chip_events(io.TextIOWrapper(chip_raw, encoding="ascii"))
            guest = guest_events(io.TextIOWrapper(cpu_raw, encoding="ascii"))
            for original, execution in itertools.zip_longest(chip, guest):
                if original is None or execution is None:
                    raise ValueError("Original 86Box CPU/chip event count differs")
                direction, frame, port, value = original
                cd, cp, cv, tsc, cycles, cs, eip, stack = execution
                if len(stack)!=9:
                    raise ValueError("Legacy V3 trace is not a V4 callstack")
                if cd=="r":
                    # Only the low byte is visible to the byte INB
                    # callback; full EMU8000 16-bit read was captured
                    # independently, never invent equality of the word.
                    if direction!="R" or port!=cp:
                        raise ValueError(f"Byte read port mismatch at {events}")
                    byte_reads += 1
                elif (direction, port, value) != (cd, cp, cv):
                    raise ValueError(f"CPU/chip physical port mismatch at {events}")
                if last_tsc is not None and tsc < last_tsc:
                    raise ValueError("Guest CPU clock runs backward")
                last_tsc = tsc
                events += 1
                if direction=="W":
                    writes += 1
                eips[eip] += 1
                if stack[1]==0xffffffff:
                    unrecorded_top += 1
                for depth, ret in enumerate(stack[1:5]):
                    returns[depth][ret] += 1
        if events!=attested["cpu_chip_event_join"]["matched_port_events"]:
            raise ValueError("Original 816594 physical events not reproduced")
        bias, opcodes, runner = choose_lcod_bias(eips)
        return {
            "schema":"125A-P003-ORIGINAL-GUEST-V4-ALLOCATOR-MODE-HINTS-V1",
            "original_cpu_emu8000_port_events_matched":events,
            "original_port_writes":writes,
            "byte_reads_full_word_value_unverified":byte_reads,
            "original_vxd_sha256":sha256(private_vxd).hexdigest(),
            "original_owner_result_name":result_zip.name,
            "original_owner_result_sha256":file_sha(result_zip.open("rb")),
            "guest_lcod_bias_derived_hex":f"0x{bias:08X}",
            "known_original_opcode_eip_events":opcodes,
            "other_opcode_eip_events":events-opcodes,
            "runner_up_opcode_bias_support":runner,
            "unavailable_top_return_hint_events":unrecorded_top,
            "regions":classify_original_hints(bias,returns),
            "descriptor_dispatch_call_hints":
                classify_original_descriptor_dispatch_calls(code,bias,returns),
            "no_proprietary_original_binary_embedded":True,
            "original_emu8000_dsp_modified":False,
            "source_clocked_pcm_reference_preserved":True,
            "autonomous_vst3_pcm_parity":"NOT_PROVEN",
            "warnings":[
                "V4 stack return addresses are unverified execution hints",
                "Original V4 guest saved-return one-byte discrepancy remains unexplained",
                "Zero matched return hints does not prove unexecuted code",
                "No function entry/exit CPU cycle measurements",
                "No original mode-1/mode-2 MIDI demonstration in this recording",
                "Original descriptor CALL+4 hint is not a proven architectural return EIP",
            ],
        }

def main() -> int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--private-original-vxd",type=Path,required=True)
    ap.add_argument("--owner-v4-zip",type=Path,required=True)
    ap.add_argument("--out-private-json",type=Path,required=True)
    a=ap.parse_args()
    try:
        if a.out_private_json.exists() or a.out_private_json.resolve() in (
                a.private_original_vxd.resolve(),a.owner_v4_zip.resolve()):
            raise ValueError("Refuse to overwrite owner original evidence")
        report=audit(a.private_original_vxd.read_bytes(),a.owner_v4_zip)
        with a.out_private_json.open("x",encoding="utf-8") as f:
            json.dump(report,f,sort_keys=True,indent=2)
            f.write("\n")
        print("P003_V4_ORIGINAL_ALLOCATOR_HINTS_PASS",
              report["original_cpu_emu8000_port_events_matched"],
              "mode0=",report["regions"]["mode_0_three_lane"]
                   ["physical_port_events_with_return_hints_in_code_region"],
              "mode1=",report["regions"]["mode_1_adjacent_pair"]
                   ["physical_port_events_with_return_hints_in_code_region"],
              "mode2=",report["regions"]["mode_2_packed_range"]
                   ["physical_port_events_with_return_hints_in_code_region"],
              "autonomous_pcm=NOT_PROVEN")
        return 0
    except (OSError,ValueError,KeyError,UnicodeError,
            zipfile.BadZipFile) as exc:
        print("P003_V4_ORIGINAL_ALLOCATOR_HINTS_NO_GO:",exc,file=sys.stderr)
        return 2

if __name__=="__main__":
    sys.exit(main())
