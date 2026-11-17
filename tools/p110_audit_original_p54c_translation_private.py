#!/usr/bin/env python3
"""P110 STRICT PRIVATE ORIGINAL 86Box P54C compile-time AGI/timing trace audit.

A source-generated JIT TRANSLATION event is NOT a guest executed x86 event.
This script never turns translator cycles into per-IO guest TSC or PCM.
Input is private; stdout only redacted aggregates plus full trace SHA.
"""
from __future__ import annotations
import argparse
from collections import Counter
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import sys
import tempfile

from p107_audit_executed_jcc_private import LCOD_BASE,ORIGINAL_SELECTOR,read_trace
HEADER=b"# P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1 "
MAX_ROWS=500000
BRANCH_OPCODES={
    0x4027:0x75, 0x40CB:0x77,0x40E4:0x74,
    0x40F1:0x73,0x4107:0x72,
}

@dataclass(frozen=True)
class Translation:
    kind:str
    seq:int
    pc:int
    pre_mask:int
    post_mask:int|None
    pre_u:int
    post_u:int|None
    pre_cycles:int
    post_cycles:int|None
    agi_calls:int|None
    agi_positive:int|None
    opcode:int|None

def parse_row(line:bytes)->Translation:
    f=line.decode("ascii","strict").split()
    if not f:raise ValueError("empty original P110 translation event")
    if f[0]=="S":
        if len(f)!=8:raise ValueError("P110 block start wrong field count")
        _,seq,pc,mask,u,decode,offset,cycles=f
        e=Translation("S",int(seq),int(pc,16),int(mask,16),
                      None,int(u),None,int(cycles),None,None,None,None)
        for x in (decode,offset):
            if abs(int(x))>1000000:raise ValueError("P110 decoder counter out of range")
    elif f[0]=="O":
        if len(f)!=14:raise ValueError("P110 opcode event wrong field count")
        (_,seq,pc,op,pre,post,preu,postu,predec,preoffset,
          precycle,postcycle,agicalls,agipos)=f
        e=Translation("O",int(seq),int(pc,16),int(pre,16),
                      int(post,16),int(preu),int(postu),
                      int(precycle),int(postcycle),int(agicalls),
                      int(agipos),int(op,16))
        if abs(int(predec))>1000000 or abs(int(preoffset))>1000000:
            raise ValueError("P110 opcode decoder delay invalid")
        if not 0<=e.agi_positive<=e.agi_calls<=16:
            raise ValueError("P110 positive original AGI count exceeds check count")
        if not 0<=e.opcode<=255:
            raise ValueError("P110 opcode invalid")
        off=e.pc-LCOD_BASE
        if off in BRANCH_OPCODES and e.opcode!=BRANCH_OPCODES[off]:
            raise ValueError("P110 original LCOD Jcc opcode differs from SHA-locked Creative")
    else:
        raise ValueError("unsupported original P110 event kind")
    if not 0<=e.seq<=MAX_ROWS or not 0<=e.pre_mask<=0xffffffff:
        raise ValueError("P110 sequence or guest register mask invalid")
    if e.post_mask is not None and not 0<=e.post_mask<=0xffffffff:
        raise ValueError("P110 translated destination mask invalid")
    if e.pre_u not in (0,1) or e.post_u not in (None,0,1):
        raise ValueError("P110 pending U-pipe state invalid")
    if abs(e.pre_cycles)>1000000000 or (
        e.post_cycles is not None and abs(e.post_cycles)>1000000000):
        raise ValueError("P110 translator booked cycle counter invalid")
    if not LCOD_BASE+0x4023<=e.pc<=LCOD_BASE+0x4125:
        raise ValueError("P110 guest translator PC outside original LCOD window")
    return e

def audit(path:Path,expected_sha256:str|None=None,
          min_translations:int=1)->dict:
    h=hashlib.sha256()
    count=0
    next_seq=1
    blocks=0
    translated=0
    block_nonzero=0
    op_mask_changed=0
    agi_checked=0
    agi_true=0
    positive_ops=0
    near_jcc=Counter()
    translated_pcs=set()
    if not path.is_file() or path.stat().st_size>80_000_000:
        raise ValueError("original P110 private translator trace absent or oversized")
    with path.open("rb") as f:
        head=f.readline()
        h.update(head)
        if not head.startswith(HEADER):
            raise ValueError("P110 original translator source header missing")
        for raw in f:
            h.update(raw)
            if raw.startswith(b"# P110_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM"):
                raise ValueError("P110 original translator event cap reached")
            e=parse_row(raw)
            if e.seq!=next_seq:
                raise ValueError("P110 original translator event sequence discontinuity")
            next_seq+=1
            count+=1
            if count>MAX_ROWS:raise ValueError("P110 trace limit exceeded")
            if e.kind=="S":
                blocks+=1
                if e.pre_mask:block_nonzero+=1
            else:
                translated+=1
                translated_pcs.add(e.pc)
                if e.post_mask!=e.pre_mask:op_mask_changed+=1
                agi_checked+=e.agi_calls
                agi_true+=e.agi_positive
                if e.agi_positive:positive_ops+=1
                if e.pc-LCOD_BASE in BRANCH_OPCODES:
                    near_jcc[e.pc-LCOD_BASE]+=1
    if expected_sha256 and h.hexdigest()!=expected_sha256:
        raise ValueError("P110 original private translator trace SHA mismatch")
    if translated<min_translations:
        raise ValueError("too few original LCOD P54C timing translations observed")
    return {
      "schema":"P110-ORIGINAL-P54C-SOURCE-TRANSLATION-AGI-PRIVATE-AUDIT",
      "status":"PENTIUM_SOURCE_TRANSLATION_ONLY_NOT_EXECUTED_TIMING",
      "trace_sha256":h.hexdigest(),
      "translated_lcod_opcode_events":translated,
      "original_translator_block_entries_in_lcod_window":blocks,
      "block_entries_with_carried_nonzero_register_mask":block_nonzero,
      "translated_opcodes_changing_carried_register_mask":op_mask_changed,
      "actual_original_agi_predicate_calls_during_lcod_translation":agi_checked,
      "actual_original_agi_positive_predicates_during_lcod_translation":agi_true,
      "lcod_translated_ops_with_positive_agi":positive_ops,
      "original_compiled_jcc_offsets":{f"0x{k:04X}":v for k,v in sorted(near_jcc.items())},
      "distinct_original_translated_guest_pcs":len(translated_pcs),
      "guest_x86_executions_observed_in_this_trace":0,
      "per_io_pentium_tsc_proven":False,
      "autonomous_vst3_pcm_proven":False,
    }

def selftest():
    start=LCOD_BASE+0x4023
    branch=LCOD_BASE+0x4027
    fixture=(HEADER+b"FICTIONAL_NO_ORIGINAL_GUEST\n"
       +f"S 1 {start:08X} 00000001 0 0 0 1\n".encode()
       +f"O 2 {branch:08X} 75 00000001 00000003 0 0 0 0 1 3 1 1\n".encode())
    with tempfile.TemporaryDirectory() as tmp:
        f=Path(tmp)/"fake-only.p110"
        f.write_bytes(fixture)
        report=audit(f,min_translations=1)
        assert report["original_translator_block_entries_in_lcod_window"]==1
        assert report["actual_original_agi_positive_predicates_during_lcod_translation"]==1
        assert report["block_entries_with_carried_nonzero_register_mask"]==1
        for error,replacement in (
          ("sequence discontinuity",b"O 3 "),
          ("original LCOD Jcc opcode",b"77 00000001"),
          ("positive original AGI count",b"3 1 0"),
          ("outside original LCOD",b"00000002 75"),
        ):
            if error=="sequence discontinuity":
                corrupt=fixture.replace(b"O 2 ",replacement)
            elif error=="original LCOD Jcc opcode":
                corrupt=fixture.replace(b"75 00000001",replacement)
            elif error=="positive original AGI count":
                corrupt=fixture.replace(b"1 3 1 1",replacement)
            else:
                corrupt=fixture.replace(f"{branch:08X} 75".encode(),replacement)
            f.write_bytes(corrupt)
            try:audit(f)
            except ValueError as e:assert error in str(e),(error,str(e))
            else:raise AssertionError(error+" mutation passed")
        f.write_bytes(fixture+b"# P110_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n")
        try:audit(f)
        except ValueError as e:assert "cap reached" in str(e)
        else:raise AssertionError("truncated source trace passed")
        f.write_bytes(fixture)
        try:audit(f,expected_sha256="0"*64)
        except ValueError as e:assert "SHA mismatch" in str(e)
        else:raise AssertionError("bad original trace hash passed")
    print("P110_SOURCE_ORIGINAL_TRANSLATION_PRIVATE_AUDITOR_SYNTHETIC_PASS")

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--private-original-translator",type=Path)
    ap.add_argument("--expected-sha256")
    ap.add_argument("--min-translations",type=int,default=50)
    ap.add_argument("--selftest",action="store_true")
    a=ap.parse_args()
    if a.selftest:selftest()
    if a.private_original_translator:
        r=audit(a.private_original_translator,a.expected_sha256,a.min_translations)
        print("P110_PRIVATE_ORIGINAL_P54C_TRANSLATION_AUDIT "
              +json.dumps(r,sort_keys=True))
    elif not a.selftest:ap.error("provide --selftest or private original timing translator trace")
if __name__=="__main__":main()
