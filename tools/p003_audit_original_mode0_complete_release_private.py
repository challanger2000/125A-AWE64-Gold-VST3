#!/usr/bin/env python3
"""Strict private-original P003 mode-0 release→VTFT I/O auditor.

Usage:
  python tools/p003_audit_original_mode0_complete_release_private.py \
       OLDER-P003-RESULT.zip NEWER-P003-RESULT.zip

Requires each exact independently SHA-256-pinned private original Win95
capture. Emits only aggregate counts. Does NOT export any original bytes.

Actual SBAWE.VXD LCOD source and original EMU8000 I/O:
  +3FCE: E22 byte selector = 0xA0|physical_voice, then A20 word = 0x00FF
  +3E0C: E22 byte selector = 0x60|SAME_voice,
          DATA0 DWORD (0x620 word=0xFFFF, 0x622 word=0)
All physical voices 0..31; NO arbitrary 30-voice truncation.

The TSC interval between release and VTFT is recorded diagnostically
but NOT interpreted as a universal Pentium cycle formula.
"""
import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path
import zipfile

LCOD=0xC10151E4
OWNER_SESSIONS=(
    ("older","6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944",
     5878,101,117,2288),
    ("newer","28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c",
     5878,108,122,2369),
)
# Source return PC, original 86Box helper EIP, hardware port, expected data.
def step_spec(voice,index):
    if index==1:return (0x3FCE,0xC1018EC4,0x0A20,0x00FF)
    if index==2:return (0x3E0C,0xC1018F12,0x0E22,0x60|voice)
    if index==3:return (0x3E0C,0xC1018F23,0x0620,0xFFFF)
    if index==4:return (0x3E0C,0xC1018F23,0x0622,0x0000)
    raise ValueError("bad step index")

def verify_stream(lines):
    """Fail-closed original Win95 I/O chronology, one pass, low memory."""
    count=Counter()
    selected=Counter()
    pending={} # original sequence number -> (voice,step,start_tsc)
    last_vtft=None
    gap=Counter()
    for raw in lines:
        if raw.startswith(b"#") or not raw.strip():continue
        fields=raw.split()
        if len(fields)!=17:continue
        direction=fields[0]
        if direction not in (b"W",b"R",b"r"):continue
        tsc=int(fields[1])
        seq=int(fields[7])
        site=int(fields[9],16)-LCOD
        helper=int(fields[4],16)
        port=int(fields[5],16)
        data=int(fields[6],16)
        if seq in pending:
            voice,step,start_tsc=pending.pop(seq)
            want=step_spec(voice,step)
            got=(site,helper,port,data)
            if direction!=b"W" or got!=want:
                raise ValueError(
                    f"Original release sequence mismatch at step={step} voice={voice}")
            count["verified_followups"]+=1
            if step==2:
                if tsc<start_tsc:raise ValueError("Original guest TSC regressed")
                gap[tsc-start_tsc]+=1
        if direction==b"W" and site==0x3F13 and port==0x0E22:
            voice=data&31
            if helper!=0xC1018F3F or data!=(0x60|voice):
                raise ValueError("Invalid source VTFT scan selector")
            last_vtft=(voice,seq)
        if direction==b"W" and site==0x3FCE and port==0x0E22:
            voice=data&31
            if helper!=0xC1018EAB or data!=(0xA0|voice):
                raise ValueError("Invalid original source release selector")
            if any((seq+i) in pending for i in range(1,5)):
                raise ValueError("Overlapping incomplete original release")
            count["selected"]+=1
            selected[voice]+=1
            if (last_vtft is not None and last_vtft[0]==29 and
                    0<seq-last_vtft[1]<=12):
                count["ranked_after_voice29"]+=1
            for i in range(1,5):
                pending[seq+i]=(voice,i,tsc)
    if pending:raise ValueError("Original release missing subsequent register words")
    if sum(selected.values())*4!=count["verified_followups"]:
        raise ValueError("Original selection-to-register correlation missing")
    if set(selected)!=set(range(32)):
        raise ValueError("Original physical voice coverage not 0..31")
    if not gap or min(gap)<0:raise ValueError("Invalid source TSC diagnostic")
    return {
        "selections":count["selected"],
        "correlated_followup_writes":count["verified_followups"],
        "physical_30_selections":selected[30],
        "physical_31_selections":selected[31],
        "ranked_after_last_voice29":count["ranked_after_voice29"],
        "distinct_tsc_gaps":len(gap),
        "min_tsc_gap":min(gap),
        "max_tsc_gap":max(gap)
    }

def file_sha256(path):
    h=hashlib.sha256()
    with open(path,"rb") as fh:
        for block in iter(lambda:fh.read(1<<20),b""):
            h.update(block)
    return h.hexdigest()

def audit(path,session):
    label,digest,total,high30,high31,ranked=session
    if file_sha256(path)!=digest:
        raise ValueError("Unexpected owner Win95 ZIP checksum")
    with zipfile.ZipFile(path) as z:
        with z.open("Original-CPU-IO.trace") as fh:
            header=fh.readline()
            if not header.startswith(b"# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK "):
                raise ValueError("Wrong original CPU I/O trace schema")
            result=verify_stream(fh)
    if (result["selections"],result["physical_30_selections"],
        result["physical_31_selections"],result["ranked_after_last_voice29"]
        )!=(total,high30,high31,ranked):
        raise ValueError(f"Original {label} counts do not match SHA-pinned audit")
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("older",type=Path)
    p.add_argument("newer",type=Path)
    args=p.parse_args()
    results={x[0]:audit(getattr(args,x[0]),x) for x in OWNER_SESSIONS}
    print(json.dumps({
        "status":"P003_ORIGINAL_MODE0_ALL32_RELEASE_TO_VTFT_11756_PASS",
        "all_selected_total":sum(x["selections"] for x in results.values()),
        "all_correlated_followup_writes":sum(
            x["correlated_followup_writes"] for x in results.values()),
        "sessions":results,
        "guest_data_published":False,
        "original_driver_or_EMU8000_modified":False,
        "native_vst3_pcm_parity_proven":False
    },sort_keys=True,indent=2))

if __name__=="__main__":
    main()
