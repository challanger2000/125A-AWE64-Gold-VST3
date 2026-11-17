#!/usr/bin/env python3
"""Source-pinned no-EXE Windows DLL build spike for the full 86Box core.
Never alter the upstream checkout in place: use a disposable CI clone.
Do not patch original EMU8000 DSP or x86 decoder or proprietary files.
"""
from pathlib import Path
from argparse import ArgumentParser
import subprocess
import shutil
import hashlib

PINNED="fee1e81977824fd2d5eed4d239589dc30d251dd7"

def git(root: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()

def main():
    ap=ArgumentParser()
    ap.add_argument("--fork", type=Path, required=True)
    args=ap.parse_args()
    fork=args.fork.resolve()
    if git(fork,"rev-parse","HEAD")!=PINNED:
        raise SystemExit("P007_REJECT_WRONG_UPSTREAM_FORK_COMMIT")
    cm=fork/"src/CMakeLists.txt"
    original=cm.read_text(encoding="utf-8")
    before="add_executable(86Box\n    86box.c\n"
    after="add_library(86Box SHARED\n    86box.c\n    P007CoreExports.c\n    P007AudioRing.c\n"
    if original.count(before)!=1:
        raise SystemExit("P007_REJECT_UPSTREAM_BUILD_LAYOUT")
    dst=fork/"src/P007CoreExports.c"
    source=Path(__file__).resolve().parents[1]
    if dst.exists():
        raise SystemExit("P007_REJECT_ALREADY_PATCHED")
    core=(fork/"src/sound/snd_emu8k.c")
    if not core.exists() or not (fork/"src/86box.c").exists():
        raise SystemExit("P007_REJECT_MISSING_CPU_OR_EMU8000")
    dsp_before=hashlib.sha256(core.read_bytes()).hexdigest()
    dst.write_bytes((source/"fork/P007CoreExports.c").read_bytes())
    shutil.copyfile(source/"fork/P007AudioRing.c",fork/"src/P007AudioRing.c")
    include=fork/"src/include/P007CoreABI.h"
    shutil.copyfile(source/"include/P007CoreABI.h",include)
    shutil.copyfile(source/"include/P007AudioRing.h",fork/"src/include/P007AudioRing.h")
    # Connect directly to the ORIGINAL 86Box post-mixer wavetable
    # before its SDL/OpenAL audio device. Never mutate snd_emu8k.c.
    sound_file=fork/"src/sound/sound.c"
    sound_original=sound_file.read_text(encoding="utf-8")
    original_hook="""        if (sound_is_float)
            givealbuffer_wt(outbuffer_w_ex);
        else
            givealbuffer_wt(outbuffer_w_ex_int16);
"""
    replacement_hook="""        /* P007: passive in-process PCM tap of ORIGINAL 86Box mixer. */
        if (sound_is_float)
            p007_audio_capture_float(outbuffer_w_ex, WTBUFLEN);
        else
            p007_audio_capture_pcm16(outbuffer_w_ex_int16, WTBUFLEN);
        if (sound_is_float)
            givealbuffer_wt(outbuffer_w_ex);
        else
            givealbuffer_wt(outbuffer_w_ex_int16);
"""
    if sound_original.count(original_hook)!=1:
        raise SystemExit("P007_REJECT_ORIGINAL_WAVETABLE_SIGNATURE")
    if sound_original.count("#include <86box/sound.h>")!=1:
        raise SystemExit("P007_REJECT_ORIGINAL_WAVETABLE_HEADER")
    sound_file.write_text(sound_original.replace(
        "#include <86box/sound.h>", "#include <86box/sound.h>\\n#include <P007AudioRing.h>",1
    ).replace(original_hook,replacement_hook,1),encoding="utf-8",newline="\\n")

    cm.write_text(original.replace(before,after),encoding="utf-8",newline="\n")
    dsp_after=hashlib.sha256(core.read_bytes()).hexdigest()
    if dsp_before!=dsp_after:
        raise SystemExit("P007_REJECT_ORIGINAL_EMU8000_MODIFIED")
    print("P007_PINNED_86BOX_ORIGINAL_CORE_DLL_CMAKE_PATCH_PASS")
    print("P007_ORIGINAL_EMU8000_UNMODIFIED_SHA256",dsp_after)

if __name__=="__main__":
    main()
