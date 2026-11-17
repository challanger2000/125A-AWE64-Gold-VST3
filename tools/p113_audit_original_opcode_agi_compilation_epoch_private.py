#!/usr/bin/env python3
"""P113: independent original P110 per-op AGI -> P111 JIT epoch/run audit.

Requires three COMPLETE original private traces from SAME Win95 86Box run:
P110 O = original Pentium translated opcode AGI counters;
P111 C/F/R/D = original compiled-block epochs and true JIT code() calls;
P113 E = exact original AGI counters + epoch at original opcode translation.
Never estimate executed-instruction TSC from compile-time original AGI.
No original guest RAM, ROM, VxD, individual PC or SoundFont uploaded.
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
    audit as audit_p110,parse_row as parse_p110,HEADER as P110_HEADER
)
from p111_audit_original_p54c_jit_block_join_private import (
    audit as audit_p111,parse_line as parse_p111,HEADER as P111_HEADER,
    LCOD_BASE
)
HEADER=b"# P113_ORIGINAL_P54C_AGI_OPCODE_JIT_EPOCH_V1 "
MAX_RECORDS=500000
WINDOW_LOW=LCOD_BASE+0x4023
WINDOW_HIGH=LCOD_BASE+0x4125

def read_events(path:Path):
    if not path.is_file() or not 25<=path.stat().st_size<=80_000_000:
        raise ValueError("P113 original private epoch trace missing/oversized")
    original_sha=hashlib.sha256()
    events=[]
    with path.open("rb") as inp:
        first=inp.readline()
        original_sha.update(first)
        if not first.startswith(HEADER):
            raise ValueError("P113 original epoch source header missing")
        for raw in inp:
            original_sha.update(raw)
            if raw.startswith(b"#"):
                raise ValueError("P113 incomplete or capped original AGI epoch trace")
            try:
                cells=raw.decode("ascii","strict").split()
                if len(cells)!=7 or cells[0]!="E":
                    raise ValueError("P113 opcode event schema invalid")
                _,seq,epoch,pc,op,checks,positive=cells
                seq=int(seq)
                epoch=int(epoch)
                pc=int(pc,16)
                op=int(op,16)
                checks=int(checks)
                positive=int(positive)
            except (UnicodeError,ValueError,OverflowError) as exc:
                raise ValueError("P113 malformed original opcode epoch event") from exc
            if (seq!=len(events)+1 or not 1<=seq<=MAX_RECORDS or
                not 1<=epoch<=MAX_RECORDS or
                not WINDOW_LOW<=pc<=WINDOW_HIGH or
                not 0<=op<=255 or not 0<=positive<=checks<=16):
                raise ValueError("P113 original AGI opcode sequence/epoch/counters invalid")
            events.append((epoch,pc,op,checks,positive))
    if not events:
        raise ValueError("P113 no original AGI opcode provenance")
    return events,original_sha.hexdigest()

def read_p110_opcodes(path:Path):
    rows=[]
    with path.open("rb") as inp:
        if not inp.readline().startswith(P110_HEADER):
            raise ValueError("P113 original P110 header missing")
        for raw in inp:
            if raw.startswith(b"#"):
                raise ValueError("P113 P110 translator clipped/truncated")
            event=parse_p110(raw)
            if event.kind=="O":
                rows.append((event.pc,event.opcode,event.agi_calls,
                             event.agi_positive))
    return rows

def read_p111_generations(path:Path):
    epochs={}
    with path.open("rb") as inp:
        if not inp.readline().startswith(P111_HEADER):
            raise ValueError("P113 original P111 header missing")
        for raw in inp:
            if raw.startswith(b"#"):
                raise ValueError("P113 P111 runtime capped/truncated")
            kind,seq,epoch,slot,pc,physical,value=parse_p111(raw)
            if kind=="C":
                if epoch in epochs:
                    raise ValueError("P113 duplicate original JIT generation")
                epochs[epoch]={"start":pc,"run":False,"finished":False}
            elif kind=="F":
                if epoch not in epochs:
                    raise ValueError("P113 finish without original generation")
                epochs[epoch]["finished"]=True
            elif kind=="R":
                if epoch not in epochs:
                    raise ValueError("P113 runtime without original JIT generation")
                epochs[epoch]["run"]=True
    return epochs

def audit(p110:Path,p111:Path,p113:Path,min_agis:int=1):
    # Reject any partial original trace BEFORE reporting positive statistics.
    result110=audit_p110(p110,min_translations=1)
    result111=audit_p111(p111,min_runs=1)
    p110_ops=read_p110_opcodes(p110)
    p113_ops,sha113=read_events(p113)
    epochs=read_p111_generations(p111)
    if len(p110_ops)!=len(p113_ops):
        raise ValueError("P113 P110 opcode AGI and P113 epoch count differs")
    stat=Counter()
    seen_epoch=None
    closed_epochs=set()
    for ref,(epoch,pc,op,checks,positive) in zip(p110_ops,p113_ops):
        if ref!=(pc,op,checks,positive):
            raise ValueError("P113 original AGI opcode reference inconsistent with P110")
        if epoch not in epochs or not epochs[epoch]["finished"]:
            raise ValueError("P113 original AGI epoch lacks successful P111 compiler block")
        if not (epochs[epoch]["start"]<=pc<=epochs[epoch]["start"]+1000):
            raise ValueError("P113 original AGI guest PC not within compiler block start")
        # Original 86Box old Dynarec compilations are sequential, not
        # re-entrant; an opcode group may not reappear after another epoch.
        if epoch!=seen_epoch:
            if epoch in closed_epochs:
                raise ValueError("P113 opcode epoch interleaved with different JIT generation")
            if seen_epoch is not None:
                closed_epochs.add(seen_epoch)
            seen_epoch=epoch
        executed=epochs[epoch]["run"]
        tag="executed" if executed else "compiled_never_jit_executed"
        stat[f"opcodes_{tag}"]+=1
        stat[f"agi_calls_{tag}"]+=checks
        stat[f"positive_agi_counts_{tag}"]+=positive
        if positive:stat[f"positive_agi_opcodes_{tag}"]+=1
    if stat["positive_agi_counts_executed"]+stat[
        "positive_agi_counts_compiled_never_jit_executed"]<min_agis:
        raise ValueError("P113 insufficient observed original AGI positive source counts")
    return {
        "schema":"P113-ORIGINAL-PENTIUM-P110-AGI-OPCODE-P111-JIT-EPOCH-PRIVATE-AUDIT",
        "status":"ORIGINAL_TRANSLATOR_AGI_ATTRIBUTED_TO_JIT_GENERATION",
        "p110_sha256":result110["trace_sha256"],
        "p111_sha256":result111["sha256"],
        "p113_sha256":sha113,
        "original_opcode_agi_events_generation_matched":len(p113_ops),
        "original_opcode_events_in_jit_executed_generations":stat["opcodes_executed"],
        "original_opcode_events_in_only_compiled_generations":
            stat["opcodes_compiled_never_jit_executed"],
        "original_positive_agi_checks_in_jit_executed_generations":
            stat["positive_agi_counts_executed"],
        "original_positive_agi_checks_in_only_compiled_generations":
            stat["positive_agi_counts_compiled_never_jit_executed"],
        "executed_jit_agi_may_include_original_interpreter_first_run":False,
        "per_guest_instruction_tsc_observed":False,
        "original_91_93_95_cpu_cycle_causality_proven":False,
        "autonomous_vst3_pcm_parity_proven":False
    }

def selftest():
    pc=LCOD_BASE+0x4023
    op=LCOD_BASE+0x4027
    p110=(P110_HEADER+b"SYNTHETIC\n"
          +f"S 1 {pc:08X} 00000001 0 0 0 1\n".encode()
          +f"O 2 {op:08X} 75 00000001 00000001 0 0 0 0 1 3 1 1\n".encode()
          +f"S 3 {pc:08X} 00000000 0 0 0 1\n".encode()
          +f"O 4 {op:08X} 75 00000000 00000000 0 0 0 0 1 3 1 0\n".encode())
    p111=(P111_HEADER+b"SYNTHETIC\n"
          +f"C 1 1 7 {pc:08X} 00001000\n".encode()
          +f"F 2 1 7 {pc:08X} 00001000 3\n".encode()
          +f"R 3 1 7 {pc:08X} 00001000 100\n".encode()
          +f"D 4 1 7 {pc:08X} 00001000 105\n".encode()
          +f"C 5 2 8 {pc:08X} 00001004\n".encode()
          +f"F 6 2 8 {pc:08X} 00001004 3\n".encode())
    p113=(HEADER+b"SYNTHETIC\n"
          +f"E 1 1 {op:08X} 75 1 1\n".encode()
          +f"E 2 2 {op:08X} 75 1 0\n".encode())
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp)
        a=root/"P110";b=root/"P111";c=root/"P113"
        a.write_bytes(p110);b.write_bytes(p111);c.write_bytes(p113)
        report=audit(a,b,c)
        assert report["original_opcode_agi_events_generation_matched"]==2
        assert report["original_positive_agi_checks_in_jit_executed_generations"]==1
        assert report["original_opcode_events_in_only_compiled_generations"]==1
        for corrupt,reason in (
            (p113.replace(b"E 2 2 ",b"E 2 9 "),"lacks successful"),
            (p113.replace(b"75 1 1",b"75 1 0"),"inconsistent"),
            (p113.replace(b"E 2 ",b"E 3 "),"sequence/epoch"),
            (p113.replace(b"E 2 2 ",b"E 2 1 "),"inconsistent"),
            (p113.split(b"E 2")[0],"count differs"),
            (p113+b"# P113_EVENT_CAP_REACHED\n","incomplete"),
        ):
            c.write_bytes(corrupt)
            try:audit(a,b,c,min_agis=0)
            except ValueError as exc:
                assert reason in str(exc),(reason,str(exc))
            else:raise AssertionError("invalid original epoch relation accepted: "+reason)
        c.write_bytes(p113)
        a.write_bytes(p110.replace(b"O 4 ",b"O 5 "))
        try:audit(a,b,c)
        except ValueError as exc:assert "sequence discontinuity" in str(exc)
        else:raise AssertionError("original P110 broken sequence accepted")
    print("P113_P110_AGI_P111_EXECUTED_EPOCH_PRIVATE_AUDITOR_SYNTHETIC_PASS")

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-original-p110",type=Path)
    parser.add_argument("--private-original-p111",type=Path)
    parser.add_argument("--private-original-p113",type=Path)
    parser.add_argument("--min-original-positive-agi",type=int,default=1)
    parser.add_argument("--selftest",action="store_true")
    args=parser.parse_args()
    if args.selftest:selftest()
    any_sources=any((args.private_original_p110,args.private_original_p111,
                     args.private_original_p113))
    if any_sources:
        if not all((args.private_original_p110,args.private_original_p111,
                    args.private_original_p113)):
            parser.error("all three same-session P110/P111/P113 original traces required")
        report=audit(args.private_original_p110,args.private_original_p111,
                     args.private_original_p113,args.min_original_positive_agi)
        print("P113_PRIVATE_ORIGINAL_JIT_AGI_EPOCH_AUDIT "+json.dumps(
            report,sort_keys=True))
    elif not args.selftest:
        parser.error("require --selftest or three private original traces")
if __name__=="__main__":main()
