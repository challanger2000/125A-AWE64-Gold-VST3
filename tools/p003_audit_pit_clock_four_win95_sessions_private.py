#!/usr/bin/env python3
"""P003 PRIVATE 4-session Win95 source-pinned ORIGINAL CPU↔EMU frame audit.

Correct guest audio timebase is NOT the integer cpu_table.rspeed 133333333:
original 86Box src/pit.c pit_set_clock() repairs a 33333333 remainder
to +100000000/3 before computing TIMER_USEC. The exact source-clock
ratio is 400000000/3 Hz. Thus:
    original_frame = (original_tsc * (3*44100) + P) // 400000000

The constant P is a particular guest/EMU timer EPOCH, not a driver timing
cost and NEVER an autonomous VST3 MIDI start phase. P is trained on the
FIRST original session only and used UNCHANGED for three holdouts.

Usage: python p003_audit_pit_clock_four_win95_sessions_private.py \
  SESSION_173917.zip SESSION_175607.zip SESSION_184835.zip SESSION_010413.zip

SHA256 pin every owner capture. Verify (port,value) for EACH original CPU
write against SAME original EMU8000 write before comparing timing. Never
commit the private archive, ROM, original Creative driver or original logs.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

CPU_HZ_NUM=400000000
CPU_HZ_DEN=3
CHIP_HZ=44100
CPU_FRAME_MULT=CPU_HZ_DEN*CHIP_HZ
CASES=[
 ("20261116-173917","7def342a694c7e9610dc6dab396e98ddf3cad1ac62f69c895f30b7f4ce34f2c7",760209),
 ("20261116-175607","cff0c0527cdf13c718621fad6bb23e255188c1f14f3c3ca4f293b9185f250fb7",1201748),
 ("20261116-184835","6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944",1201218),
 ("20261117-010413","28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c",1203659),
]

def filehash(path:Path)->str:
    digest=hashlib.sha256()
    with open(path,"rb") as fh:
        for block in iter(lambda:fh.read(1<<20),b""):
            digest.update(block)
    return digest.hexdigest()

def guest_writes(z):
    with z.open("Original-CPU-IO.trace") as fh:
        header=fh.readline()
        if not header.startswith(b"# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK "):
            raise ValueError("Unexpected original CPU trace provenance/schema")
        for raw in fh:
            if not raw.startswith(b"W "):
                continue
            cols=raw.split()
            if len(cols)!=17:
                raise ValueError("Invalid original Win95 CPU I/O record")
            yield int(cols[1]),int(cols[5],16),int(cols[6],16)

def chip_writes(z):
    with z.open("Original-Register.trace") as fh:
        first=fh.readline()
        if not first.startswith(b"# EMU8000 port write trace, 44100 Hz timebase"):
            raise ValueError("Wrong original 44.1 kHz chip trace")
        for raw in fh:
            if raw.startswith((b"#",b"R ",b"\n")):
                continue
            c=raw.split()
            if len(c)!=3:raise ValueError("Malformed original chip write")
            yield int(c[0]),int(c[1],16),int(c[2],16)

def session_scan(path:Path,pin:str,expected:int,phase:int|None):
    if filehash(path)!=pin:
        raise ValueError(f"SHA256 pin failed for {path.name}: reject")
    lo=None
    hi=None
    verified=0
    errors=0
    old_integer_hz_wrong=0
    with zipfile.ZipFile(path) as z:
        cpu=guest_writes(z)
        chip=chip_writes(z)
        for tsc,port,value in cpu:
            item=next(chip,None)
            if item is None:
                raise ValueError("Original CPU write had no EMU8000 write")
            frame,p2,v2=item
            if (port,value)!=(p2,v2):
                raise ValueError("Original guest CPU/chip port/value mismatch")
            u=frame*CPU_HZ_NUM-tsc*CPU_FRAME_MULT
            lo=u if lo is None else max(lo,u)
            hi=(u+CPU_HZ_NUM-1) if hi is None else min(hi,u+CPU_HZ_NUM-1)
            if phase is not None:
                pred=(tsc*CPU_FRAME_MULT+phase)//CPU_HZ_NUM
                errors+=(pred!=frame)
            verified+=1
        if next(chip,None) is not None:
            raise ValueError("Original EMU8000 write had no matching CPU I/O write")
    if verified!=expected:
        raise ValueError(f"Original write count mismatch {verified} != {expected}")
    if lo is None or hi is None or lo>hi:
        raise ValueError("No valid single epoch phase for this original capture")
    return dict(physical_writes=verified,epoch_phase_lower=lo,
                epoch_phase_upper=hi,wrong_frames=errors)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("sessions",nargs=4,type=Path,metavar="ORIGINAL_ZIP")
    args=p.parse_args()
    # Train ONLY on earliest session; do not inspect later phases.
    label,pin,n=CASES[0]
    train=session_scan(args.sessions[0],pin,n,None)
    fitted_phase=(train["epoch_phase_lower"]+train["epoch_phase_upper"])//2
    output=[(label,session_scan(args.sessions[0],pin,n,fitted_phase))]
    for (label,pin,n),path in zip(CASES[1:],args.sessions[1:]):
        output.append((label,session_scan(path,pin,n,fitted_phase)))
    exact=sum(x["physical_writes"]-x["wrong_frames"] for k,x in output)
    total=sum(x["physical_writes"] for k,x in output)
    result=dict(
        status=("P003_ORIGINAL_86BOX_PIT_RATIONAL_FOUR_SESSIONS_PASS"
                if exact==total else "P003_ORIGINAL_86BOX_PIT_RATIONAL_FOUR_SESSIONS_FAIL"),
        source="turican0/86Box-AWE32 v6.0-AWE32-0.93 pit_set_clock original 100000000/3 repair",
        measured_original_cpu_timebase_hz="400000000/3",
        original_chip_sample_hz=44100,
        epoch_trained_on_first_only=True,
        fixed_epoch_numerator_for_this_guest_start=fitted_phase,
        total_strict_port_value_matches=total,
        total_correct_frame_positions=exact,
        total_wrong_frame_positions=total-exact,
        independent_sessions=[dict(session=k,**v) for k,v in output],
        original_guest_bytes_exported=False,
        original_cpu_instruction_timing_reproduced=False,
        autonomous_vst3_pcm_parity_proven=False)
    print(json.dumps(result,indent=2,sort_keys=True))
    if exact!=total:raise SystemExit(1)

if __name__=="__main__":main()
