#!/usr/bin/env python3
"""P107 read-only verifier for EXECUTED old-86Box-Dynarec guest x86 PCs.

Use only on a PRIVATE run of the SHA-pinned original Win95/Creative VM
with the publicly built P107 instrumented old Dynarec. Never upload
Original-*.trace, Win95 image, original driver, ROM or SF2 to GitHub.

The sampler logs instruction PC and EBP from actually executed JIT code.
This verifier derives Jcc taken/not-taken ONLY when the NEXT recorded executed
PC is exactly one of the two static branch successors. The first-generation
observer emits a SPARSE sequence; skips are explicitly UNKNOWN, not failures
nor inferred from a guessed EFLAGS value or static instruction decoder.

JIT block-level tsc at the callback is NOT a per-instruction time oracle;
no original CPU cycle/PCM timing validation is claimed by this script.
"""
from __future__ import annotations
from collections import Counter
from dataclasses import dataclass
import argparse
import hashlib
import json
from pathlib import Path
import sys

HEADER="# P107_OLD_DYNAREC_EXECUTED_LCOD_V1"
LCOD_BASE=0xC10151E4
ORIGINAL_SELECTOR=0x28
# Original VxD x86 offsets and targets, verified against original LCOD SHA.
BRANCHES={
    0x4027:(0x4048,0x4029,"JNE_PREFERRED_MODE"),
    0x40CB:(0x40D4,0x40CD,"JA_CCCA_BEFORE_SAMPLE_END"),
    0x40E4:(0x40EB,0x40E6,"JE_PENALTY_ABSENT"),
    0x40F1:(0x40F8,0x40F3,"JAE_PRIOR_VOICE_SCORE"),
    0x4107:(0x4118,0x4109,"JB_KEEP_BEST_PAIR"),
}
SCAN_START=0x40C4
NEXT_VTFT=0x4060
MAX_LINES=2_000_000

@dataclass(frozen=True)
class X86Event:
    seq:int
    tsc:int
    selector:int
    pc:int
    ebp:int
    known:int
    local18:int


def read_trace(path:Path,expected_sha256:str|None=None):
    sha=hashlib.sha256()
    rows=[]
    complete=True
    with path.open("rb") as stream:
        first=stream.readline()
        sha.update(first)
        if not first.decode("ascii").startswith(HEADER):
            raise ValueError("P107 old Dynarec *executed* source header missing")
        for raw in stream:
            sha.update(raw)
            s=raw.decode("ascii").strip()
            if not s:continue
            if s.startswith("# P107_EVENT_CAP_REACHED"):
                complete=False
                continue
            if s.startswith("#"):
                raise ValueError("unknown original execution footer/header")
            f=s.split()
            if len(f)!=8 or f[0]!="I":
                raise ValueError("wrong original executed-PC record shape")
            _,seq,tsc,cs,pc,ebp,known,local18=f
            event=X86Event(int(seq,10),int(tsc,10),int(cs,16),
                           int(pc,16),int(ebp,16),int(known,10),int(local18,16))
            if event.seq!=len(rows):
                raise ValueError("missing/duplicate guest x86 instruction")
            if event.selector!=ORIGINAL_SELECTOR:
                raise ValueError("wrong original guest CS selector")
            if not LCOD_BASE+0x4023<=event.pc<=LCOD_BASE+0x4125:
                raise ValueError("foreign guest x86 instruction outside LCOD window")
            if event.known not in (0,1):
                raise ValueError("invalid local-stack availability flag")
            if event.known and event.pc!=LCOD_BASE+0x4027:
                raise ValueError("stack local was read outside the only intended JNE")
            if event.known==0 and event.local18!=0xffffffff:
                raise ValueError("unknown stack local spuriously assigned a value")
            if rows and event.tsc<rows[-1].tsc:
                raise ValueError("virtual guest tsc regressed")
            if len(rows)>=MAX_LINES:
                raise ValueError("execution trace violates agreed cap")
            rows.append(event)
    digest=sha.hexdigest()
    if expected_sha256 and digest!=expected_sha256:
        raise ValueError("original private P107 trace SHA-256 mismatch")
    if not rows:raise ValueError("missing executed source guest instructions")
    if not complete:raise ValueError("truncated: recorder hit event cap")
    return rows,digest


def verify_paths(rows:list[X86Event], allow_sparse:bool=False) -> dict:
    branches=Counter()
    outcomes=Counter()
    ambiguous=Counter()
    ambiguous_stack=Counter()
    unknown_stack=0
    mode_observations=Counter()
    scanned_windows=0
    scan_branches=[]
    # The *next* executed PC, not EFLAGS captured during codegen, decides
    # whether a branch was genuinely taken at runtime.
    for a,b in zip(rows,rows[1:]):
        off=a.pc-LCOD_BASE
        if off==SCAN_START:
            scan_branches=[]
        if off in BRANCHES:
            taken_target,fallthrough,label=BRANCHES[off]
            nxt=b.pc-LCOD_BASE
            if nxt not in (taken_target,fallthrough):
                if not allow_sparse:
                    raise ValueError(f"dynamic branch at +{off:x} next PC invalid")
                # Sparse JIT callbacks: a skipped callback need not represent
                # a Jcc target. Never invent a taken/not-taken outcome.
                ambiguous[label]+=1
                if off==0x4027 and a.known:
                    ambiguous_stack["local_observed_but_jcc_unproven"]+=1
                continue
            taken=nxt==taken_target
            branches[label]+=1
            outcomes[(label,"taken" if taken else "not_taken")]+=1
            scan_branches.append((off,taken))
            if off==0x4027:
                if a.known:
                    mode_observations["nonzero" if a.local18!=0 else "zero"]+=1
                    if taken != (a.local18!=0):
                        raise ValueError("original JNE disagrees with guest EBP-0x18")
                else:
                    unknown_stack+=1
        if b.pc-LCOD_BASE==NEXT_VTFT and scan_branches:
            scanned_windows+=1
            scan_branches=[]
    if not branches:raise ValueError("no dynamic original conditional branches")
    return {
        "schema":"P107_EXECUTED_GUEST_JCC_CAPTURE_PRIVATE_V1",
        "physical_x86_guest_instructions_observed":len(rows),
        "original_branch_checks_with_direct_successor":sum(branches.values()),
        "ambiguous_jcc_successors_not_assigned":sum(ambiguous.values()),
        "ambiguous_jcc_by_branch":dict(ambiguous),
        "local_observed_without_proven_successor":ambiguous_stack[
            "local_observed_but_jcc_unproven"],
        "actual_jcc_branch_execution_counts":dict(branches),
        "branch_taken_outcomes":{
            label:{outcome:outcomes[(label,outcome)]
                   for outcome in ("taken","not_taken")}
            for label in set(branches)
        },
        "observed_guest_local18_nonzero":mode_observations["nonzero"],
        "observed_guest_local18_zero":mode_observations["zero"],
        "guest_local18_unavailable":unknown_stack,
        "ccca_to_next_vtft_windows_with_observed_x86_path":scanned_windows,
        "per_instruction_tsc_accuracy":"NOT_VERIFIED_BLOCK_LEVEL_ONLY",
        "original_vst3_pcm_parity":False,
        "complete_original_86box_cpu_scheduler":False,
        "complete_executed_instruction_stream":False,
        "jcc_claim_scope":"DIRECT_ADJACENT_OBSERVER_SUCCESSORS_ONLY",
    }


def synthetic_selftest():
    def ev(n,off,local=None,tsc=8):
        return X86Event(n,tsc,ORIGINAL_SELECTOR,LCOD_BASE+off,0x12340000,
                        0 if local is None else 1,
                        0xffffffff if local is None else local)
    # A real executed-next-PC test, not a static-path prediction.
    good=[ev(0,0x4027,1),ev(1,0x4048),
          ev(2,0x40C4),ev(3,0x40CB),ev(4,0x40D4),
          ev(5,0x40E4),ev(6,0x40EB),
          ev(7,0x40F1),ev(8,0x40F8),
          ev(9,0x4107),ev(10,0x4118),
          ev(11,0x4023),ev(12,0x4027,0),ev(13,0x4029),
          ev(14,0x405e),ev(15,0x4060)]
    summary=verify_paths(good)
    assert summary["original_branch_checks_with_direct_successor"]==6
    assert summary["ambiguous_jcc_successors_not_assigned"]==0
    assert summary["observed_guest_local18_nonzero"]==1
    assert summary["observed_guest_local18_zero"]==1
    assert summary["ccca_to_next_vtft_windows_with_observed_x86_path"]==1
    bad=list(good)
    bad[1]=ev(1,0x4029)
    try:verify_paths(bad)
    except ValueError as exc:
        assert "EBP-0x18" in str(exc)
    else:raise AssertionError("JNE false branch with nonzero local escaped")
    bad=list(good)
    bad[4]=ev(4,0x40E6)
    try:verify_paths(bad)
    except ValueError as exc:
        assert "next PC invalid" in str(exc)
    else:raise AssertionError("non-branch successor spoof escaped")
    partial=verify_paths(bad,allow_sparse=True)
    assert partial["ambiguous_jcc_successors_not_assigned"]==1
    assert partial["ambiguous_jcc_by_branch"]["JA_CCCA_BEFORE_SAMPLE_END"]==1
    assert partial["original_branch_checks_with_direct_successor"]==5
    # Run a full file-parser negative test without the owner's data.
    from io import BytesIO
    def parse_test_bytes(lines):
        from tempfile import TemporaryDirectory
        with TemporaryDirectory() as d:
            p=Path(d)/"fictional.trace"
            p.write_bytes((HEADER+" synthetic_only\n"+"\n".join(lines)+"\n").encode())
            return read_trace(p)
    lines=[
        f"I {e.seq} {e.tsc} {e.selector:04X} {e.pc:08X} {e.ebp:08X} {e.known} {e.local18:08X}"
        for e in good
    ]
    parsed,_=parse_test_bytes(lines)
    assert len(parsed)==len(good)
    for i,badline in [(0,lines[0].replace("I 0 ","I 1 ",1)),
                      (4,lines[4].replace(" 0028 "," 0030 "))]:
        altered=lines.copy()
        altered[i]=badline
        try:parse_test_bytes(altered)
        except ValueError:pass
        else:raise AssertionError("sequence/selector mutation survived")
    print("P107_EXECUTED_X86_JCC_SYNTHETIC_NEGATIVE_CONTROLS_PASS")


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--executed-x86-trace",type=Path)
    p.add_argument("--sha256")
    p.add_argument("--selftest",action="store_true")
    args=p.parse_args()
    if args.selftest:synthetic_selftest()
    if args.executed_x86_trace:
        rows,sha=read_trace(args.executed_x86_trace,args.sha256)
        result=verify_paths(rows,allow_sparse=True)
        result["executed_trace_sha256"]=sha
        print("P107_PRIVATE_ORIGINAL_EXECUTED_JCC_PARTIAL_ANALYSIS "+
              json.dumps(result,sort_keys=True))
    elif not args.selftest:
        p.error("provide --selftest or --executed-x86-trace")


if __name__=="__main__":
    main()
