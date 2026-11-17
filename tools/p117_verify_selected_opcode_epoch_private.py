#!/usr/bin/env python3
"""P117 private original 86Box decoded P54C word -> P113 opcode/JIT epoch audit.
Strict compiler-time provenance only; P111 run does not imply every opcode
inside its translated block actually executed. No original guest data printed.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import tempfile

from p113_audit_original_opcode_agi_compilation_epoch_private import (
    audit as audit113,read_events as read113)
from p111_audit_original_p54c_jit_block_join_private import (
    parse_line as parse111,HEADER as HEADER111,LCOD_BASE)

HEADER=b'# P117_ORIGINAL_P54C_SELECTED_OPCODE_JIT_EPOCH_V1 '
LIMIT=500000

def selected(prefix:int,opcode:int,fetchdat:int)->int:
    if prefix in (0xd8,0xda,0xdc,0xdd,0xde,0xdf):
        return (opcode>>3)&7
    if prefix in (0xd9,0xdb):
        return opcode&0x3f if fetchdat&0xc0==0xc0 else (opcode>>3)&7
    if prefix==0x0f:return opcode
    if opcode in (0x80,0x81,0x82,0x83,0xc0,0xc1,0xd0,0xd1,
                  0xd2,0xd3,0xf6,0xf7,0xff):
        return (fetchdat>>3)&7
    return opcode

def load(path:Path)->tuple[list[tuple],str]:
    if not path.is_file() or not 30<=path.stat().st_size<=115_000_000:
        raise ValueError('P117 original private decode trace missing/oversized')
    records=[]
    sha=hashlib.sha256()
    with path.open('rb') as inp:
        head=inp.readline();sha.update(head)
        if not head.startswith(HEADER):
            raise ValueError('P117 source-original decode header absent')
        for raw in inp:
            sha.update(raw)
            if not raw or raw.startswith(b'#'):
                raise ValueError('P117 original decode truncated/capped')
            f=raw.decode('ascii','strict').split()
            if len(f)!=11 or f[0]!='D':
                raise ValueError('P117 incorrect original decode record')
            try:
                seq=int(f[1]);epoch=int(f[2]);pc=int(f[3],16)
                opcode=int(f[4],16);prefix=int(f[5],16);fetch=int(f[6],16)
                op32=int(f[7],16);picked=int(f[8],16)
                timing=int(f[9],16);dep=int(f[10],16)
            except (ValueError,OverflowError) as ex:
                raise ValueError('P117 invalid original decode integer') from ex
            if (seq!=len(records)+1 or seq>LIMIT or
                not 1<=epoch<=LIMIT or
                not LCOD_BASE+0x4023<=pc<=LCOD_BASE+0x4125 or
                not 0<=opcode<=255 or not 0<=prefix<=255 or
                not 0<=picked<=255 or not 0<=fetch<=0xffffffff or
                not 0<=op32<=0xffffffff or
                not 0<=timing<=0xffffffffffffffff or
                not 0<=dep<=0xffffffffffffffff):
                raise ValueError('P117 original decoder source bounds/sequence invalid')
            if picked!=selected(prefix,opcode,fetch):
                raise ValueError('P117 source-original selected group opcode inconsistent')
            records.append((epoch,pc,opcode,prefix,fetch,op32,picked,timing,dep))
    if not records:raise ValueError('P117 original decoded opcode events absent')
    return records,sha.hexdigest()

def live_epochs(path:Path)->set[int]:
    epochs=set()
    with path.open('rb') as inp:
        if not inp.readline().startswith(HEADER111):
            raise ValueError('P117 original P111 header missing')
        for raw in inp:
            kind,seq,epoch,slot,pc,phys,value=parse111(raw)
            if kind=='R':epochs.add(epoch)
    return epochs

def audit(p110:Path,p111:Path,p113:Path,p117:Path)->dict:
    proven=audit113(p110,p111,p113)
    reference,_=read113(p113)
    records,sha=load(p117)
    if len(records)!=len(reference):
        raise ValueError('P117/P113 original compiled opcode event counts differ')
    live=live_epochs(p111)
    counter=Counter()
    for i,(rec,ref) in enumerate(zip(records,reference)):
        epoch,pc,opcode,prefix,fetch,op32,picked,timing,dep=rec
        if (epoch,pc,opcode)!=ref[:3]:
            raise ValueError('P117/P113 original compiler epoch+opcode ordinal mismatch')
        if (pc-LCOD_BASE in (0x4027,0x40cb,0x40e4,0x40f1,0x4107)
            and opcode not in (0x75,0x77,0x74,0x73,0x72)):
            raise ValueError('P117 original Creative branch opcode mismatch')
        status='jit_generation_ran' if epoch in live else 'compiled_only'
        counter[status]+=1
        if timing and ((timing>>29)&7)<=3 and not (dep & (0x3ff<<24)):
            counter['integer_eligible_'+status]+=1
        else:
            counter['unsupported_'+status]+=1
    return {
      'schema':'P117-ORIGINAL-P54C-TIMING-INPUT-AND-JIT-EPOCH-PROVENANCE',
      'status':'ORIGINAL_COMPILED_OPCODE_WORDS_ATTRIBUTED_NOT_EXECUTED_TIMING',
      'p110_sha256':proven['p110_sha256'],
      'p111_sha256':proven['p111_sha256'],
      'p113_sha256':proven['p113_sha256'],
      'p117_sha256':sha,
      'original_decoder_events_matched_to_p113':len(records),
      'original_decoded_words_in_later_executed_jit_generations':
          counter['jit_generation_ran'],
      'original_decoded_words_only_compiled':
          counter['compiled_only'],
      'integer_eligible_words_in_executed_jit_generations':
          counter['integer_eligible_jit_generation_ran'],
      'per_instruction_guest_execution_proven':False,
      'exact_noteon_port_tsc_proven':False,
      'autonomous_vst3_pcm_parity_proven':False
    }

def selftest():
    with tempfile.TemporaryDirectory() as d:
        path=Path(d)/'p117'
        pc=LCOD_BASE+0x4027
        head=HEADER+b'FICTIONAL\n'
        good=(head+f'D 1 1 {pc:08X} 75 00 000000C0 00000300 75 0000000040000003 0000000000000000\n'.encode())
        path.write_bytes(good)
        rows,_=load(path)
        assert len(rows)==1 and rows[0][6]==0x75
        for bad,why in (
            (good.replace(b'D 1 1 ',b'D 2 1 '),'sequence'),
            (good.replace(b'00000300 75 ',b'00000300 70 '),'group opcode'),
            (good+b'# P117_EVENT_CAP_REACHED\n','truncated'),
            (good.replace(b'75 00 000000C0',b'75 00 1FFFFFFFF'),'bounds'),
        ):
            path.write_bytes(bad)
            try:load(path)
            except ValueError as ex:
                assert why in str(ex),(why,str(ex))
            else:raise AssertionError('P117 corrupt private decode trace accepted')
        print('P117_ORIGINAL_SELECTED_DECODE_PRIVATE_AUDIT_SYNTHETIC_PASS')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('p110','p111','p113','p117'):
        ap.add_argument('--private-'+name,type=Path)
    ap.add_argument('--selftest',action='store_true')
    a=ap.parse_args()
    if a.selftest:selftest()
    ps=[a.private_p110,a.private_p111,a.private_p113,a.private_p117]
    if any(ps):
        if not all(ps):ap.error('P117 requires P110/P111/P113/P117 same original session')
        print('P117_PRIVATE_ORIGINAL_SELECTED_OPCODE_EPOCH_AUDIT '
              +json.dumps(audit(*ps),sort_keys=True))
    elif not a.selftest:ap.error('provide --selftest or all four private traces')

if __name__=='__main__':main()
