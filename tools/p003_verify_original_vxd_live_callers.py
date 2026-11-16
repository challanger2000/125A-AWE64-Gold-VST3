#!/usr/bin/env python3
"""P003 PRIVATE original Win95 x86 dynamic I/O caller verifier.

Requires real V4 guest-EBP/return-address trace and SHA-pinned SBAWE.VXD.
Derives an ORIGINAL-SESSION-specific LCOD load bias by matching the 12
verified physical I/O instruction EIPs. Never hardcodes prior Win95 load
bias (0xC10151E5). For each directly recorded return EIP, verifies the
actual original CALL rel32 that targets that *specific* I/O helper.
A guessed stack frame, a raw E8 data byte, or an unobserved call site
NEVER qualifies as a dynamically observed original CPU caller.

This is evidence reconstruction, not a software port of Creative binaries.
No proprietary binary, original ROM, SF2 or audio is published here.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import struct
import sys

from p003_join_original_guest_cpu_chip_clocks import guest_events, sha256
from p003_private_vxd_static_callers import (
    PINNED_VXD_SHA256, OPCODE_ANCHORS, EMU8K_IO_HELPERS, original_lcod)

UNKNOWN=0xffffffff

def io_helper_for_anchor(offset:int)->str:
    # Each hardware IN/OUT opcode is in one of six original helper
    # stack-frame functions and has a known function entry. This does
    # NOT identify its higher-level MIDI or voice allocator callers.
    pairs=sorted(EMU8K_IO_HELPERS.items(),key=lambda t:t[1])
    prior=None
    for name,at in pairs:
        if offset < at:
            break
        prior=name
    if prior is None:
        raise ValueError("Known original I/O anchor precedes all helpers")
    return prior

def choose_lcod_bias(eips:Counter)->tuple[int,int,int]:
    """Find a single global session loading bias, or FAIL on ambiguity."""
    if not eips:
        raise ValueError("No actual original guest x86 I/O EIP events")
    candidates={(eip-anchor)&0xffffffff for eip in eips
                 for anchor in OPCODE_ANCHORS}
    scores=[]
    for bias in candidates:
        support=sum(eips.get((bias+off)&0xffffffff,0)
                    for off in OPCODE_ANCHORS)
        if support:
            scores.append((support,bias))
    scores.sort(reverse=True)
    top,bias=scores[0]
    runner_up=scores[1][0] if len(scores)>1 else 0
    total=sum(eips.values())
    if top*1000<total*980 or runner_up*100 >= top*95:
        raise ValueError("Ambiguous or low-coverage ORIGINAL LCOD load bias")
    return bias,top,runner_up

def validate_direct_return(code:bytes,bias:int,
                           physical_eip:int,return_eip:int)->tuple[str,int]|None:
    """A live return address counts only if original E8 rel32 targets
    the exact helper inferred from the *executed* I/O opcode EIP.
    """
    anchor=(physical_eip-bias)&0xffffffff
    if anchor not in OPCODE_ANCHORS:
        return None
    if return_eip==UNKNOWN:
        return None
    offset=(return_eip-bias)&0xffffffff
    if offset<5 or offset>=len(code):
        return None
    call=offset-5
    if code[call]!=0xe8:
        return None
    destination=offset+struct.unpack_from("<i",code,call+1)[0]
    helper=io_helper_for_anchor(anchor)
    if destination!=EMU8K_IO_HELPERS[helper]:
        return None
    return helper,call

def audit(vxd_bytes:bytes,lines)->dict:
    code=original_lcod(vxd_bytes)
    # Stream 816K+ events without retaining source bytes or full
    # original MIDI/register histories in the public repository.
    eips=Counter()
    event_eips=Counter()
    snapshots=Counter()
    ret_pairs=Counter()
    no_return=0
    seen=0
    for direction,port,value,tsc,cycles,cs,eip,stack in guest_events(iter(lines)):
        if len(stack)!=9:
            raise ValueError("Original V4 guest-CPU stack trace REQUIRED")
        if eip<0 or eip>0xffffffff:
            raise ValueError("Invalid original CPU EIP")
        eips[eip]+=1
        ret0=stack[1]
        if ret0==UNKNOWN:
            no_return+=1
        else:
            ret_pairs[(eip,ret0)]+=1
        seen+=1
    if not seen:
        raise ValueError("Guest trace contains no physical EMU8000 I/O")
    bias,matched,runner=choose_lcod_bias(eips)
    good=Counter()
    unresolved=0
    for (eip,ret0),amount in ret_pairs.items():
        direct=validate_direct_return(code,bias,eip,ret0)
        if direct is None:
            unresolved+=amount
        else:
            helper,offset=direct
            good[(helper,offset)]+=amount
    original_verified=sum(good.values())
    return {
        "schema":"125A-P003-ORIGINAL-WIN95-GUEST-VXD-LIVE-CALLER-V4",
        "pinned_original_vxd_sha256":PINNED_VXD_SHA256,
        "actual_guest_cpu_events":seen,
        "original_cpu_io_opcode_events_with_session_bias":matched,
        "original_cpu_io_other_eips":seen-matched,
        "session_lcod_load_bias_derived_hex":f"0x{bias:08X}",
        "second_best_load_bias_support":runner,
        "stack_return_unknown":no_return,
        "original_direct_e8_call_and_target_verified_events":original_verified,
        "original_stack_hints_not_verified":unresolved,
        "original_live_direct_callers":[
            {"helper":helper,"original_lcod_call_offset_hex":f"0x{off:04X}",
             "physical_io_event_count":count}
            for (helper,off),count in sorted(good.items())],
        "original_instruction_boundary_verified":
            "E8 destination bytes verified only at observed return EIP - 5",
        "stack_older_frames_and_midi_arguments":
            "Captured in V4 guest CPU trace; not yet verified as NoteOn arguments",
        "full_cpu_instruction_trace":"NOT_CAPTURED",
        "original_emu8000_core_modified":False,
        "autonomous_vst3_pcm_parity":"NOT_PROVEN",
        "notes":"Dynamic physical I/O call edges ONLY. This does not prove caller's full MIDI control flow, instruction execution costs, or ASIO/DAW parity."
    }

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-original-vxd",type=Path,required=True)
    p.add_argument("--guest-cpu-v4-trace",type=Path,required=True)
    p.add_argument("--private-json",type=Path,required=True)
    args=p.parse_args()
    try:
        if args.private_json.exists() or args.private_json.resolve() in (
                args.private_original_vxd.resolve(),
                args.guest_cpu_v4_trace.resolve()):
            raise ValueError("Output must be new, outside input files")
        raw=args.private_original_vxd.read_bytes()
        with args.guest_cpu_v4_trace.open(encoding="utf-8") as f:
            report=audit(raw,f)
        report["owner_guest_cpu_v4_trace_sha256"]=sha256(args.guest_cpu_v4_trace)
        with args.private_json.open("x",encoding="utf-8") as f:
            json.dump(report,f,sort_keys=True,indent=2)
            f.write("\n")
        print("P003_REAL_GUEST_VXD_CALLERS",
              report["original_direct_e8_call_and_target_verified_events"],
              "/",report["actual_guest_cpu_events"],
              "session_lcod_bias=",report["session_lcod_load_bias_derived_hex"],
              "autonomous_pcm_parity=NOT_PROVEN")
        return 0
    except (OSError,ValueError,struct.error,UnicodeError) as e:
        print("P003_REAL_VXD_CALLERS_NO_GO:",str(e),file=sys.stderr)
        return 2

if __name__=="__main__":
    sys.exit(main())
