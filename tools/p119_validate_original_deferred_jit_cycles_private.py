#!/usr/bin/env python3
"""P119 private original OLD-x64 JIT pending cycle audit, P118 token-joined.

Do not turn original deferred *compiler* deltas into an autonomous
guest-instruction retirement clock. JIT callback is at instruction
entry and may precede a completing instruction, branch or I/O flush.
No original guest bytes, PC, ROM or WAV in public reports.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import tempfile

from p118_join_original_runtime_opcode_site_private import (
    load as read118,join as join118)

HEADER=b'# P119_ORIGINAL_X64_JIT_PENDING_CYCLES_V1 '
MAX_LINES=1000000

def read119(path:Path):
    if not path.is_file() or not 40<=path.stat().st_size<=160000000:
        raise ValueError('P119 original private pending-JIT trace missing/oversized')
    events=[];sha=hashlib.sha256()
    previous=-1
    with path.open('rb') as stream:
        first=stream.readline();sha.update(first)
        if not first.startswith(HEADER):
            raise ValueError('P119 original deferred-cycle source header absent')
        for raw in stream:
            sha.update(raw)
            if raw.startswith(b'#') or not raw:
                raise ValueError('P119 original deferred-cycle source NO-GO/capped')
            fields=raw.decode('ascii','strict').split()
            if len(fields)!=9 or fields[0]!='Y':
                raise ValueError('P119 original deferred-cycle field count invalid')
            try:
                seq=int(fields[1]);site=int(fields[2])
                pc=int(fields[3],16);tsc=int(fields[4])
                committed=int(fields[5]);pending=int(fields[6])
                aux=int(fields[7]);jit=int(fields[8])
            except (ValueError,OverflowError) as exc:
                raise ValueError('P119 original deferred-cycle numeric field invalid') from exc
            if (seq<=previous or seq>2000000 or
                not 1<=site<=500000 or not 0<=pc<=0xffffffff or
                not 0<=tsc<=0xffffffffffffffff or
                not -2**31<=committed<2**31 or
                not -2**31<=pending<2**31 or
                not -2**31<=aux<2**31 or jit!=1 or
                len(events)>=MAX_LINES):
                raise ValueError('P119 source-original deferred-cycle bound/sequence invalid')
            previous=seq
            events.append((seq,site,pc,tsc,committed,pending,aux))
    if not events:raise ValueError('P119 no real JIT site pending delta events')
    return events,sha.hexdigest()

def join(p118:Path,p119:Path,minimum:int=1):
    original,sha118=read118(p118)
    charges,sha119=read119(p119)
    if len(original)!=len(charges) or len(charges)<minimum:
        raise ValueError('P119 original P118/P119 runtime JIT callback event count mismatch')
    totals=Counter()
    for (i,site,pc,tsc),(j,src_site,src_pc,src_tsc,committed,pending,aux) in zip(original,charges):
        if (i,site,pc,tsc)!=(j,src_site,src_pc,src_tsc):
            raise ValueError('P119 original P118 site/sequence/clock mismatch')
        # Keep arithmetic in signed 64-bit; this is a diagnostic
        # deferred-delta projection, not an original source TSC mutation.
        hypothetical=committed+pending
        if not -2**32<=hypothetical<=2**32:
            raise ValueError('P119 original deferred-cycle projection overflow')
        totals['jit_runtime_site_entries']+=1
        totals['nonzero_original_compiler_deferred_delta']+=int(pending!=0)
        totals['negative_deferred_delta']+=int(pending<0)
        totals['nonzero_original_acycs']+=int(aux!=0)
        totals['max_abs_deferred_delta']=max(
            totals['max_abs_deferred_delta'],abs(pending))
    return {
      'schema':'P119-ORIGINAL-86BOX-JIT-DEFERRED-CYCLE-MEMORY-DIAGNOSTIC',
      'status':'COMPILER_DELAYED_CYCLES_OBSERVED_AT_JIT_SITE_NOT_INSTR_RETIRED',
      'p118_sha256':sha118,'p119_sha256':sha119,
      'runtime_site_callbacks_with_original_cycle_snapshots':
          totals['jit_runtime_site_entries'],
      'callbacks_with_unflushed_compiler_cycle_delta':
          totals['nonzero_original_compiler_deferred_delta'],
      'callbacks_with_negative_unflushed_compiler_delta':
          totals['negative_deferred_delta'],
      'callbacks_with_nonzero_source_acycs':
          totals['nonzero_original_acycs'],
      'maximum_absolute_original_deferred_cycle_delta':
          totals['max_abs_deferred_delta'],
      'original_cpu_counters_mutated_by_probe':False,
      'per_guest_instruction_tsc_proven':False,
      'original_91_93_95_cpu_cycle_causality_proven':False,
      'autonomous_vst3_pcm_parity_proven':False
    }

def selftest():
    with tempfile.TemporaryDirectory() as d:
        a=Path(d)/'p118';b=Path(d)/'p119'
        a.write_bytes(b'# P118_ORIGINAL_JIT_EXECUTED_SITE_P117_TOKEN_V1 FAKE\n'
            b'X 0 1 C101920B 100\nX 2 1 C101920B 100\n')
        good=(HEADER+b'FAKE\n'
            b'Y 0 1 C101920B 100 1000 -3 3 1\n'
            b'Y 2 1 C101920B 100 997 0 3 1\n')
        b.write_bytes(good)
        ok=join(a,b)
        assert ok['runtime_site_callbacks_with_original_cycle_snapshots']==2
        assert ok['callbacks_with_unflushed_compiler_cycle_delta']==1
        for wrong,expected in (
            (good.replace(b'Y 2 1',b'Y 3 1'),'sequence/clock mismatch'),
            (good.replace(b'997 0 3 1',b'997 0 3 0'),'bound/sequence'),
            (good.replace(b'100 1000 -3',b'101 1000 -3'),'sequence/clock mismatch'),
            (good.replace(b'Y 2 1',b'Y 2 2'),'site/sequence/clock mismatch'),
            (good+b'# P119_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n','NO-GO/capped')
        ):
            b.write_bytes(wrong)
            try:join(a,b)
            except ValueError as exc:
                assert expected in str(exc),(expected,str(exc))
            else:raise AssertionError('P119 corrupted original data passed')
        print('P119_P118_JIT_SITE_SOURCE_DEFERRED_CYCLE_JOIN_SYNTHETIC_PASS')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for n in ('p107','p110','p111','p113','p117','p118','p119'):
        parser.add_argument('--private-'+n,type=Path)
    parser.add_argument('--selftest',action='store_true')
    parser.add_argument('--min-sites',type=int,default=100)
    x=parser.parse_args()
    if x.selftest:selftest()
    if x.private_p119 or x.private_p118:
        needed=[x.private_p107,x.private_p111,x.private_p117,
                x.private_p118,x.private_p119]
        if not all(needed):
            parser.error('private P119 needs original P107/P111/P117/P118/P119 traces')
        # Validate original epoch/decoder source BEFORE crediting CPU snapshots.
        join118(x.private_p107,x.private_p111,x.private_p117,x.private_p118,
                x.private_p110,x.private_p113,x.min_sites)
        print('P119_PRIVATE_ORIGINAL_P54C_JIT_PENDING_CYCLE_AUDIT '
              +json.dumps(join(x.private_p118,x.private_p119,x.min_sites),
                          sort_keys=True))
    elif not x.selftest:
        parser.error('provide --selftest or all private original traces')

if __name__=='__main__':main()
