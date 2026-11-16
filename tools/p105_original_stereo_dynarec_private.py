#!/usr/bin/env python3
"""SHA-locked private original P54C/Dynarec CCCA->VTFT causal exclusion test.

The user must not upload any Win95/Creative trace to public GitHub.
Run ONLY locally on the 2026-11-16 original V4 archive. Prints aggregate
counts, no original CCCA values, guest TSC sequence, ROM or driver bytes.
Does not generate original CPU timing or approve native VST3 audio.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import zipfile

SOURCE_SHA = "2b507624a159f4f8ed9c69e22b3a300bbbe8937aecb0d6d3b254ae641ca2f3c6"
ORIGINAL_VXD_SHA = "d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a"
CODE_BASE = 0xC10151E4
# Independently established original LCOD helper CALL return offsets.
# No original instruction or program bytes in this file.
SITES = {
    0x4065: (0, (("W",0xE22),("R",0x620),("R",0x622))),
    0x40A7: (1, (("W",0xE22),("R",0xA20))),
    0x40C4: (2, (("W",0xE22),("R",0xA20),("R",0xA22))),
    0x414F: (3, (("W",0xE22),("W",0xA20))),
    0x415D: (3, (("W",0xE22),("W",0xA20))),
}

def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024*1024), b""):
            h.update(chunk)
    return h.hexdigest()

def monotone_cursor(cursors):
    return len(cursors)>=2 and all(b>a for a,b in zip(cursors,cursors[1:]))

def single_fixed_threshold_possible(cursors,gaps):
    """One boolean threshold over a fixed sample end yields <=1 change."""
    if len(cursors)!=len(gaps) or not cursors:
        raise ValueError("invalid threshold evidence")
    ordered=sorted(zip(cursors,gaps))
    if any(b[0]<=a[0] for a,b in zip(ordered,ordered[1:])):
        raise ValueError("cursor input contains equal values")
    return sum(a[1]!=b[1] for a,b in zip(ordered,ordered[1:]))<=1

def extract_original_runs(path):
    runs=[]
    pending=[]
    group=[]
    current=None
    last_seq=-1
    total_physical=0
    with zipfile.ZipFile(path) as z,z.open("Original-CPU-IO.trace") as fh:
        if b"V4_STACK" not in fh.readline():
            raise ValueError("not a V4 original physical callback trace")
        for raw in fh:
            f=raw.split()
            if len(f)!=17:raise ValueError("wrong original CPU event width")
            seq=int(f[7]); tsc=int(f[1]); rem=int(f[2])
            site=(int(f[9],16)-CODE_BASE)&0xffffffff
            if seq<=last_seq:raise ValueError("original CPU seq is not increasing")
            last_seq=seq
            if site not in SITES:
                if pending or group:raise ValueError("other handler interrupted stereo helper")
                continue
            kind,sig=SITES[site]
            direction=f[0].decode().upper()[:1]
            port=int(f[5],16); word=int(f[6],16)
            if not pending and not group and site!=0x4065:
                raise ValueError("not original mode-1 allocator opening helper")
            if current is None:current=site
            if current!=site or len(group)>=len(sig):
                raise ValueError("incomplete original x86 callsite helper")
            if (direction,port)!=sig[len(group)]:
                raise ValueError("original EMU8000 callback signature changed")
            if group and (tsc!=group[0][0] or seq!=group[-1][2]+1):
                raise ValueError("split original CPU callback group")
            group.append((tsc,rem,seq,word))
            if len(group)==len(sig):
                cursor=((group[1][3]|(group[2][3]<<16))&0xFFFFFF) if kind==2 else None
                pending.append((
                    site,kind,group[0][0],group[0][1],cursor,
                    group[0][2],group[-1][2],group[0][1]-group[-1][1]
                ))
                total_physical+=len(group)
                group=[]
                current=None
                if site==0x415D:
                    if len(pending)<2 or pending[-2][0]!=0x414F:
                        raise ValueError("second stereo release missing")
                    runs.append(pending)
                    pending=[]
    if pending or group:raise ValueError("incomplete original mode-1 pair")
    if len(runs)!=25 or total_physical!=4867:
        raise ValueError("original 25/4867 source count mismatch")
    return runs

def summarize(runs):
    full=runs[-9:]
    shapes=[[(x[0],x[1]) for x in row] for row in full]
    if len(full)!=9 or any(len(row)!=92 or shape!=shapes[0]
                          for row,shape in zip(full,shapes)):
        raise ValueError("missing 9 identical original full scan paths")
    if any(sum(x[6]-x[5]+1 for x in row)!=245 for row in full):
        raise ValueError("full scan physical port size mismatch")
    variable=0
    increasing=0
    rejected=0
    ccca_intra_12=0
    deltas=set()
    for ix in range(91):
        gaps=[row[ix+1][2]-row[ix][2] for row in full]
        if len(set(gaps))<=1:continue
        variable+=1
        if shapes[0][ix][1]!=2 or shapes[0][ix+1][1]!=0:
            raise ValueError("non-CCCA->VTFT variation")
        values=[row[ix][4] for row in full]
        if any(x is None for x in values):
            raise ValueError("source CCCA values missing")
        if monotone_cursor(values):increasing+=1
        if not single_fixed_threshold_possible(values,gaps):rejected+=1
        for row in full:
            if row[ix][7]!=12:raise ValueError("CCCA subcycle counter changed")
            ccca_intra_12+=1
        deltas.update(gaps)
    if (variable,increasing,rejected,ccca_intra_12)!=(8,8,8,72):
        raise ValueError("private original CCCA source signature changed")
    if deltas!={91,93,95}:
        raise ValueError("original x86 TSC class changed")
    return {
        "schema":"P105-PENTIUM-P54C-DYNAREC-CCCA-THRESHOLD-FALSIFIED",
        "authentic_original_stereo_allocations":25,
        "identical_last_full_scans":9,
        "helper_groups_each":92,
        "source_variable_ccca_to_vtft_positions":8,
        "strictly_increasing_ccca_cursor_positions":8,
        "positions_not_explainable_by_one_fixed_sample_end_threshold":8,
        "ccca_12cycle_internal_groups_verified":72,
        "observed_tsc_gap_classes":[91,93,95],
        "original_cpu_model":"Pentium P54C Dynarec as recorded in private 86Box VM config",
        "autonomous_cpu_time_approved":False,
        "causal_branch_ids_proved":False,
    }

def selftest():
    # Mutations and negative controls never require original user media.
    vals=list(range(9))
    assert monotone_cursor(vals)
    assert single_fixed_threshold_possible(vals,[93]*9)
    assert single_fixed_threshold_possible(vals,[91]*4+[93]*5)
    assert not single_fixed_threshold_possible(vals,[93,93,91,93,93,93,93,93,93])
    assert not single_fixed_threshold_possible(vals,[95,95,93,93,91,93,93,93,93])
    assert not single_fixed_threshold_possible(vals,[95,93,91,95,93,91,95,93,91])
    assert not monotone_cursor([0,1,3,2])
    try:single_fixed_threshold_possible([0,1,1],[90,92,94])
    except ValueError:pass
    else:raise AssertionError("ambiguous duplicate cursor survived")
    print("P105_PENTIUM_DYNAREC_SYNTHETIC_NEGATIVE_CONTROLS_PASS")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original-v4-zip",type=Path)
    p.add_argument("--selftest",action="store_true")
    a=p.parse_args()
    if a.selftest:selftest()
    if a.original_v4_zip:
        if digest(a.original_v4_zip)!=SOURCE_SHA:
            raise SystemExit("P105 ORIGINAL V4 SHA MISMATCH")
        print("P105_PRIVATE_ORIGINAL_SOURCE_PASS "+json.dumps(
            summarize(extract_original_runs(a.original_v4_zip)),sort_keys=True))
    elif not a.selftest:
        p.error("provide --selftest or --original-v4-zip")

if __name__=="__main__":
    main()
