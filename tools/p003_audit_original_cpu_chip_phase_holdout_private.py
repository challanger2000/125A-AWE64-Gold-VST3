#!/usr/bin/env python3
"""PRIVATE P003: SHA-locked CPU-TSC to EMU8000 frame cross-session holdout.

Strictly join ALL original Win95 port writes to original EMU8000 sample-frame
writes; compare (port,value) before assigning timestamps. Fit ONE integer
phase-origin on older owner data; HOLD OUT all newer owner data and separately
its genuinely divergent suffix. No Creative original bytes published.

This is a diagnostic, NOT a live VST3 autonomous MIDI/Pentium scheduler.
The capture epoch phase must not be hardcoded into a product VST3.
"""
from array import array
from collections import Counter
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

CPU_HZ=133333333
CHIP_HZ=44100
OWNER_SHA={
 "older":"6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944",
 "newer":"28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c",
}

def cpu_writes(z):
    with z.open("Original-CPU-IO.trace") as fh:
        if not fh.readline().startswith(b"# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK "):
            raise ValueError("Unrecognized original guest CPU trace")
        for line in fh:
            if not line.startswith(b"W "):continue
            f=line.split()
            if len(f)!=17:raise ValueError("Malformed original CPU record")
            yield int(f[1]),int(f[5],16),int(f[6],16),int(f[9],16)

def chip_writes(z):
    with z.open("Original-Register.trace") as fh:
        if not fh.readline().startswith(b"# EMU8000 port write trace"):
            raise ValueError("Unrecognized original EMU8000 sample frame")
        for line in fh:
            if line.startswith((b"R ",b"#",b"\n")):continue
            f=line.split()
            if len(f)!=3:raise ValueError("Malformed original chip frame")
            yield int(f[0]),int(f[1],16),int(f[2],16)

def read(path,sha):
    h=hashlib.sha256()
    with path.open("rb") as f:
        for part in iter(lambda:f.read(1<<20),b""):h.update(part)
    if h.hexdigest()!=sha:raise ValueError("Owner original SHA256 mismatch")
    types=(("u","q"),("tsc","Q"),("frame","I"),("port","H"),("value","H"),("pc","I"))
    d={key:array(kind) for key,kind in types}
    with zipfile.ZipFile(path) as z:
        a=cpu_writes(z); b=chip_writes(z)
        for tsc,port,value,pc in a:
            try:frame,p2,v2=next(b)
            except StopIteration as e:raise ValueError("Missing original chip write") from e
            if (port,value)!=(p2,v2):
                raise ValueError("Original guest CPU ↔ EMU8000 physical write mismatch")
            d["u"].append(frame*CPU_HZ-tsc*CHIP_HZ)
            for k,x in (("tsc",tsc),("frame",frame),
                        ("port",port),("value",value),("pc",pc)):
                d[k].append(x)
        if next(b,None) is not None:raise ValueError("Unmatched EMU8000 writes")
    if len(d["u"])<1000000:raise ValueError("Truncated original capture")
    return d

def best_phase(u):
    """Strict integer rational frame: floor((tsc*44100+P)/133333333)."""
    a=sorted(u)
    if not a:raise ValueError("Empty original stream")
    left=0;best=(0,0)
    for right,x in enumerate(a):
        # Valid for each event: frame*CPU-tsc*FS <=P<
        # (frame+1)*CPU-tsc*FS, hence u<=P<u+CPU.
        while x-a[left]>=CPU_HZ:left+=1
        if right-left+1>best[0]:best=(right-left+1,x)
    return best

def evaluate(d,phase,start=0):
    failure=Counter();sites=Counter();moments=set()
    for i in range(start,len(d["u"])):
        u=d["u"][i]
        if not(phase-CPU_HZ<u<=phase):
            failure["next" if u>phase else "previous"]+=1
            moments.add(d["tsc"][i])
            sites[f"0x{d['pc'][i]:08X}"]+=1
    total=len(d["u"])-start
    return dict(events=total,exact=total-sum(failure.values()),
                wrong_frame=sum(failure.values()),distinct_cpu_tsc=len(moments),
                wrong_frame_direction=dict(failure),top_source_pcs=sites.most_common(6))

def common_prefix(a,b):
    for i in range(min(len(a["u"]),len(b["u"]))):
        if any(a[k][i]!=b[k][i] for k in ("tsc","frame","port","value")):
            return i
    return min(len(a["u"]),len(b["u"]))

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--older",type=Path,required=True)
    ap.add_argument("--newer",type=Path,required=True)
    args=ap.parse_args()
    older=read(args.older,OWNER_SHA["older"])
    newer=read(args.newer,OWNER_SHA["newer"])
    _,phase=best_phase(older["u"])
    diverged=common_prefix(older,newer)
    result=dict(
        result="P003_ORIGINAL_CPU_TO_EMU_SAMPLE_PHASE_CROSS_SESSION_HOLDOUT",
        guest_cpu_hz=CPU_HZ,chip_sample_hz=CHIP_HZ,
        total_original_port_value_exact=len(older["u"])+len(newer["u"]),
        fitted_only_on_older_phase_numerator=phase,
        older_training=evaluate(older,phase),
        newer_holdout=evaluate(newer,phase),
        original_shared_prefix_write_events=diverged,
        newer_diverged_suffix_holdout=evaluate(newer,phase,diverged),
        source_timer_process_causality_measured=False,
        original_cpu_instruction_cost_model_proven=False,
        autonomous_vst3_pcm_parity_proven=False,
        guest_trace_or_driver_bytes_published=False)
    print(json.dumps(result,indent=2,sort_keys=True))
    return result

if __name__=="__main__":main()
