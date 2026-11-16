#!/usr/bin/env python3
"""P106 read-only private original x86 LCOD static CFG against P105 V4 callbacks.

Counts syntactically reachable paths from LCOD+40C4 after CCCA helper to
LCOD+4060, the next VTFT helper. Restricts the graph using the *observed*
mode-1 full 32-voice scan call order. This is NOT a captured EFLAGS trace,
timing synthesizer, complete x86 emulator, or PCM release gate.

Never prints or stores Creative binary bytes, CPU per-event values, or timing
vectors. The authenticated original private driver/trace are never in CI.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

LCOD_SHA256 = "bea69aa3492aeb6fc40400fca1cf584d553cbaa973d9011784db25867beed906"
SOURCE = 0x40C4
NEXT_VTFT = 0x4060
HELPER_CALLS = frozenset((0x4060,0x40A2,0x40BF,0x414A,0x4158))
ASSEMBLY = re.compile(r"^\s*([0-9a-f]+):\s+((?:[a-f0-9]{2}\s+)+)\s+([a-z][a-z0-9]*)\s*(.*)$")
HEX = re.compile(r"^(?:0x)?[0-9a-f]+$")
BRANCHES = {0x40C4:"and",0x40C9:"cmp",0x40CB:"ja",0x40DA:"je",
            0x40E4:"je",0x40F1:"jae",0x4107:"jb",0x4120:"jne"}
# A full original 32-voice scan advances directly to the next adjacent
# voice's VTFT call, without immediate release or skipping that voice.
# These are path restrictions from physical helper order, not EFLAGS samples.
# The recorded full-scan next voice is directly eligible, but the V4 CPU
# I/O trace does not contain the actual RAM DWORD at [device+0x18].
# In the native C++23 port it is DeviceState.mode_count (default 32).
# JNE +0x4027 is therefore UNSPECIFIED in the original trace.
OBSERVED_FULL_SCAN = {0x40DA:False,0x4120:True,0x4054:False}
SKIP_PREDICATES_IF_LOW_COUNT = (0x402D,0x403C,0x4042)

def parse_original_x86(code_path:Path):
    if hashlib.sha256(code_path.read_bytes()).hexdigest()!=LCOD_SHA256:
        raise ValueError("original mapped LCOD SHA-256 mismatch")
    p=subprocess.run(["objdump","-D","-b","binary","-m","i386","-M","intel",
                      "--start-address=0x4023","--stop-address=0x416c",
                      str(code_path)],check=True,capture_output=True,text=True)
    decoded={}
    for line in p.stdout.splitlines():
        m=ASSEMBLY.match(line)
        if not m: continue
        addr=int(m.group(1),16)
        raw=bytes.fromhex(m.group(2))
        operand=m.group(4).strip().split()[0] if m.group(4).strip() else ""
        decoded[addr]=(len(raw),m.group(3),operand)
    if len(decoded)<100 or any(decoded.get(off,(None,""))[1]!=mn
                               for off,mn in BRANCHES.items()):
        raise ValueError("real SHA-locked original x86 opcode anchors differ")
    return decoded

def successors(decoded,addr):
    if addr not in decoded: raise ValueError("missing decoded instruction")
    size,op,operand=decoded[addr]
    nxt=addr+size
    if op=="ret":return []
    if op=="jmp":
        if not HEX.fullmatch(operand):raise ValueError("indirect jump cannot be enumerated")
        return [(int(operand,16),None)]
    if op.startswith("j"):
        if not HEX.fullmatch(operand):raise ValueError("unresolved original Jcc target")
        return [(int(operand,16),(addr,True)),(nxt,(addr,False))]
    return [(nxt,None)]

def enumerate_paths(decoded):
    pending=[(SOURCE,tuple(),frozenset(),0)]
    accepted=[]
    endpoints=Counter()
    count=0
    while pending:
        addr,branches,seen,instructions=pending.pop()
        count+=1
        if count>500000: raise ValueError("unbounded original x86 CFG")
        if addr in HELPER_CALLS:
            endpoints[addr]+=1
            if addr==NEXT_VTFT: accepted.append((branches,instructions))
            continue
        if addr in seen:
            endpoints[-1]+=1
            continue
        if addr not in decoded:
            endpoints[-2]+=1
            continue
        for nxt,decision in successors(decoded,addr):
            if not 0x4023<=nxt<=0x416C:
                endpoints[-3]+=1
                continue
            pending.append((nxt,branches+((decision,) if decision else ()),
                            seen|{addr},instructions+1))
    if not accepted:raise ValueError("no route to next original VTFT")
    return accepted,endpoints

def constrained_paths(all_paths,mode_count=None):
    """Restrict to a witnessed next-adjacent-voice full-scan transition.

    mode_count=None: source I/O trace cannot tell +0x4027 taken/not taken.
    mode_count=32: native-port hypothesis [device+0x18]>2, so JNE taken.
    mode_count=2: converse hypothesis, needing all three no-skip checks.
    Neither alternative claims the V4 guest RAM value was captured.
    """
    if mode_count not in (None,2,32):
        raise ValueError("only explicitly supported counterfactual modes")
    possible=[]
    for decisions,instruction_count in all_paths:
        values=dict(decisions)
        if not all(values.get(pc)==taken for pc,taken in OBSERVED_FULL_SCAN.items()):
            continue
        # When JNE +4027 is taken the no-skip checks are not executed.
        high=values.get(0x4027)
        if high is None:raise ValueError("missing original +4027 branch")
        if mode_count==32 and high is not True:continue
        if mode_count==2 and high is not False:continue
        if not high and any(values.get(pc) is not False
                            for pc in SKIP_PREDICATES_IF_LOW_COUNT):
            continue
        possible.append((decisions,instruction_count))
    expected={None:32,2:16,32:16}[mode_count]
    if len(possible)!=expected:
        raise ValueError(f"expected {expected} x86 routes for mode hypothesis")
    dynamic={pc for path,_ in possible for pc,_ in path}
    dynamic-=set(OBSERVED_FULL_SCAN)|set(SKIP_PREDICATES_IF_LOW_COUNT)
    if mode_count is not None:dynamic.discard(0x4027)
    expected_dynamic={0x40CB,0x40E4,0x40F1,0x4107}
    if dynamic!=(expected_dynamic|({0x4027} if mode_count is None else set())):
        raise ValueError("unexpected unresolved conditional sites")
    expected_lengths={None:set(range(32,38))|set(range(40,46)),
                      2:set(range(40,46)),32:set(range(32,38))}
    if {n for _,n in possible}!=expected_lengths[mode_count]:
        raise ValueError("original constrained instruction-count range changed")
    return possible,dynamic

def synthetic_selftest():
    # Entirely fabricated x86 pseudoinstruction graph; no Creative code.
    toy={0x40C4:(2,"jne","40c8"),0x40C6:(1,"nop",""),
         0x40C7:(2,"jmp","4060"),0x40C8:(2,"jmp","4060"),
         0x4060:(5,"call","0000")}
    ok,_=enumerate_paths(toy)
    assert len(ok)==2 and {n for _,n in ok}=={2,3}
    assert {branch[0][1] for branch,_ in ok}=={True,False}
    corrupted=dict(toy)
    corrupted[0x40C4]=(2,"jne","deadbeef")
    short,exits=enumerate_paths(corrupted)
    assert len(short)==1 and exits[-3]>0, "bad jump mutation survived"
    print("P106_SYNTHETIC_X86_PATH_NEGATIVE_CONTROLS_PASS")

def inspect_original(code_path,original_trace=None):
    decoded=parse_original_x86(code_path)
    all_paths,endpoints=enumerate_paths(decoded)
    possible,remaining=constrained_paths(all_paths,mode_count=None)
    native32,native32_dynamic=constrained_paths(all_paths,mode_count=32)
    lowcount,_=constrained_paths(all_paths,mode_count=2)
    if len(all_paths)!=48:raise ValueError("original CFG route count changed")
    if len({(tuple(path),n) for path,n in all_paths})!=48:
        raise ValueError("duplicate CFG path escaped")
    report={
      "schema":"125A-P106-ORIGINAL-CCCA-VTFT-STATIC-X86-CFG",
      "original_lcod_sha256_verified":True,
      "unconstrained_static_routes_to_next_vtft":len(all_paths),
      "routes_after_observed_fullscan_without_original_mode_count":len(possible),
      "unresolved_original_conditional_sites":[hex(x) for x in sorted(remaining)],
      "routes_if_native_mode_count_32":len(native32),
      "conditional_sites_if_native_mode_count_32":[hex(x) for x in sorted(native32_dynamic)],
      "instruction_counts_if_native_mode_count_32":sorted(set(n for _,n in native32)),
      "routes_if_original_mode_count_at_most_2":len(lowcount),
      "instruction_counts_if_low_mode_count":sorted(set(n for _,n in lowcount)),
      "original_device_mode_count_captured":False,
      "observed_dynamic_eflags_recorded":False,
      "original_cpu_cycle_cost_reconstructed":False,
      "autonomous_vst3_clock_approved":False
    }
    if original_trace:
        from p105_original_stereo_dynarec_private import (
            SOURCE_SHA, extract_original_runs, summarize
        )
        # SHA lock *before* reading any owner data.
        if hashlib.sha256(original_trace.read_bytes()).hexdigest()!=SOURCE_SHA:
            raise ValueError("private original CPU V4 SHA-256 mismatch")
        stats=summarize(extract_original_runs(original_trace))
        if (stats["authentic_original_stereo_allocations"]!=25 or
            stats["identical_last_full_scans"]!=9 or
            stats["source_variable_ccca_to_vtft_positions"]!=8):
            raise ValueError("original V4 stereo source changed")
        report["original_stereo_allocations"]=25
        report["original_identical_full_scans"]=9
        report["original_variable_ccca_to_vtft_sites"]=8
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-lcod",type=Path)
    p.add_argument("--private-v4-trace",type=Path)
    p.add_argument("--selftest",action="store_true")
    a=p.parse_args()
    if a.selftest:synthetic_selftest()
    if a.private_v4_trace and not a.private_lcod:
        p.error("--private-v4-trace requires --private-lcod")
    if a.private_lcod:
        print("P106_ORIGINAL_X86_STATIC_CFG_PASS "+json.dumps(
            inspect_original(a.private_lcod,a.private_v4_trace),sort_keys=True))
    elif not a.selftest:p.error("supply --selftest or --private-lcod")
if __name__=="__main__":
    main()
