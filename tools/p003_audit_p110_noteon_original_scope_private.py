#!/usr/bin/env python3
"""P003 PRIVATE original Win95 NOTE-ON timing coverage vs P110 x86 scope.

Two SHA-pinned owner captures, real physical CPU-I/O callbacks only.
The resulting PC distribution is NOT a count of executed x86 instructions:
callback RETURN-PC is used to show which real register paths lie OUTSIDE
P110's current translator window LCOD+0x4023..+0x4125. P110/P111
compile-epoch-to-run correlation remains essential for actual cycle costs.

Private original zip, ROM, VXD, executable, audio and full traces must never
be pushed into public GitHub. Emit aggregate counts only.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import zipfile

LCOD=0xC10151E4
P110_FIRST=0x4023
P110_LAST=0x4125
SESSIONS=(
 ("2026-11-16T18:48","6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944",1522737),
 ("2026-11-17T01:04","28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c",1529747),
)
IMPORTANT=(
  0x3E0C,0x3F13,0x3F52,0x3F83,0x3FCE,
  0x4065,0x40A7,0x40C4,0x414F,0x415D,0x4F97,0x5280
)

def checksum(path):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for part in iter(lambda:f.read(1<<20),b""):h.update(part)
    return h.hexdigest()

def verify_capture(path,session):
    name,digest,expected=session
    if checksum(path)!=digest:
        raise ValueError("Original owner CPU-I/O zip SHA256 mismatch")
    counters=Counter()
    sites=Counter()
    prior_tsc=None
    with zipfile.ZipFile(path) as z,z.open("Original-CPU-IO.trace") as trace:
        head=trace.readline()
        if not head.startswith(b"# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK"):
            raise ValueError("Wrong original 86Box CPU-I/O trace schema")
        for line in trace:
            if line[:2] not in (b"W ",b"R ",b"r "):
                continue
            f=line.split()
            if len(f)!=17:raise ValueError("Corrupted original port callback")
            tsc=int(f[1])
            if prior_tsc is not None and tsc<prior_tsc:
                raise ValueError("Original guest CPU TSC regression")
            prior_tsc=tsc
            pc=int(f[9],16)-LCOD
            counters["physical_io_callbacks"]+=1
            if P110_FIRST<=pc<=P110_LAST:
                counters["p110_window_callback_return_sites"]+=1
            if 0x3000<=pc<=0x6000:
                counters["lcod_3000_6000_return_sites"]+=1
            if pc in IMPORTANT:
                sites[pc]+=1
    if counters["physical_io_callbacks"]!=expected:
        raise ValueError("Original CPU-I/O event count inconsistent")
    return {
        "session":name,
        **dict(counters),
        "original_lcod_site_observations":{hex(k):sites[k] for k in IMPORTANT},
        "p110_current_translator_only_covers_lcod_4023_to_4125":True,
        "observed_callbacks_outside_p110_window":
          counters["physical_io_callbacks"]-counters["p110_window_callback_return_sites"],
        "full_noteon_instruction_cost_coverage_proven":False,
    }

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("older",type=Path)
    p.add_argument("newer",type=Path)
    args=p.parse_args()
    output=[
        verify_capture(args.older,SESSIONS[0]),
        verify_capture(args.newer,SESSIONS[1])
    ]
    print(json.dumps({
      "result":"P003_ORIGINAL_P110_COMPLETE_NOTEON_SOURCE_SCOPE_GAP_CONFIRMED",
      "original_owner_captures":output,
      "translation_not_guest_execution":True,
      "full_original_note_on_timing_autonomous":False,
      "private_driver_trace_media_published":False
    },indent=2,sort_keys=True))
if __name__=="__main__":main()
