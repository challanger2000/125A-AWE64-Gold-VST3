#!/usr/bin/env python3
"""P108 PRIVATE original-Win95 Creative CPU/Jcc reproduction, no user handoff.

Use only inside the private, isolated original Win95/86Box environment.
Does not upload any original guest/DLL/VxD, ROM, SF2 or trace to GitHub.

Steps: SHA-verify original VM and exact known stress PE32/SF2; create a fresh
disposable FAT16 VM clone; inject the exact original P003 3-bank stress guest;
launch the *already compiled* P108-instrumented old-x64 86Box; auto-watch
the guest's completed P003MODE.LOG; request graceful X11 application exit;
after 86Box exits, run strict independent P107/P108 x86-I/Jcc-B comparison.
Output a local, small SHA/counter-only report. Any missing data = NO GO.

A stalled Win95 guest, emulator crash or missing UI-close support is NO GO,
never silently treated as a successful CPU test.
"""
from __future__ import annotations
import argparse
from datetime import datetime,timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from p003_owner_two_bank_run import (
    PINNED_OWNER_SF2_SHA,PINNED_ROM_SHA,PINNED_VXD_SHA,PINNED_DLL_SHA
)
from p003_private_fat16_replace import patch_disposable_guest
from p002_extract_original_win95_ram_log_fat16 import PrivateFat16Reader
from p003_verify_original_win95_mode_stress_log import validate_original_stress_log
from p108_verify_direct_jcc import read_direct,compare_original
from p110_audit_original_p54c_translation_private import audit as audit_p110_translator
from p108_resolve_private_original_package import resolve as resolve_original_v4_package
from p107_audit_executed_jcc_private import read_trace

STRESS_EXE_SHA="3c9306261ed599073f80d9695a8d73140ea65e2de101dc431169c2e6b3f6bc9f"
STRESS_SECOND_SF2_SHA="cfeea9897fce687f97160e069f01ae785e7e736798290a0e07ce56af7d8326fb"
ORIGINAL_LCOD_RUNTIME_BASE="0xC10151E4"
TRACE_I_NAME="P108-PRIVATE-EXECUTED-I.trace"
TRACE_B_NAME="P108-PRIVATE-DIRECT-JCC-B.trace"
ROOT_LOG=b"P003MODELOG"
MAX_RUN_SECONDS=2400

def sha_file(path:Path)->str:
    digest=hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda:file.read(1024*1024),b""):
            digest.update(chunk)
    return digest.hexdigest()

def require(ok,reason:str):
    if not ok:raise ValueError(reason)

def verify_original_guest_sources(original_vm:Path,guest_exe:Path,bank:Path):
    disk=original_vm/"VM"/"win95.img"
    rom=original_vm/"86Box"/"roms"/"sound"/"creative"/"awe32.raw"
    config=original_vm/"VM"/"86box.cfg"
    require(disk.is_file() and disk.stat().st_size==536870912,
            "original 512 MiB FAT16 Windows 95 source missing")
    require(config.is_file(),"original 86Box VM configuration missing")
    require(rom.is_file() and sha_file(rom)==PINNED_ROM_SHA,
            "authentic original 86Box AWE32 ROM differs")
    require(guest_exe.is_file() and sha_file(guest_exe)==STRESS_EXE_SHA,
            "not the original 2026-11-16 3-bank Win95 stress PE32")
    require(bank.is_file() and sha_file(bank)==STRESS_SECOND_SF2_SHA,
            "not the original 2026-11-16 comparison SoundFont")
    with PrivateFat16Reader(disk) as reader:
        require(hashlib.sha256(reader.read_root_file(
            b"P002RAM SF2",32*1024*1024)).hexdigest()==PINNED_OWNER_SF2_SHA,
            "original first bank mismatch")
        for key,expected in ((b"SBAWE   VXD",PINNED_VXD_SHA),
                             (b"AWEMAN32DLL",PINNED_DLL_SHA)):
            require(hashlib.sha256(reader.read_short_path_file(
                (b"WINDOWS    ",b"SYSTEM     ",key),
                2*1024*1024)).hexdigest()==expected,
                "original private Creative Win95 binary mismatch")
        for key in (b"P003BANKSF2",ROOT_LOG):
            try:reader.read_root_file(key,32*1024*1024)
            except ValueError:pass
            else:raise ValueError("source VM already contains staged stress file: refuse")
    return {"original_disk_sha256":sha_file(disk),
            "original_rom_sha256":PINNED_ROM_SHA,
            "original_guest_stress_sha256":STRESS_EXE_SHA,
            "original_second_sf2_sha256":STRESS_SECOND_SF2_SHA}

def make_probe_environment(environ,trace_i:Path,trace_b:Path,io_path:Path,
                           trace_p110:Path|None=None):
    env=environ.copy()
    env["AWE64_P107_EXECUTED_X86_TRACE"]=str(trace_i)
    env["AWE64_P108_DIRECT_BRANCH_TRACE"]=str(trace_b)
    env["AWE64_P107_LCOD_BASE"]=ORIGINAL_LCOD_RUNTIME_BASE
    env["AWE64_P107_LCOD_SELECTOR"]="0x28"
    env["AWE64_P003_CPU_IO_TRACE"]=str(io_path)
    env["AWE64_P003_GUEST_CALLSTACK"]="1"
    for key in ("AWE64_P110_TIMING_TRANSLATION_TRACE",
                "AWE64_P110_LCOD_BASE","AWE64_P110_LCOD_SELECTOR"):
        env.pop(key,None)
    if trace_p110 is not None:
        env["AWE64_P110_TIMING_TRANSLATION_TRACE"]=str(trace_p110)
        env["AWE64_P110_LCOD_BASE"]=ORIGINAL_LCOD_RUNTIME_BASE
        env["AWE64_P110_LCOD_SELECTOR"]="0x28"
    return env

def completed_original_guest(disk:Path):
    try:
        with PrivateFat16Reader(disk) as reader:
            log=reader.read_root_file(ROOT_LOG,16384)
        proof=validate_original_stress_log(log)
        return proof
    except (OSError,ValueError,UnicodeError):
        return None

def close_x11_emulator_gracefully(env,exe_name="86Box"):
    """Request WM_DELETE on a visible 86Box window. Never kill/wipe original."""
    if not env.get("DISPLAY") or not shutil.which("xdotool"):
        return False
    try:
        r=subprocess.run(["xdotool","search","--onlyvisible","--name",exe_name],
                         env=env,text=True,capture_output=True,timeout=4,check=False)
        windows=r.stdout.split()
        if r.returncode or not windows:return False
        subprocess.run(["xdotool","windowclose",windows[-1]],env=env,
                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,
                       timeout=4,check=False)
        return True
    except (OSError,subprocess.TimeoutExpired):
        return False

def watch_guest_until_done(proc,disk,env,max_run_seconds,poll_seconds=3):
    deadline=time.monotonic()+max_run_seconds
    proof=None
    while time.monotonic()<deadline:
        if proc.poll() is not None:
            raise ValueError("86Box exited before completed original Creative 3-bank log")
        proof=completed_original_guest(disk)
        if proof:
            # Avoid racing in-flight disk write / asynchronous audio callbacks.
            time.sleep(3)
            stable=completed_original_guest(disk)
            if stable==proof:break
            proof=None
        time.sleep(poll_seconds)
    require(proof is not None,"Windows-95 guest stress timed out or did not complete")
    require(close_x11_emulator_gracefully(env),
            "guest completed but no graceful local X11 86Box close path")
    try:code=proc.wait(timeout=45)
    except subprocess.TimeoutExpired:
        raise ValueError("86Box did not shut down cleanly after original stress")
    require(code==0,"86Box guest runtime exited with nonzero result")
    return proof

P003_PHYSICAL_HEADER=b"# P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK "
EMU8K_PHYSICAL_PORTS=frozenset((0xe22,0xe20,0xa20,0xa22,0x620,0x622))

def verify_original_physical_cpu_io(path:Path,min_samples:int=100000):
    """Streaming proof: real SHA-pinned 86Box physical 8/16/32 callbacks.

    Strict contiguous original physical I/O sequence and Pentium TSC;
    never expose private CPU GPRs/stack or raw port readings.
    """
    require(path is not None and path.is_file() and 10_000<path.stat().st_size<400_000_000,
            "authentic P003 original source physical IO trace missing/oversized")
    digest=hashlib.sha256()
    count=0
    previous_tsc=None
    first_tsc=None
    with path.open("rb") as stream:
        first=stream.readline()
        digest.update(first)
        require(first.startswith(P003_PHYSICAL_HEADER),
                "P003 original physical guest CPU V4-stack header missing")
        for raw in stream:
            digest.update(raw)
            chunks=raw.split()
            require(len(chunks)==17 and chunks[0] in (b"r",b"w"),
                    "original physical I/O callback layout invalid")
            try:
                tsc=int(chunks[1],10)
                port=int(chunks[5],16)
                seq=int(chunks[7],10)
            except ValueError as exc:
                raise ValueError("malformed physical guest callback") from exc
            if first_tsc is None:first_tsc=tsc
            require(tsc>=0 and (previous_tsc is None or tsc>=previous_tsc),
                    "original physical CPU-TSC regressed")
            require(port in EMU8K_PHYSICAL_PORTS,
                    "original physical callback port outside EMU8000 bus")
            require(seq==count, "original physical callback sequence missing or duplicated")
            previous_tsc=tsc
            count+=1
            require(count<=3_000_000, "original physical callback count over cap")
    require(count>=min_samples,
            "too few original physical EMU8000 register callbacks")
    return {"original_physical_callback_count":count,
            "original_tsc_first":first_tsc,
            "original_tsc_last":previous_tsc,
            "original_physical_io_sha256":digest.hexdigest(),
            "original_p54c_cpu_tsc_reconstructed":False}

def verify_private_capture(disk:Path,trace_i:Path,trace_b:Path,trace_io:Path,
                           trace_p110:Path|None=None):
    proof=completed_original_guest(disk)
    require(proof is not None,"guest original Creative 3-bank stress result missing")
    rows,original_i_sha=read_trace(trace_i)
    branches,original_b_sha=read_direct(trace_b)
    checked=compare_original(rows,branches)
    require(trace_io is not None,"original EMU8000 physical CPU V4 I/O evidence required")
    physical=verify_original_physical_cpu_io(trace_io)
    require(physical["original_tsc_first"]<=rows[0].tsc and
            rows[-1].tsc<=physical["original_tsc_last"],
            "original Jcc and physical EMU8000 I/O have disjoint guest-TSC windows")
    require(checked["executed_jcc_total"]>=3000,
            "not sufficient original Creative stereo x86 Jcc executions")
    require(checked["unobserved_or_unexplained_jcc"]==0,
            "P108 direct original x86 flag has unconfirmed Jcc")
    require(checked["next_pc_independently_confirms_flags"]>=3000,
            "too few independently matching branch decisions")
    translator=audit_p110_translator(
        trace_p110,min_translations=50) if trace_p110 is not None else None
    return {
        "original_p110_compile_time_evidence":translator,
        "schema":"P108-OWNER-ORIGINAL-WIN95-3BANK-LAZY-FLAG-CPU-CROSSCHECK",
        "status":"ORIGINAL_X86_JCC_DIRECT_FLAGS_PASS_NOT_PCM_TIMING",
        "original_guest_midi_calls":proof["original_win95_midi_send_calls"],
        "original_guest_midi_api_failures":proof["original_win95_midi_api_failures"],
        "p107_trace_sha256":original_i_sha,
        "p108_direct_jcc_sha256":original_b_sha,
        "independent_flags_next_pc_audit":checked,
        "original_cpu_io_evidence":physical,
        "original_tsc_reproduction_proved":False,
        "native_vst3_pcm_bit_exact_proved":False,
        "private_trace_or_creative_rom_published":False
    }

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--private-original-vm-root",type=Path)
    p.add_argument("--private-original-v4-package",type=Path)
    p.add_argument("--private-original-stress-pe32",type=Path)
    p.add_argument("--private-original-comparison-sf2",type=Path)
    p.add_argument("--p108-instrumented-86box",type=Path)
    p.add_argument("--p108-instrumented-86box-sha256")
    p.add_argument("--private-result-root",type=Path,required=True)
    p.add_argument("--existing-guest-image",type=Path,
                   help="Only audit a previously executed PRIVATE capture")
    p.add_argument("--existing-p107-i",type=Path)
    p.add_argument("--existing-p108-b",type=Path)
    p.add_argument("--existing-cpu-io",type=Path)
    p.add_argument("--require-p110-translator",action="store_true")
    p.add_argument("--existing-p110-translator",type=Path)
    p.add_argument("--max-run-seconds",type=int,default=1200)
    args=p.parse_args()
    try:
        # The read-only audit mode never starts VM, modifies FAT or
        # even opens the original VM archive.
        if args.existing_guest_image:
            require(args.existing_p107_i and args.existing_p108_b and
                    args.existing_cpu_io,
                    "existing PRIVATE audit requires P107-I P108-B AND original physical CPU V4 I/O traces")
            require(not args.require_p110_translator or
                    args.existing_p110_translator,
                    "original P110 translator trace required but missing")
            result=verify_private_capture(args.existing_guest_image,
                                          args.existing_p107_i,args.existing_p108_b,
                                          args.existing_cpu_io,
                                          args.existing_p110_translator)
            print("P108_PRIVATE_ORIGINAL_EXECUTION_AUDIT "+json.dumps(result,sort_keys=True))
            return 0
        require(args.p108_instrumented_86box and args.p108_instrumented_86box_sha256,
                "P108 instrumented original 86Box binary and SHA are mandatory")
        require(0<args.max_run_seconds<=MAX_RUN_SECONDS,
                "bounded original 86Box session required")
        output=args.private_result_root.resolve()
        require(not args.private_original_v4_package or
                (args.private_original_vm_root is None and
                 args.private_original_stress_pe32 is None and
                 args.private_original_comparison_sf2 is None),
                "original V4 Gesamtpaket and individual source paths cannot be mixed")
        if args.private_original_v4_package:
            v4=args.private_original_v4_package.resolve()
            require(output!=v4 and v4 not in output.parents,
                    "original private V4 source cannot be overwritten")
            output.mkdir(parents=True,exist_ok=True)
            prepared=resolve_original_v4_package(v4,output/("P108-RESOLVED-V4-PRIVATE-"+
                datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")))
            source=Path(prepared["private_original_vm_root"]).resolve()
            stress=Path(prepared["private_original_stress_pe32"])
            comparison=Path(prepared["private_original_comparison_sf2"])
        else:
            require(all((args.private_original_vm_root,
                         args.private_original_stress_pe32,
                         args.private_original_comparison_sf2)),
                    "private original VM, exact stress PE32 and SF2 required")
            source=args.private_original_vm_root.resolve()
            stress=args.private_original_stress_pe32
            comparison=args.private_original_comparison_sf2
            require(output!=source and source not in output.parents,
                    "output must remain outside original private VM source")
        require(args.p108_instrumented_86box.is_file() and
                sha_file(args.p108_instrumented_86box)==
                args.p108_instrumented_86box_sha256.lower(),
                "P108 instrumented emulator SHA mismatch")
        source_report=verify_original_guest_sources(
            source,stress,
            comparison)
        output.mkdir(parents=True,exist_ok=True)
        session=output/("P108-PRIVATE-"+datetime.now(timezone.utc).strftime(
            "%Y%m%dT%H%M%S%fZ"))
        session.mkdir(exist_ok=False)
        vm=session/"ORIGINAL-VM-THROWAWAY"
        shutil.copytree(source,vm,symlinks=False)
        disk=vm/"VM"/"win95.img"
        # Clone must initially be IDENTICAL; do not write any owner original.
        require(sha_file(disk)==source_report["original_disk_sha256"],
                "private Win95 disposable clone differs before guest staging")
        stage=patch_disposable_guest(
            disk,stress.read_bytes(),
            comparison.read_bytes(),
            PINNED_OWNER_SF2_SHA)
        trace_i=session/TRACE_I_NAME
        trace_b=session/TRACE_B_NAME
        io_trace=session/"P108-PRIVATE-ORIGINAL-CPU-IO.trace"
        p110_trace=(session/"P110-PRIVATE-ORIGINAL-PENTIUM-COMPILE.trace"
                    if args.require_p110_translator else None)
        env=make_probe_environment(os.environ,trace_i,trace_b,io_trace,p110_trace)
        exe=args.p108_instrumented_86box.resolve()
        argv=[str(exe),"-P",str(vm/"VM"),"-R",str(vm/"86Box"/"roms")]
        p86=subprocess.Popen(argv,cwd=vm/"86Box",env=env)
        try:watch_guest_until_done(p86,disk,env,args.max_run_seconds)
        finally:
            if p86.poll() is None:
                p86.terminate()
                try:p86.wait(timeout=20)
                except subprocess.TimeoutExpired:p86.kill();p86.wait()
        report=verify_private_capture(disk,trace_i,trace_b,io_trace,p110_trace)
        report["source_identifiers"]=source_report
        report["original_guest_clone_stage"]=stage["status"]
        report["instrumented_86box_sha256"]=sha_file(exe)
        target=session/"P108-PRIVATE-REDACTED-REPORT.json"
        target.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n",
                          encoding="utf8")
        print("P108_PRIVATE_ORIGINAL_WIN95_JCC_DIRECT_FLAGS_PASS "+json.dumps({
          "session_redacted_report_sha256":sha_file(target),
          "original_jcc":report["independent_flags_next_pc_audit"]["executed_jcc_total"],
          "all_original_branches_crosschecked":True,
          "autonomous_tsc_parity":False,"native_vst3_pcm_parity":False
        },sort_keys=True))
        return 0
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as exc:
        print("P108_PRIVATE_ORIGINAL_WIN95_CAPTURE_NO_GO "+str(exc),file=sys.stderr)
        return 2

if __name__=="__main__":
    sys.exit(main())
