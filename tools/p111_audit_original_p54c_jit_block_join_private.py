#!/usr/bin/env python3
"""P111 strict private original JIT generation/execution provenance.

C: compilation started, F: successful compilation finished,
R: actual original block code() called, D: returned.
Compiled-only blocks count as NOT EXECUTED. Runtime block TSC boundaries are
not original instruction TSC or autonomous VST3 PCM proof.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys
import tempfile

HEADER=b"# P111_ORIGINAL_P54C_COMPILE_TO_EXECUTE_JIT_JOIN_V1 "
LCOD_BASE=0xC10151E4
MAX_EVENTS=1000000

def parse_line(raw:bytes):
    try:
        f=raw.decode("ascii","strict").split()
        if not f or f[0] not in ("C","F","R","D"):
            raise ValueError("P111 unknown event kind")
        if len(f)!=(6 if f[0]=="C" else 7):
            raise ValueError("P111 event field count invalid")
        kind=f[0]
        seq=int(f[1],10)
        epoch=int(f[2],10)
        slot=int(f[3],10)
        pc=int(f[4],16)
        phys=int(f[5],16)
        value=None if kind=="C" else int(f[6],10)
    except (UnicodeError,ValueError,OverflowError) as exc:
        raise ValueError("P111 malformed original block provenance event") from exc
    if not (0<seq<=MAX_EVENTS and 0<epoch<=MAX_EVENTS and
            0<=slot<1048576 and LCOD_BASE+0x3f00<=pc<=LCOD_BASE+0x4200 and
            0<=phys<=0xffffffff):
        raise ValueError("P111 source-original generation/slot/address invalid")
    if kind=="F" and not -10000000<=value<=10000000:
        raise ValueError("P111 translator booked cycles invalid")
    if kind in ("R","D") and not 0<=value<=0xffffffffffffffff:
        raise ValueError("P111 original CPU block boundary TSC invalid")
    return kind,seq,epoch,slot,pc,phys,value

def audit(path:Path,expected_sha256:str|None=None,min_runs:int=1)->dict:
    if not path.is_file() or not 10<path.stat().st_size<=120000000:
        raise ValueError("original private P111 generation trace absent/oversized")
    digest=hashlib.sha256()
    generations={}
    slots={}
    active={}
    totals=Counter()
    delta=0
    last_epoch=last_seq=0
    with path.open("rb") as stream:
        line=stream.readline()
        digest.update(line)
        if not line.startswith(HEADER):
            raise ValueError("P111 original compilation/runtime header missing")
        for raw in stream:
            digest.update(raw)
            if raw.startswith(b"# P111_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM"):
                raise ValueError("P111 original generation event cap reached")
            kind,seq,epoch,slot,pc,phys,value=parse_line(raw)
            if seq!=last_seq+1:
                raise ValueError("P111 original event sequence missing or duplicated")
            last_seq=seq
            key=(slot,pc,phys)
            if kind=="C":
                if epoch<=last_epoch:
                    raise ValueError("P111 original generation epoch reused/nonmonotonic")
                if slot in active:
                    raise ValueError("P111 original slot recompiled while executing")
                last_epoch=epoch
                slots[slot]=epoch
                generations[epoch]={"key":key,"finished":False,"runs":0}
                totals["compiled_started"]+=1
                continue
            if epoch not in generations:
                raise ValueError("P111 runtime/finish without original compilation")
            g=generations[epoch]
            if slots.get(slot)!=epoch or g["key"]!=key:
                raise ValueError("P111 stale or reused JIT block generation accepted")
            if kind=="F":
                if g["finished"] or g["runs"]:
                    raise ValueError("P111 original compilation finish duplicate")
                g["finished"]=True
                totals["finished"]+=1
                totals["compiled_cycle_sum"]+=value
            elif kind=="R":
                if not g["finished"]:
                    raise ValueError("P111 original block executed before compile finish")
                if slot in active:
                    raise ValueError("P111 block runtime nested without return")
                active[slot]=(epoch,value)
                g["runs"]+=1
                totals["runtime_calls"]+=1
            else:
                if slot not in active or active[slot][0]!=epoch:
                    raise ValueError("P111 orphan old Dynarec runtime return")
                start=active.pop(slot)[1]
                if value<start:
                    raise ValueError("P111 block runtime guest TSC regressed")
                delta+=value-start
                totals["runtime_returns"]+=1
    if expected_sha256 and digest.hexdigest()!=expected_sha256:
        raise ValueError("P111 original provenance SHA mismatch")
    if active:
        raise ValueError("P111 missing old Dynarec block runtime return")
    if any(not g["finished"] for g in generations.values()):
        raise ValueError("P111 original JIT compilation not finished")
    if totals["runtime_calls"]<min_runs:
        raise ValueError("P111 insufficient original compiled block executions")
    executed=sum(g["runs"]>0 for g in generations.values())
    return {
      "schema":"P111-ORIGINAL-P54C-COMPILE-RUN-GENERATION-SAFE-PRIVATE-AUDIT",
      "status":"JIT_GENERATION_JOIN_NOT_INSTRUCTION_TIMING",
      "sha256":digest.hexdigest(),
      "original_jit_generations":totals["finished"],
      "original_generations_ever_executed":executed,
      "original_compiled_but_unexecuted":totals["finished"]-executed,
      "real_code_block_invocations":totals["runtime_calls"],
      "real_code_block_returns":totals["runtime_returns"],
      "codegen_booked_cycle_sum":totals["compiled_cycle_sum"],
      "runtime_block_boundary_tsc_delta_sum":delta,
      "original_generation_slot_reuse_verified":True,
      "per_instruction_cpu_tsc_proven":False,
      "pentium_agi_original_causality_proven":False,
      "autonomous_vst3_pcm_parity_proven":False
    }

def selftest():
    header=HEADER+b"SYNTHETIC_NO_ORIGINAL_CREATIVE\n"
    C=b"C 1 5 7 C1019207 0000A000\n"
    F=b"F 2 5 7 C1019207 0000A000 32\n"
    R=b"R 3 5 7 C1019207 0000A000 1000\n"
    D=b"D 4 5 7 C1019207 0000A000 1032\n"
    C2=b"C 5 6 8 C1019207 0000A004\n"
    F2=b"F 6 6 8 C1019207 0000A004 11\n"
    with tempfile.TemporaryDirectory() as t:
        f=Path(t)/"fictional.trace"
        good=header+C+F+R+D+C2+F2
        f.write_bytes(good)
        report=audit(f)
        assert report["real_code_block_invocations"]==1
        assert report["original_compiled_but_unexecuted"]==1
        assert report["runtime_block_boundary_tsc_delta_sum"]==32
        tests=[
          (header+C+F+D.replace(b"D 4 ",b"D 3 "),"orphan old Dynarec"),
          (header+C+R.replace(b"R 3 ",b"R 2 ")+D.replace(b"D 4 ",b"D 3 "),"before compile finish"),
          (header+C+F+R,"missing old Dynarec"),
          (header+C+F+R.replace(b"R 3 ",b"R 7 ")+D,"sequence missing"),
          (header+C+F+R+D.replace(b"1032",b"999"),"TSC regressed"),
          (header+C+F+R+D+C2.replace(b"C 5 6 8 ",b"C 5 5 7 ")+F2,
            "generation epoch reused")
        ]
        for bad,reason in tests:
            f.write_bytes(bad)
            try:audit(f)
            except ValueError as e:
                assert reason in str(e),(reason,str(e))
            else:raise AssertionError("bad provenance passed: "+reason)
        f.write_bytes(good+b"# P111_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n")
        try:audit(f)
        except ValueError as e:assert "cap reached" in str(e)
        else:raise AssertionError("event cap passed")
        f.write_bytes(good)
        try:audit(f,"0"*64)
        except ValueError as e:assert "SHA mismatch" in str(e)
        else:raise AssertionError("wrong SHA accepted")
    print("P111_SYNTHETIC_ORIGINAL_JIT_COMPILE_RUN_GENERATION_AUDITOR_PASS")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-original-p111-trace",type=Path)
    p.add_argument("--sha256")
    p.add_argument("--min-runtime-blocks",type=int,default=1)
    p.add_argument("--selftest",action="store_true")
    a=p.parse_args()
    if a.selftest:selftest()
    if a.private_original_p111_trace:
        result=audit(a.private_original_p111_trace,a.sha256,a.min_runtime_blocks)
        print("P111_PRIVATE_ORIGINAL_GENERATION_AUDIT "+json.dumps(result,sort_keys=True))
    elif not a.selftest:p.error("require --selftest or private original P111 trace")
if __name__=="__main__":main()
