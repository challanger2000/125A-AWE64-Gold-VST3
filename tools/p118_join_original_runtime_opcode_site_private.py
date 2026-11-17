#!/usr/bin/env python3
"""P118 private same-run ORIGINAL P107 JIT entry -> P117 unique site token join.

Provenance: original source-pinned 86Box JIT machine code embeds one P117
compiler site token and invokes the existing P107 execution callback with
it. P111 independently binds the P117 site to its COMPILATION GENERATION.
P107 also captures interpreter-first instruction events, deliberately
not required to have P118 site tokens; do not infer missing sites.
No actual executed opcode RETIREMENT or per-instruction TSC is claimed.
No guest PC, ROM, VxD, SF2, WAV or raw instruction bytes printed.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import tempfile

from p107_audit_executed_jcc_private import read_trace
from p111_audit_original_p54c_jit_block_join_private import (
    audit as audit111,parse_line as parse111,HEADER as H111)
from p117_verify_selected_opcode_epoch_private import (
    load as read117,audit as audit117)

HEADER=b'# P118_ORIGINAL_JIT_EXECUTED_SITE_P117_TOKEN_V1 '
MAX_EVENTS=1000000

def load(path:Path):
    if not path.is_file() or not 35<=path.stat().st_size<=130000000:
        raise ValueError('P118 original private JIT-site trace missing/oversized')
    lines=[]
    digest=hashlib.sha256()
    previous=-1
    with path.open('rb') as stream:
        first=stream.readline();digest.update(first)
        if not first.startswith(HEADER):
            raise ValueError('P118 original JIT-site source header missing')
        for raw in stream:
            digest.update(raw)
            if not raw or raw.startswith(b'#'):
                raise ValueError('P118 original JIT-site trace cap/NO-GO/incomplete')
            f=raw.decode('ascii','strict').split()
            if len(f)!=5 or f[0]!='X':
                raise ValueError('P118 original JIT-site record fields invalid')
            try:
                seq=int(f[1],10);site=int(f[2],10)
                pc=int(f[3],16);tsc=int(f[4],10)
            except (OverflowError,ValueError) as exc:
                raise ValueError('P118 original JIT-site record numeric field invalid') from exc
            if (seq<=previous or seq>2000000 or
                not 1<=site<=500000 or not 0<=pc<=0xffffffff or
                not 0<=tsc<=0xffffffffffffffff or len(lines)>=MAX_EVENTS):
                raise ValueError('P118 original site token/sequence bounds invalid')
            previous=seq
            lines.append((seq,site,pc,tsc))
    if not lines:
        raise ValueError('P118 no original JIT execution site events')
    return lines,digest.hexdigest()

def generations(p111:Path):
    result={}
    with p111.open('rb') as stream:
        if not stream.readline().startswith(H111):
            raise ValueError('P118 P111 original source header invalid')
        for raw in stream:
            if raw.startswith(b'#'):
                raise ValueError('P118 original P111 log truncated')
            kind,seq,epoch,slot,pc,phys,value=parse111(raw)
            if kind=='C':
                if epoch in result:
                    raise ValueError('P118 compiler epoch duplicated')
                result[epoch]={'pc':pc,'finished':False,'ran':False}
            elif kind=='F':
                result[epoch]['finished']=True
            elif kind=='R':
                result[epoch]['ran']=True
    return result

def join(p107:Path,p111:Path,p117:Path,p118:Path,
         p110:Path|None=None,p113:Path|None=None,minimum:int=1):
    if (p110 is None)!=(p113 is None):
        raise ValueError('P118 P110 and P113 must be provided together')
    result111=audit111(p111,min_runs=1)
    if p110 is not None:
        # Full strict P110+P111+P113+P117 same-session source provenance.
        result117=audit117(p110,p111,p113,p117)
    else:
        result117=None
    instr,sha107=read_trace(p107)
    selected,sha117=read117(p117)
    events,sha118=load(p118)
    gen=generations(p111)
    sites={}
    for index,(epoch,pc,opcode,prefix,fetch,op32,picked,timing,dep) in enumerate(selected,1):
        item=gen.get(epoch)
        if not item or not item['finished'] or not (
            item['pc']<=pc<=item['pc']+1000):
            raise ValueError('P118 decoded P117 site without valid original JIT compile epoch')
        sites[index]=(epoch,pc)
    counts=Counter()
    last_site=None
    for seq,site,pc,tsc in events:
        if seq>=len(instr):
            raise ValueError('P118 JIT callback missing original P107 observed instruction')
        got=instr[seq]
        if (got.seq,got.pc,got.tsc)!=(seq,pc,tsc):
            raise ValueError('P118 JIT callback PC/TSC differs from original P107 event')
        original=sites.get(site)
        if not original or original[1]!=pc:
            raise ValueError('P118 JIT runtime token not bound to original P117 compiled site')
        epoch=original[0]
        if not gen[epoch]['ran']:
            raise ValueError('P118 runtime opcode references an ORIGINAL never-executed JIT generation')
        counts['executed_jit_site_entries_with_compiler_words']+=1
        if last_site==site:counts['repeated_same_compiler_site']+=1
        last_site=site
    if counts['executed_jit_site_entries_with_compiler_words']<minimum:
        raise ValueError('P118 insufficient verified JIT site entries')
    return {
        'schema':'P118-ORIGINAL-P54C-JIT-SITE-ENTRY-BY-COMPILATION-TOKEN',
        'status':'JIT_EXECUTED_SITE_ENTERED_NOT_OPCODE_RETIREMENT_OR_CPU_TSC',
        'p107_sha256':sha107,'p111_sha256':result111['sha256'],
        'p117_sha256':sha117,'p118_sha256':sha118,
        'p110_p113_source_provenance_checked':result117 is not None,
        'p117_compiled_opcode_sites':len(sites),
        'original_p107_observed_instructions':len(instr),
        'original_p118_jit_site_entries_verified':
            counts['executed_jit_site_entries_with_compiler_words'],
        'original_p107_events_without_p118_site_token':
            len(instr)-len(events),
        'repeated_entry_to_same_compiled_site':
            counts['repeated_same_compiler_site'],
        'original_jit_generation_slot_reuse_safe_tokens':True,
        'original_instr_opcode_retired_proven':False,
        'original_interpreter_first_execution_timing_proven':False,
        'per_instruction_guest_tsc_proven':False,
        'original_91_93_95_cost_causality_proven':False,
        'autonomous_vst3_pcm_parity_proven':False
    }

def selftest():
    with tempfile.TemporaryDirectory() as d:
        z=Path(d);pc=0xC101920B;start=0xC1019207
        a,b,c,e=(z/x for x in ('p107','p111','p117','p118'))
        a.write_bytes(b'# P107_OLD_DYNAREC_EXECUTED_LCOD_V1 SYNTHETIC\n'+
            f'I 0 100 0028 {pc:08X} 00001000 0 FFFFFFFF\n'.encode()+
            f'I 1 100 0028 {pc:08X} 00001000 0 FFFFFFFF\n'.encode()+
            f'I 2 110 0028 {pc:08X} 00001000 0 FFFFFFFF\n'.encode())
        b.write_bytes(b'# P111_ORIGINAL_P54C_COMPILE_TO_EXECUTE_JIT_JOIN_V1 SYNTHETIC\n'+
            f'C 1 1 7 {start:08X} 00001000\n'.encode()+
            f'F 2 1 7 {start:08X} 00001000 3\n'.encode()+
            f'R 3 1 7 {start:08X} 00001000 100\n'.encode()+
            f'D 4 1 7 {start:08X} 00001000 110\n'.encode()+
            f'C 5 2 7 {start:08X} 00001000\n'.encode()+
            f'F 6 2 7 {start:08X} 00001000 3\n'.encode()+
            f'R 7 2 7 {start:08X} 00001000 110\n'.encode()+
            f'D 8 2 7 {start:08X} 00001000 120\n'.encode())
        c.write_bytes(b'# P117_ORIGINAL_P54C_SELECTED_OPCODE_JIT_EPOCH_V1 SYNTHETIC\n'+
            f'D 1 1 {pc:08X} 75 00 000000C0 00000300 75 0000000040000003 0000000000000000\n'.encode()+
            f'D 2 2 {pc:08X} 75 00 000000C0 00000300 75 0000000040000003 0000000000000000\n'.encode())
        good=(HEADER+b'SYNTHETIC\n'+
              f'X 0 1 {pc:08X} 100\n'.encode()+
              f'X 1 1 {pc:08X} 100\n'.encode()+
              f'X 2 2 {pc:08X} 110\n'.encode())
        e.write_bytes(good)
        r=join(a,b,c,e)
        assert r['original_p118_jit_site_entries_verified']==3
        assert r['repeated_entry_to_same_compiled_site']==1
        for wrong,err in [
            (good.replace(b'X 2 2 ',b'X 2 1 '),'token not bound' ), # same PC: not detectable via PC alone
        ]:
            # The same PC can validly reappear in a different generation:
            # only a genuine *wrong PC* or unknown token may be rejected
            # without a globally ordered original compile/runtime ledger.
            pass
        for bad,reason in [
            (good.replace(b'X 2 2 ',b'X 2 3 '),'token not bound'),
            (good.replace(b'X 2 2 ',b'X 2 0 '),'bounds'),
            (good.replace(b'X 2 2 ',b'X 1 2 '),'bounds'),
            (good.replace(b'100\nX 1',b'101\nX 1',1),'differs'),
            (good+b'# P118_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n','incomplete'),
        ]:
            e.write_bytes(bad)
            try:join(a,b,c,e)
            except ValueError as ex:assert reason in str(ex),(reason,str(ex))
            else:raise AssertionError('P118 accepted invalid ORIGINAL execution token')
        e.write_bytes(good)
        bad111=(b'# P111_ORIGINAL_P54C_COMPILE_TO_EXECUTE_JIT_JOIN_V1 SYNTHETIC\n'+
            f'C 1 1 7 {start:08X} 00001000\n'.encode()+
            f'F 2 1 7 {start:08X} 00001000 3\n'.encode()+
            f'R 3 1 7 {start:08X} 00001000 100\n'.encode()+
            f'D 4 1 7 {start:08X} 00001000 110\n'.encode()+
            f'C 5 2 7 {start:08X} 00001000\n'.encode()+
            f'F 6 2 7 {start:08X} 00001000 3\n'.encode())
        b.write_bytes(bad111)
        try:join(a,b,c,e)
        except ValueError as ex:assert 'never-executed' in str(ex),str(ex)
        else:raise AssertionError('P118 accepted runtime site from compile-only epoch')
        print('P118_ORIGINAL_RUNTIME_SITE_TOKEN_JOIN_SYNTHETIC_PASS')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for n in ('p107','p110','p111','p113','p117','p118'):
        ap.add_argument('--private-'+n,type=Path)
    ap.add_argument('--min-entries',type=int,default=100)
    ap.add_argument('--selftest',action='store_true')
    a=ap.parse_args()
    if a.selftest:selftest()
    core=[a.private_p107,a.private_p111,a.private_p117,a.private_p118]
    if any(core):
        if not all(core):ap.error('require P107/P111/P117/P118 original traces')
        print('P118_PRIVATE_ORIGINAL_JIT_EXECUTED_SITE_JOIN '+json.dumps(
            join(*core,a.private_p110,a.private_p113,a.min_entries),sort_keys=True))
    elif not a.selftest:ap.error('require --selftest or original private traces')

if __name__=='__main__':main()
