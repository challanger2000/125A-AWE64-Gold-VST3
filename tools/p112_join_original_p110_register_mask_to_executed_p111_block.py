#!/usr/bin/env python3
"""P112: original P110 Pentium translator block-S mask to P111 JIT run join.

Both probes are patched at the SAME original old-x64 86Box block-start:
P111 C before P110 S, with the same PC and selector. Within the common
LCOD interval, check *entire ordered sequences* one-to-one, including
repeated guest PCs; never join by address alone or by synthetic TSC.
Classify whether original carried P54C register masks belonged to JIT
generations that later actually entered code(), or never did.

This proves only SOURCE-TRANSLATOR MASK CARRIAGE for EXECUTED-JIT BLOCK
GENERATIONS. P110 per-OPCODE positive AGI still lacks epoch tagging, and
the initial original interpreter guest execution is outside P111 code().
Original VM/ROM/VxD/SF2/PCM data remain private; redact all raw PCs.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys
import tempfile

from p110_audit_original_p54c_translation_private import (
    audit as audit_p110,parse_row as parse_p110,
    HEADER as HEADER_P110,MAX_ROWS
)
from p111_audit_original_p54c_jit_block_join_private import (
    audit as audit_p111,parse_line as parse_p111,
    HEADER as HEADER_P111,LCOD_BASE
)
FIRST=LCOD_BASE+0x4023
LAST=LCOD_BASE+0x4125

def read_p110_starts(path:Path):
    results=[]
    with path.open("rb") as f:
        if not f.readline().startswith(HEADER_P110):
            raise ValueError("P112 P110 original source header absent")
        for raw in f:
            if raw.startswith(b"#"):raise ValueError("P112 P110 original capped/truncated")
            r=parse_p110(raw)
            if r.kind=="S":
                results.append((r.pc,r.pre_mask))
    return results

def read_p111_compilation_execution(path:Path):
    starts=[]
    counts=Counter()
    with path.open("rb") as f:
        if not f.readline().startswith(HEADER_P111):
            raise ValueError("P112 P111 original source header absent")
        for raw in f:
            if raw.startswith(b"#"):raise ValueError("P112 P111 original capped/truncated")
            kind,seq,epoch,slot,pc,phys,value=parse_p111(raw)
            if kind=="C":
                counts[epoch]=0
                if FIRST<=pc<=LAST:starts.append((epoch,pc))
            elif kind=="R":
                counts[epoch]+=1
    return starts,counts

def join(p110:Path,p111:Path,min_common_starts:int=1):
    # Verify entire files BEFORE extracting/redacting any relation. A
    # damaged trace must never partially contribute positive coverage.
    p110_result=audit_p110(p110,min_translations=1)
    p111_result=audit_p111(p111,min_runs=1)
    mask_starts=read_p110_starts(p110)
    compile_starts,executions=read_p111_compilation_execution(p111)
    if len(mask_starts)!=len(compile_starts):
        raise ValueError("P112 unpaired P110/P111 original compilation-start event counts")
    if len(mask_starts)<min_common_starts:
        raise ValueError("P112 insufficient original matching compiler block starts")
    count=Counter()
    for (pc,mask),(epoch,p111_pc) in zip(mask_starts,compile_starts):
        if pc!=p111_pc:
            raise ValueError("P112 original block start order/PC mismatch")
        ran=executions[epoch]>0
        count["starts"]+=1
        count["executed" if ran else "never_executed"]+=1
        if mask:
            count["carried_mask_executed" if ran else
                  "carried_mask_never_executed"]+=1
        else:
            count["zero_mask_executed" if ran else
                  "zero_mask_never_executed"]+=1
    if count["starts"]!=count["executed"]+count["never_executed"]:
        raise ValueError("P112 original crossprobe count inconsistent")
    return {
      "schema":"P112-SAME-ORIGINAL-COMPILER-START-ORDINAL-AGI-MASK-JIT-JOIN",
      "status":"SOURCE_REGMASK_ORIGINAL_JIT_GENERATION_JOIN_NOT_AGI_CAUSALITY",
      "p110_sha256":p110_result["trace_sha256"],
      "p111_sha256":p111_result["sha256"],
      "matched_original_compile_starts_in_shared_lcod_window":count["starts"],
      "matched_generations_later_actually_jit_executed":count["executed"],
      "matched_generations_never_jit_executed":count["never_executed"],
      "executed_generations_with_original_nonzero_carried_mask":
          count["carried_mask_executed"],
      "unexecuted_generations_with_original_nonzero_carried_mask":
          count["carried_mask_never_executed"],
      "executed_generations_with_zero_carried_mask":count["zero_mask_executed"],
      "unexecuted_generations_with_zero_carried_mask":
          count["zero_mask_never_executed"],
      "scope":"ONLY_BLOCKS_WITH_START_IN_BOTH_P110_P111_WINDOWS",
      "interpreter_first_execution_counted":False,
      "per_opcode_positive_agi_epoch_link_proven":False,
      "91_93_95_original_tsc_causality_proven":False,
      "autonomous_vst3_pcm_parity_proven":False
    }

def selftest():
    with tempfile.TemporaryDirectory() as t:
        root=Path(t)
        p110=root/"synthetic-p110.trace"
        p111=root/"synthetic-p111.trace"
        # Two compilations of the SAME original-shaped PC: one executes
        # with carried mask, the other never executes with zero mask.
        pc=LCOD_BASE+0x4027
        i=LCOD_BASE+0x4023
        p110_data=(HEADER_P110+b"FICTIONAL_ONLY\n"
          +f"S 1 {i:08X} 00000002 0 0 0 0\n".encode()
          +f"O 2 {pc:08X} 75 00000002 00000002 0 0 0 0 0 1 1 1\n".encode()
          +f"S 3 {i:08X} 00000000 0 0 0 0\n".encode()
          +f"O 4 {pc:08X} 75 00000000 00000000 0 0 0 0 0 1 0 0\n".encode())
        p111_data=(HEADER_P111+b"FAKE_NO_GUEST\n"
           +f"C 1 1 7 {i:08X} 00001000\n".encode()
           +f"F 2 1 7 {i:08X} 00001000 3\n".encode()
           +f"R 3 1 7 {i:08X} 00001000 100\n".encode()
           +f"D 4 1 7 {i:08X} 00001000 105\n".encode()
           +f"C 5 2 8 {i:08X} 00001004\n".encode()
           +f"F 6 2 8 {i:08X} 00001004 3\n".encode())
        p110.write_bytes(p110_data)
        p111.write_bytes(p111_data)
        r=join(p110,p111,min_common_starts=2)
        assert r["matched_generations_later_actually_jit_executed"]==1
        assert r["matched_generations_never_jit_executed"]==1
        assert r["executed_generations_with_original_nonzero_carried_mask"]==1
        assert r["unexecuted_generations_with_zero_carried_mask"]==1
        assert r["per_opcode_positive_agi_epoch_link_proven"] is False
        for wrong,reason in (
            (p110_data.replace(f"S 3 {i:08X}".encode(),
                               f"S 3 {i+4:08X}".encode()),"start order/PC mismatch"),
            (p110_data.split(f"S 3".encode())[0],
             "unpaired P110/P111 original compilation-start event counts"),
        ):
            p110.write_bytes(wrong)
            try:join(p110,p111)
            except ValueError as e:assert reason in str(e),(reason,str(e))
            else:raise AssertionError("P112 original counterexample survived")
            p110.write_bytes(p110_data)
        p111.write_bytes(p111_data+b"# P111_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n")
        try:join(p110,p111)
        except ValueError as e:assert "cap reached" in str(e)
        else:raise AssertionError("truncated original join survived")
    print("P112_ORIGINAL_SHARED_COMPILE_ORDER_GENERATION_JOIN_SYNTHETIC_PASS")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-original-p110",type=Path)
    p.add_argument("--private-original-p111",type=Path)
    p.add_argument("--min-common-starts",type=int,default=20)
    p.add_argument("--selftest",action="store_true")
    a=p.parse_args()
    if a.selftest:selftest()
    if a.private_original_p110 or a.private_original_p111:
        if not (a.private_original_p110 and a.private_original_p111):
            p.error("require BOTH original private P110 and P111 traces")
        print("P112_PRIVATE_ORIGINAL_COMPILE_EPOCH_JOIN "+json.dumps(
            join(a.private_original_p110,a.private_original_p111,a.min_common_starts),
            sort_keys=True))
    elif not a.selftest:p.error("require --selftest or both original traces")
if __name__=="__main__":main()
