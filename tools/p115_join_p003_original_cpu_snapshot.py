#!/usr/bin/env python3
"""P115 strict paired original P003 physical-I/O + live-dynarec snapshot audit.
Only output redacted aggregate clocks. Never infer x86 executed instructions from
translator-only P110 AGI traces, port-gap guesses or source MIDI note tables.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import tempfile

H003=(b'# P003_CPU_GUEST_PHYSICAL_8_16_32_V3 ',
      b'# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK ')
H115=b'# P115_ORIGINAL_DYNAREC_LIVE_CPU_TSC_V1 '
ALLOWED={0xe22,0xe20,0xa20,0xa22,0x620,0x622}
MAX_LINES=1000000

def read_rows(p:Path,head:tuple[bytes,...]):
    if not p.is_file() or not 20<=p.stat().st_size<=230_000_000:
        raise ValueError('original private trace missing/oversized')
    data=p.read_bytes()
    rows=data.splitlines()
    if not rows or not any(rows[0].startswith(x) for x in head):
        raise ValueError('original private source header missing')
    if not 1<=len(rows)-1<=MAX_LINES:
        raise ValueError('original private trace missing/capped')
    for row in rows[1:]:
        if not row or row.startswith(b'#'):
            raise ValueError('original private source trace incomplete or capped')
    return [r.decode('ascii','strict').split() for r in rows[1:]],hashlib.sha256(data).hexdigest()

def audit(p003:Path,p115:Path,minimum:int=1)->dict:
    io,sha003=read_rows(p003,H003)
    clock,sha115=read_rows(p115,(H115,))
    if len(io)!=len(clock) or len(io)<minimum:
        raise ValueError('P115/P003 original physical sequence lengths mismatch')
    last_eff=None
    first_eff=None
    zero=0
    progressed=0
    jit=0
    interpreted=0
    for index,(a,b) in enumerate(zip(io,clock)):
        if len(a) not in (8,17) or len(b)!=11:
            raise ValueError('P003/P115 physical event fields malformed')
        if a[0] not in ('W','R','r') or b[0]!='S' or b[2]!=a[0]:
            raise ValueError('P115 original physical callback direction mismatch')
        try:
            raw003=int(a[1]);cyc003=int(a[2]);port003=int(a[5],16);seq003=int(a[7]);
            seq115=int(b[1]);port115=int(b[3],16)
            raw=int(b[4]);eff=int(b[5]);old=int(b[6]);cyc_old=int(b[7]);cyc=int(b[8]);aux=int(b[9]);run=int(b[10]);
        except (ValueError,OverflowError) as e:
            raise ValueError('P115 invalid numeric original CPU source field') from e
        if (index!=seq003 or index!=seq115 or port003!=port115 or
            port003 not in ALLOWED or raw003!=raw or cyc003!=cyc):
            raise ValueError('P115 original physical port/seq/TSC/cycles mismatch')
        if (raw<0 or raw>2**64-1 or old<0 or old>raw or
            eff<raw or eff>2**64-1 or
            not -2**31<=cyc<=2**31-1 or not -2**31<=cyc_old<=2**31-1 or
            not -2**31<=aux<=2**31-1 or run not in (0,1) or
            (not run and aux!=0)):
            raise ValueError('P115 invalid original CPU clock state')
        spent=cyc_old-cyc+aux
        already_booked=raw-old
        projected=raw+max(0,spent-already_booked)
        if projected!=eff:
            raise ValueError('P115 source-original update_tsc accounting mismatch')
        if last_eff is not None:
            if eff<last_eff:
                raise ValueError('P115 guest effective TSC non-monotone')
            if eff==last_eff:zero+=1
            else:progressed+=1
        if first_eff is None:first_eff=eff
        last_eff=eff
        jit+=int(bool(run))
        interpreted+=int(not run)
    return {'schema':'P115-PRIVATE-SOURCE-ORIGINAL-CLOCK-AND-P003-IO-JOIN',
            'status':'SOURCE_CPU_STATE_RECONCILIATION_ONLY_NOT_NATIVE_AUTONOMY',
            'p003_sha256':sha003,'p115_sha256':sha115,
            'matched_physical_port_callbacks':len(io),
            'callbacks_with_same_effective_tsc_as_previous':zero,
            'callbacks_with_increased_effective_tsc':progressed,
            'jit_callbacks':jit,'non_jit_callbacks':interpreted,
            'span_in_original_guest_tsc_ticks':last_eff-first_eff,
            'complete_x86_instruction_costs_proven':False,
            'autonomous_vst3_pcm_parity_proven':False}

def selftest():
    with tempfile.TemporaryDirectory() as tmp:
        a=Path(tmp)/'p003';b=Path(tmp)/'p115'
        aa=H003[0]+b'TEST_SYNTHETIC\n'
        aa+=b'W 100 997 0028 00000001 0E22 0000 0\n'
        aa+=b'W 100 997 0028 00000001 0E20 0000 1\n'
        aa+=b'R 105 990 0028 00000001 0620 0000 2\n'
        bb=H115+b'TEST_SYNTHETIC\n'
        bb+=b'S 0 W 0E22 100 103 100 1000 997 0 1\n'
        bb+=b'S 1 W 0E20 100 103 100 1000 997 0 1\n'
        bb+=b'S 2 R 0620 105 110 100 1000 990 0 1\n'
        a.write_bytes(aa);b.write_bytes(bb)
        z=audit(a,b);assert z['matched_physical_port_callbacks']==3
        assert z['callbacks_with_same_effective_tsc_as_previous']==1
        assert z['span_in_original_guest_tsc_ticks']==7
        for bad,expect in [
            (bb.replace(b' 1 W 0E20',b' 2 W 0E20'),'port/seq'),
            (bb.replace(b'100 103 100',b'100 104 100',1),'accounting mismatch'),
            (bb.replace(b'S 2 R 0620',b'S 2 R 0622'),'port/seq'),
            (bb.replace(b'S 1 W 0E20',b'S 1 R 0E20'),'direction'),
            (bb+b'# P115_EVENT_CAP_REACHED_NO_COMPLETE_CLAIM\n','incomplete'),
        ]:
            b.write_bytes(bad)
            try:audit(a,b)
            except ValueError as e:assert expect in str(e),(expect,str(e))
            else:raise AssertionError('P115 corruption accepted '+expect)
        print('P115_P003_SOURCE_LIVE_CPU_SNAPSHOT_SYNTHETIC_AUDIT_PASS')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--private-original-p003',type=Path)
    ap.add_argument('--private-original-p115',type=Path)
    ap.add_argument('--min-callbacks',type=int,default=100)
    ap.add_argument('--selftest',action='store_true')
    a=ap.parse_args()
    if a.selftest:selftest()
    if a.private_original_p003 or a.private_original_p115:
        if not a.private_original_p003 or not a.private_original_p115:
            ap.error('both original private P003 and P115 traces are required')
        print('P115_PRIVATE_ORIGINAL_CPU_LIVE_CLOCK_AUDIT '+json.dumps(
            audit(a.private_original_p003,a.private_original_p115,a.min_callbacks),sort_keys=True))
    elif not a.selftest:ap.error('require --selftest or two private traces')
if __name__=='__main__':main()
