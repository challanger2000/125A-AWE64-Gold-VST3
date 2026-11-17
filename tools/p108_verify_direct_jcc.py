#!/usr/bin/env python3
"""P108 offline cross-validation: original executed x86 lazy CF/ZF versus direct next PC.

Only user-private trace file inputs; no raw row/CPU/Creative byte output.
Require every executed original P107 Jcc I-event to have exactly one direct
P108 predicate B-event, and all Jcc with known next-PC to agree. The formerly
missing successors may then be separately counted as supported by an
independent flag observation, NEVER a fabricated next-PC.

Block-level 86Box TSC is not per-instruction clock; this does not establish
autonomous P003 CPU schedule or bit-exact PCM parity.
"""
from __future__ import annotations
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
import sys
import argparse
import hashlib
import json
import tempfile

from p107_audit_executed_jcc_private import (
    read_trace,X86Event,BRANCHES,LCOD_BASE,ORIGINAL_SELECTOR
)
DIRECT_HEADER="# P108_DIRECT_EXECUTED_X86_JCC_CF_ZF_V1"
MAX_DIRECT=2_000_000

@dataclass(frozen=True)
class Direct:
    seq:int
    tsc:int
    selector:int
    pc:int
    zf:int
    cf:int
    taken:int

def original_branch_from_flags(offset:int,zf:int,cf:int)->int:
    if zf not in (0,1) or cf not in (0,1):raise ValueError("invalid original lazy flags")
    if offset==0x4027:return 1-zf
    if offset==0x40CB:return (1-zf)*(1-cf)
    if offset==0x40E4:return zf
    if offset==0x40F1:return 1-cf
    if offset==0x4107:return cf
    raise ValueError("not one of five authenticated original Jcc branches")

def read_direct(path:Path, expected_sha256:str|None=None):
    events={}
    count=0
    sha=hashlib.sha256()
    incomplete=False
    with path.open("rb") as fh:
        head=fh.readline()
        sha.update(head)
        if not head.decode("ascii").startswith(DIRECT_HEADER):
            raise ValueError("P108 direct-original-lazy-flags file header absent")
        for raw in fh:
            sha.update(raw)
            s=raw.decode("ascii").strip()
            if not s:continue
            if s.startswith("# P108_EVENT_CAP_REACHED"):
                incomplete=True
                continue
            f=s.split()
            if len(f)!=8 or f[0]!="B":
                raise ValueError("unsupported or malformed P108 branch row")
            _,seq,tsc,selector,pc,zf,cf,taken=f
            b=Direct(int(seq),int(tsc),int(selector,16),int(pc,16),
                     int(zf),int(cf),int(taken))
            if b.selector!=ORIGINAL_SELECTOR or b.pc-LCOD_BASE not in BRANCHES:
                raise ValueError("foreign original guest Jcc branch event")
            if b.taken not in (0,1) or b.taken!=original_branch_from_flags(
                b.pc-LCOD_BASE,b.zf,b.cf):
                raise ValueError("P108 Jcc lazy-flag equation mismatch")
            if b.seq in events:raise ValueError("duplicate original Jcc event")
            events[b.seq]=b
            count+=1
            if count>MAX_DIRECT:raise ValueError("P108 event cap exceeded")
    if incomplete:raise ValueError("P108 truncated direct-original-Jcc trace")
    if expected_sha256 and sha.hexdigest()!=expected_sha256:
        raise ValueError("P108 original branch trace SHA-256 mismatch")
    if not events:raise ValueError("P108 no actual original branch executed")
    return events,sha.hexdigest()

def compare_original(rows:list[X86Event],direct:dict[int,Direct]):
    expected=set()
    confirmed=Counter()
    repaired=Counter()
    observed=Counter()
    original_stack_jne=Counter()
    for index,e in enumerate(rows):
        off=e.pc-LCOD_BASE
        if off not in BRANCHES:continue
        expected.add(e.seq)
        if e.seq not in direct:raise ValueError("missing P108 direct flag Jcc event")
        b=direct[e.seq]
        if b.pc!=e.pc or b.tsc!=e.tsc or b.selector!=e.selector:
            raise ValueError("P108 Jcc event not aligned to original P107 I event")
        target,fallthrough,label=BRANCHES[off]
        observed[label]+=1
        # LCOD+4023 compares the guest stack local directly with zero.
        # Require the direct JNE flag to agree with original read-only RAM.
        if off==0x4027:
            if e.known:
                if b.taken!=int(e.local18!=0):
                    raise ValueError("original JNE contradicts guest EBP-0x18")
                original_stack_jne["verified"]+=1
            else:
                original_stack_jne["unavailable"]+=1
        if index+1<len(rows):
            nextoffset=rows[index+1].pc-LCOD_BASE
            if nextoffset in (target,fallthrough):
                taken=int(nextoffset==target)
                if b.taken!=taken:
                    raise ValueError("original directly observed Jcc contradicts lazy flags")
                confirmed[label]+=1
                continue
        repaired[label]+=1
    if set(direct)!=expected:
        raise ValueError("spurious or missing direct original branch event")
    if sum(repaired.values())==0:
        state="COMPLETE_DIRECT_CROSSVALIDATION"
    else:
        state="FLAGS_COVER_MISSING_NEXT_PC_BUT_NO_PER_INSTRUCTION_TIMING"
    return {
        "status":state,
        "executed_jcc_total":sum(observed.values()),
        "branch_conditions_directly_captured":len(direct),
        "next_pc_independently_confirms_flags":sum(confirmed.values()),
        "previously_unknown_next_pc_but_direct_flags_captured":sum(repaired.values()),
        "unobserved_or_unexplained_jcc":0,
        "branch_samples_by_kind":dict(sorted(observed.items())),
        "independent_original_jne_guest_stack_matches":original_stack_jne["verified"],
        "independent_original_jne_guest_stack_unavailable":original_stack_jne["unavailable"],
        "no_unverified_next_pc_fabricated":True,
        "source_guest_instructions":len(rows),
        "per_instruction_tsc_accuracy":"NOT_ESTABLISHED",
        "autonomous_vst3_emu8000_parity":False,
    }

def synthetic():
    # These are invented event sequences; no user's Creative data present.
    def e(n,offset):
        return X86Event(n,100,ORIGINAL_SELECTOR,LCOD_BASE+offset,
                        0x1000,0,0xffffffff)
    rows=[e(0,0x4027),e(1,0x4048),e(2,0x40CB),e(3,0x4107),e(4,0x4118)]
    direct={
       0:Direct(0,100,ORIGINAL_SELECTOR,LCOD_BASE+0x4027,0,0,1),
       2:Direct(2,100,ORIGINAL_SELECTOR,LCOD_BASE+0x40CB,1,0,0),
       3:Direct(3,100,ORIGINAL_SELECTOR,LCOD_BASE+0x4107,0,1,1)
    }
    report=compare_original(rows,direct)
    assert report["executed_jcc_total"]==3
    assert report["next_pc_independently_confirms_flags"]==2
    assert report["previously_unknown_next_pc_but_direct_flags_captured"]==1
    assert report["unobserved_or_unexplained_jcc"]==0
    mutated=dict(direct)
    mutated[3]=Direct(3,100,ORIGINAL_SELECTOR,LCOD_BASE+0x4107,0,0,0)
    try:compare_original(rows,mutated)
    except ValueError as exc:assert "contradicts" in str(exc)
    else:raise AssertionError("wrong lazy flags survived next-PC crosscheck")
    for dropped in (0,2,3):
        mutated=dict(direct)
        del mutated[dropped]
        try:compare_original(rows,mutated)
        except ValueError as exc:assert "missing" in str(exc)
        else:raise AssertionError("missing direct branch record survived")
    for offset in BRANCHES:
        for zf in (0,1):
            for cf in (0,1):
                pred=original_branch_from_flags(offset,zf,cf)
                assert pred in (0,1)
    for offset in (0x4027,0x40CB,0x40E4,0x40F1,0x4107):
        assert original_branch_from_flags(offset,0,0)==(
          {0x4027:1,0x40CB:1,0x40E4:0,0x40F1:1,0x4107:0}[offset])
    for unknown in (0x40ca,0x4108):
        try:original_branch_from_flags(unknown,0,1)
        except ValueError:pass
        else:raise AssertionError("unknown guest Jcc accepted")
    with tempfile.TemporaryDirectory() as temp:
        f=Path(temp)/"fully-fictional-jcc.flags"
        header=DIRECT_HEADER+" no_original_data\n"
        payload=(header+
            "\n".join(
                "B %d %d %04X %08X %d %d %d"%(
                    b.seq,b.tsc,b.selector,b.pc,b.zf,b.cf,b.taken)
                for b in direct.values())+"\n")
        f.write_text(payload,encoding="ascii")
        parsed,_=read_direct(f)
        assert parsed==direct
        # Malformed branch predicate must fail before it is accepted.
        f.write_text(payload.replace("B 0 100","B 0 99",1).replace(
            f"{LCOD_BASE+0x4027:08X} 0 0 1",
            f"{LCOD_BASE+0x4027:08X} 1 0 1",1),encoding="ascii")
        try:read_direct(f)
        except ValueError as exc:assert "equation mismatch" in str(exc)
        else:raise AssertionError("bad JNE predicate survived")
    print("P108_ORIGINAL_FIVE_JCC_DIRECT_FLAGS_SYNTHETIC_CROSSCHECK_PASS")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-p107-instructions",type=Path)
    p.add_argument("--private-p108-branches",type=Path)
    p.add_argument("--p107-sha256")
    p.add_argument("--p108-sha256")
    p.add_argument("--selftest",action="store_true")
    a=p.parse_args()
    if a.selftest:synthetic()
    if a.private_p107_instructions or a.private_p108_branches:
        if not a.private_p107_instructions or not a.private_p108_branches:
            p.error("provide both private original trace paths")
        rows,oldsha=read_trace(a.private_p107_instructions,a.p107_sha256)
        direct,newsha=read_direct(a.private_p108_branches,a.p108_sha256)
        result=compare_original(rows,direct)
        result["p107_instruction_trace_sha256"]=oldsha
        result["p108_direct_branch_trace_sha256"]=newsha
        print("P108_PRIVATE_ORIGINAL_X86_CROSSCHECK "+json.dumps(result,sort_keys=True))
    elif not a.selftest:p.error("use --selftest or both private traces")
if __name__=="__main__":
    main()
