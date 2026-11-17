#!/usr/bin/env python3
"""P120 ONE-CLICK private original Creative Win95 source-CPU measurement.

Reuses the already tested P003 owner Windows host to SHA-verify the owner's
private original P002 Win95/ROM/Creative VXD zip, clone it, stage the real
Pentium 3-bank stress guest, and run the P119-instrumented ORIGINAL old-86Box
Win64 executable only in that disposable VM clone.

Enables all source-observer traces from ONE guest session. After the user
closes 86Box normally, fail-close the complete P107/P111/P117/P118/P119
JIT-entry/deferred-cycle join and P003 main source-original trace gate.
Produces one PRIVATE measurement zip; no ROM, VXD, Windows image, SF2 or
instrumented product code is put in GitHub.

Original system/CPU/PCM parity must be checked separately; never assert it.
"""
from __future__ import annotations
from datetime import datetime,timezone
import argparse
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import sys
import tempfile
import zipfile

from p003_owner_two_bank_run import (
    choose_private_zip, package_directory, run as owner_run, sha256
)
from p119_validate_original_deferred_jit_cycles_private import (
    join as join119,
)
from p118_join_original_runtime_opcode_site_private import (
    join as join118,
)
from p115_join_p003_original_cpu_snapshot import (
    audit as audit115,
)

ORIGINAL_LCOD_BASE="0xC10151E4"
TRACE_NAMES={
    "p107":"P107-ORIGINAL-EXECUTED-X86.trace",
    "p108":"P108-ORIGINAL-DIRECT-JCC.trace",
    "p110":"P110-ORIGINAL-P54C-TRANSLATION.trace",
    "p111":"P111-ORIGINAL-OLD-JIT-EPOCH.trace",
    "p113":"P113-ORIGINAL-P54C-AGI-EPOCH.trace",
    "p115":"P115-ORIGINAL-LIVE-CPU-TSC.trace",
    "p117":"P117-ORIGINAL-SELECTED-P54C-OPCODE.trace",
    "p118":"P118-ORIGINAL-EXECUTED-JIT-SITE.trace",
    "p119":"P119-ORIGINAL-UNFLUSHED-JIT-CYCLES.trace",
}
MAX_ZIP_MEMBER=270_000_000
MAX_ZIP_TOTAL=850_000_000

def require(ok:bool,reason:str)->None:
    if not ok:
        raise ValueError(reason)

def configure_traces(root:Path)->dict[str,str]:
    """Trace paths stay private/local. No public upload or host heuristics."""
    require(root.is_absolute(),"P120 trace directory must be absolute")
    env={}
    for key,name in TRACE_NAMES.items():
        env["AWE64_"+key.upper()+"_PLACEHOLDER"]=str(root/name)
    # Deliberately write the original probes' EXACT environment keys.
    result={
        "AWE64_P107_EXECUTED_X86_TRACE":env["AWE64_P107_PLACEHOLDER"],
        "AWE64_P108_DIRECT_BRANCH_TRACE":env["AWE64_P108_PLACEHOLDER"],
        "AWE64_P110_TIMING_TRANSLATION_TRACE":env["AWE64_P110_PLACEHOLDER"],
        "AWE64_P111_JIT_BLOCK_JOIN_TRACE":env["AWE64_P111_PLACEHOLDER"],
        "AWE64_P113_ORIGINAL_AGI_EPOCH_TRACE":env["AWE64_P113_PLACEHOLDER"],
        "AWE64_P115_LIVE_TSC_TRACE":env["AWE64_P115_PLACEHOLDER"],
        "AWE64_P117_ORIGINAL_SELECTED_TRACE":env["AWE64_P117_PLACEHOLDER"],
        "AWE64_P118_EXECUTED_SITE_TRACE":env["AWE64_P118_PLACEHOLDER"],
        "AWE64_P119_PENDING_JIT_CYCLES_TRACE":env["AWE64_P119_PLACEHOLDER"],
    }
    for n in ("P107","P110","P111","P113","P117"):
        result["AWE64_"+n+"_LCOD_BASE"]=ORIGINAL_LCOD_BASE
        result["AWE64_"+n+"_LCOD_SELECTOR"]="0x28"
    # No 1996 private guest files are ever made accessible to GitHub.
    return result

def hashed(path:Path)->dict:
    return {
       "bytes":path.stat().st_size,
       "sha256":sha256(path),
    }

def stage_private_zipped_result(result:Path,traces:Path,
                                host_result:Path,report:dict)->None:
    """Create one ZIP containing source traces/WAV (no ROM, VXD or SF2).

    Every input path is exact, internal and generated locally. Never add
    recursively from the private VM directory, which contains Win95 itself.
    """
    approved=[]
    for key,name in TRACE_NAMES.items():
        f=traces/name
        if f.is_file():
            require(0<f.stat().st_size<=MAX_ZIP_MEMBER,
                    "P120 original "+key+" trace over size limit")
            approved.append(("ORIGINAL-FORSCHUNG/"+name,f))
    # The P003 result archive contains original SF2, which must NOT be
    # reproduced in the P120 upload. Copy ONLY approved evidence files.
    allowed={
        "Original-CPU-IO.trace":"P003-ORIGINAL-CPU-IO.trace",
        "Original-Register.trace":"P003-ORIGINAL-EMU8000-REGISTERS.trace",
        "Original-DRY.wav":"P003-ORIGINAL-DRY-PCM.wav",
        "Creative-Win95-Gast.LOG":"P003-ORIGINAL-CREATIVE-GUEST.LOG",
        "Original-Stimmenmodus-Stress.LOG":"P003-ORIGINAL-STRESS.LOG",
    }
    total=sum(f.stat().st_size for _,f in approved)
    require(total<MAX_ZIP_TOTAL,"P120 raw private evidence exceeds cap")
    with zipfile.ZipFile(host_result) as origin:
        members=set(origin.namelist())
        for original_name in set(members)&set(allowed):
            item=origin.getinfo(original_name)
            require(0<item.file_size<=MAX_ZIP_MEMBER,
                    "P120 original P003 source evidence member oversized")
            total+=item.file_size
        require(total<=MAX_ZIP_TOTAL,"P120 evidence total exceeds cap")
        with zipfile.ZipFile(result,"x",compression=zipfile.ZIP_DEFLATED,
                             compresslevel=5,allowZip64=True) as z:
            z.writestr("P120-ORIGINAL-STATUS.json",
                json.dumps(report,indent=2,sort_keys=True,
                           ensure_ascii=False)+"\n")
            z.writestr("START-HIER.txt",
                "125A P120 ORIGINAL CREATIVE WINDOWS95 CPU MEASUREMENT\n"
                "Dieses Archiv enthaelt nur die beobachteten Messspuren.\n"
                "KEIN Win95, keine Creative-Treiber/ROM/SF2-Binaerdateien.\n"
                "Originale Gastadressen/CPU-Zustaende sind privat; bitte NICHT\n"
                "oeffentlich auf GitHub hochladen.\n"
                "Eine originale CPU-Messung ist keine autonome VST3-Abnahme.\n")
            for dst,src in approved:
                z.write(src,dst)
            for name,dst in sorted(allowed.items()):
                if name in members:
                    with origin.open(name,"r") as f:
                        with z.open("ORIGINAL-FORSCHUNG/"+dst,"w",force_zip64=True) as out:
                            shutil.copyfileobj(f,out,1024*1024)

def run(private_archive:Path,original_stress_guest:Path,
        instrumented_emulator:Path,results:Path)->tuple[Path,dict]:
    require(os.name=="nt","P120 original Win95 recording runs on Windows")
    require(private_archive.is_file(),"P120 original private Win95 archive missing")
    require(original_stress_guest.is_file(),"P120 stress Win95 guest PE32 missing")
    require(instrumented_emulator.is_file(),"P120 original 86Box Win64 EXE missing")
    require(instrumented_emulator.name.lower()=="86box.exe",
            "P120 must use the pinned-original 86Box executable by name")
    require(results.is_dir(),"P120 results destination missing")
    stamp=datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    sid=stamp+"-"+secrets.token_hex(4)
    trace_root=results/"PRIVATE"/("P120-"+sid)
    trace_root.mkdir(parents=True,exist_ok=False)
    paths={k:trace_root/v for k,v in TRACE_NAMES.items()}
    saved={k:os.environ.get(k) for k in configure_traces(trace_root)}
    report={
        "schema":"P120-ORIGINAL-CREATIVE-WIN95-P54C-JIT-CYCLE-CAPTURE",
        "session":"P120-"+sid,
        "status":"NO_GO_NOT_RUN",
        "source_only_private_owner_vm":True,
        "original_guest_cycle_cost_proven":False,
        "autonomous_vst3_pcm_parity_proven":False,
        "original_vm_sha256":sha256(private_archive),
        "instrumented_original_86box_sha256":sha256(instrumented_emulator),
        "stressed_guest_sha256":sha256(original_stress_guest),
    }
    result_zip=results/("P120-Original-CPU-Messung-"+sid+"-ERGEBNIS.zip")
    host_zip=None
    try:
        os.environ.update(configure_traces(trace_root))
        # The existing P003 one-click host installs/starts ONLY a temporary
        # cloned VM, verifies 1996 private Creative VXD/ROM hashes and
        # rejects fake/missing original register/audio evidence.
        host_zip,p003=owner_run(private_archive,original_stress_guest,None,
                                results,instrumented_emulator,
                                sha256(instrumented_emulator),
                                original_mode_stress=True)
        report["p003_status"]=p003["status"]
        report["p003_source_archive_sha256"]=sha256(host_zip)
        if not p003["status"].startswith("SOURCE_CREATIVE_"):
            raise ValueError("P120 original Win95 P003/Creative guest NO-GO: "+
                             p003.get("error",p003["status"]))
        for key,file in paths.items():
            require(file.is_file() and file.stat().st_size>0,
                    "P120 original "+key+" observer missing/unrecorded")
        # The primary P003 physical I/O source trace is inside the
        # existing owner P003 result ZIP; inspect only that whitelisted
        # member, never import ROM/Win95/SF2 from the private archive.
        with tempfile.TemporaryDirectory(dir=trace_root,
                                         prefix="p120-source-io-") as td:
            io=Path(td)/"ORIGINAL-P003-CPU-IO.trace"
            with zipfile.ZipFile(host_zip) as source:
                require("Original-CPU-IO.trace" in source.namelist(),
                        "P120 original physical CPU I/O trace absent")
                with source.open("Original-CPU-IO.trace") as src,io.open("xb") as dst:
                    shutil.copyfileobj(src,dst,1024*1024)
            report["p115_original_physical_io_clock_join"]=audit115(
                io,paths["p115"],minimum=100)
        p118_join=join118(paths["p107"],paths["p111"],paths["p117"],
                           paths["p118"],paths["p110"],paths["p113"],
                           minimum=100)
        report["p118_original_jit_source_join"]=p118_join
        p119_join=join119(paths["p118"],paths["p119"],minimum=100)
        report["p119_original_pending_cpu_join"]=p119_join
        report["private_observer_trace_identifiers"]={
            key:hashed(f) for key,f in paths.items()
        }
        report["status"]="SOURCE_ORIGINAL_WIN95_JIT_CPU_PROVENANCE_CAPTURED"
    except (OSError,ValueError,zipfile.BadZipFile) as e:
        report["status"]="NO_GO"
        report["error"]=str(e)
    finally:
        for k,old in saved.items():
            if old is None:os.environ.pop(k,None)
            else:os.environ[k]=old
        # Even a failure produces a private diagnostic ZIP when P003
        # produced original evidence. Never publish private original data.
        if host_zip is not None and host_zip.is_file():
            stage_private_zipped_result(result_zip,trace_root,host_zip,report)
        else:
            with zipfile.ZipFile(result_zip,"x") as z:
                z.writestr("P120-ORIGINAL-STATUS.json",
                    json.dumps(report,indent=2,ensure_ascii=False)+"\n")
    return result_zip,report

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--private-original-archive",type=Path)
    ap.add_argument("--stress-guest",type=Path)
    ap.add_argument("--instrumented-86box",type=Path)
    ap.add_argument("--results-root",type=Path)
    a=ap.parse_args()
    root=package_directory()
    try:
        private=choose_private_zip(root,a.private_original_archive)
        guest=a.stress_guest or root/"P002RAM.EXE"
        exe=a.instrumented_86box or root/"86Box.exe"
        output=a.results_root or root
        output.mkdir(parents=True,exist_ok=True)
        result,report=run(private,guest,exe,output)
        print("\nP120 Ergebnis: "+str(result))
        print("Status: "+report["status"])
        if "error" in report:print("Grund: "+report["error"])
        return 0 if report["status"].startswith("SOURCE_ORIGINAL_") else 2
    except (ValueError,OSError) as e:
        print("P120_NO_GO "+str(e),file=sys.stderr)
        return 2

if __name__=="__main__":raise SystemExit(main())
