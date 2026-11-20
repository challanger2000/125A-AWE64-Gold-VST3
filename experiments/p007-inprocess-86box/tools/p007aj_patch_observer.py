#!/usr/bin/env python3
"""P007AJ test-only extension to the pinned ORIGINAL 86Box test fork.
Run AFTER p007_patch_upstream.py. This patches ONLY the existing optional
trace wrapper, NEVER src/sound/snd_emu8k.c, original EMU8000 DSP, CPU,
Creative driver or proprietary Win95 files. Never use in production CI.
"""
from pathlib import Path
from argparse import ArgumentParser
import hashlib
import shutil
import subprocess

PIN="fee1e81977824fd2d5eed4d239589dc30d251dd7"

def must_once(text, anchor):
    if text.count(anchor)!=1:
        raise SystemExit("P007AJ_ORIGINAL_TRACE_ANCHOR_MISMATCH: "+repr(anchor[:85]))

def main():
    ap=ArgumentParser()
    ap.add_argument("--fork",type=Path,required=True)
    args=ap.parse_args()
    fork=args.fork.resolve()
    head=subprocess.check_output(
        ["git","-C",str(fork),"rev-parse","HEAD"],text=True).strip()
    if head!=PIN:
        raise SystemExit("P007AJ_WRONG_ORIGINAL_86BOX_SHA")
    src=Path(__file__).resolve().parents[1]
    original_emu=fork/"src/sound/snd_emu8k.c"
    before_sha=hashlib.sha256(original_emu.read_bytes()).hexdigest()
    dest_source=fork/"src/P007RegisterProbe.c"
    dest_header=fork/"src/include/P007RegisterProbe.h"
    if dest_source.exists() or dest_header.exists():
        raise SystemExit("P007AJ_DUPLICATE_INSTRUMENTED_FORK")
    cm=fork/"src/CMakeLists.txt"
    cmtext=cm.read_text(encoding="utf-8")
    must_once(cmtext,"    P007BootTrace.c\n")
    cmtext=cmtext.replace("    P007BootTrace.c\n",
                         "    P007BootTrace.c\n    P007RegisterProbe.c\n",1)
    trace=fork/"src/sound/snd_emu8k_trace.c"
    traced=trace.read_text(encoding="utf-8")
    include="#include <86box/snd_emu8k_trace.h>"
    must_once(traced,include)
    traced=traced.replace(include,include+
                          "\n#include <P007RegisterProbe.h> /* P007AJ passive QA observer */",1)
    advance="""emu8k_trace_advance(int samples)
{
    if (!trace_checked)
        trace_open();
"""
    write="""emu8k_trace_write(uint16_t addr, uint16_t val)
{
    if (!trace_checked)
        trace_open();
"""
    must_once(traced,advance)
    must_once(traced,write)
    traced=traced.replace(advance,advance+
       "\n    p007aj_probe_advance(samples); /* bounded RAM only; no file */",1)
    traced=traced.replace(write,write+
       "\n    p007aj_probe_write(addr,val); /* no change to original I/O */",1)
    # All preflight validation above happens before ANY fork source write.
    shutil.copyfile(src/"fork/P007RegisterProbe.c",dest_source)
    shutil.copyfile(src/"include/P007RegisterProbe.h",dest_header)
    cm.write_text(cmtext,encoding="utf-8",newline="\n")
    trace.write_text(traced,encoding="utf-8",newline="\n")
    after_sha=hashlib.sha256(original_emu.read_bytes()).hexdigest()
    if before_sha!=after_sha:
        raise SystemExit("P007AJ_ABSOLUTE_FAILURE_ORIGINAL_EMU8000_DSP_MUTATED")
    print("P007AJ_ORIGINAL_EMU8000_UNMODIFIED_SHA256",after_sha)
    print("P007AJ_PASSIVE_BOUNDED_TEST_TRACE_PATCH_NO_DSP_CHANGE_PASS")

if __name__=="__main__":
    main()
