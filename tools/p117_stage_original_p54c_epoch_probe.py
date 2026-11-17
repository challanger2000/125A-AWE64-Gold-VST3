#!/usr/bin/env python3
"""Stage pinned original 86Box P107/P108/P110/P111/P113/P117 in disposable copy.
Uses existing proven patch scripts; preserves all product and DSP sources.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

SCRIPT=Path(__file__).resolve().parent
def call(*args:str)->None:
    subprocess.run([sys.executable,str(SCRIPT/args[0]),*args[1:]],check=True)

def stage(original:Path,dest:Path)->None:
    if not original.is_dir() or dest.resolve()==original.resolve() or dest.exists():
        raise ValueError('P117 refuses missing original, identical/old destination')
    if not (original/'src/cpu/386_dynarec.c').is_file():
        raise ValueError('P117 original 86Box CPU file missing')
    shutil.copytree(original,dest,ignore=shutil.ignore_patterns('.git','build','build-*'))
    codegen=dest/'src/codegen/codegen_x86-64.c'
    dynarec=dest/'src/cpu/386_dynarec.c'
    pentium=dest/'src/cpu/codegen_timing_pentium.c'
    io=dest/'src/io.c'
    io_scratch=dest/'src/io.p003.p117.tmp'
    call('p003_instrument_86box_io.py',
        '--original-io-c',str(original/'src/io.c'),
        '--output-io-c',str(io_scratch))
    io_scratch.replace(io)
    scratch=dest/'src/codegen/codegen_x86-64.p117.tmp'
    call('p107_instrument_old_dynarec.py',
        '--original-codegen',str(original/'src/codegen/codegen_x86-64.c'),
        '--patched-output',str(scratch))
    scratch.replace(codegen)
    call('p107_patch_old_dynarec_interpreter_fallback.py',
        '--original-dynarec',str(original/'src/cpu/386_dynarec.c'),
        '--copy-dynarec',str(dynarec),
        '--copy-old-codegen',str(codegen))
    call('p108_direct_executed_jcc_flags.py',
        '--copy-old-codegen',str(codegen))
    call('p110_instrument_original_p54c_compile_timing.py',
        '--original-pentium-timing',str(original/'src/cpu/codegen_timing_pentium.c'),
        '--copy-pentium-timing',str(pentium),
        '--copy-old-codegen',str(codegen))
    call('p111_join_original_p54c_jit_compile_execute.py',
        '--original-dynarec',str(original/'src/cpu/386_dynarec.c'),
        '--copy-dynarec',str(dynarec),
        '--copy-old-codegen',str(codegen))
    call('p113_tag_original_p110_opcode_agi_with_p111_compile_epoch.py',
        '--original-pentium-timing',str(original/'src/cpu/codegen_timing_pentium.c'),
        '--copy-pentium-timing',str(pentium),
        '--copy-old-codegen',str(codegen))
    call('p117_tag_original_p54c_selected_opcode_epoch.py',
        '--original-pentium',str(original/'src/cpu/codegen_timing_pentium.c'),
        '--copy-pentium',str(pentium),
        '--copy-old-codegen',str(codegen))
    call('p115_original_live_cpu_tsc_probe.py',
        '--original-dynarec',str(original/'src/cpu/386_dynarec.c'),
        '--copy-dynarec',str(dynarec),
        '--copy-p003-io',str(io))
    call('p118_bind_runtime_jit_to_p117_opcode.py',
        '--pristine-old-codegen',str(original/'src/codegen/codegen_x86-64.c'),
        '--disposable-old-codegen',str(codegen))
    call('p119_capture_original_pending_jit_cycles.py',
        '--pristine-acc',str(original/'src/codegen/codegen_accumulate_x86-64.c'),
        '--copy-acc',str(dest/'src/codegen/codegen_accumulate_x86-64.c'),
        '--pristine-old-codegen',str(original/'src/codegen/codegen_x86-64.c'),
        '--copy-old-codegen',str(codegen))
    for preserve in ('src/sound/snd_emu8k.c','src/sound/snd_sb.c'):
        if (original/preserve).read_bytes() != (dest/preserve).read_bytes():
            raise ValueError('P117 forbidden original sound core mutation')
    if (b'P115_ORIGINAL_DYNAREC_LIVE_CPU_TSC_V1' not in dynarec.read_bytes() or
        b'P115_ORIGINAL_DYNAREC_LIVE_CPU_TSC_V1' not in io.read_bytes()):
        raise ValueError('P117 P003/P115 original clock join absent')
    print('P117_STAGE_ORIGINAL_86BOX_COPY_PROBES_PASS P003_P115_P117_P118_P119_SAME_GUEST_BUILD_READY=TRUE source_original_unchanged=TRUE')

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--original-root',type=Path,required=True)
    ap.add_argument('--disposable-root',type=Path,required=True)
    a=ap.parse_args()
    try:stage(a.original_root,a.disposable_root)
    except (OSError,ValueError,subprocess.CalledProcessError) as ex:
        print('P117_STAGE_NO_GO '+str(ex),file=sys.stderr);return 2
    return 0
if __name__=='__main__':raise SystemExit(main())
