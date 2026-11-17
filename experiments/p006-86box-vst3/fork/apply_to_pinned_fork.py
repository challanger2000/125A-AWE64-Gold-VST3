#!/usr/bin/env python3
"""Patch a PRIVATE clone of the PINNED 86Box-AWE32 fork with a passive
wavetable capture hook. No modification to snd_emu8k.c, chip DSP, or
original Windows95 Creative driver. No ROM/VXD/Win95 private files.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import shutil
import subprocess

PINNED_COMMIT="fee1e81977824fd2d5eed4d239589dc30d251dd7"
PINNED_SOUND_BLOB="afdab37d332cbf7e2cfc18c379175c551d3373ec"
def git(fork:Path,*args:str)->str:
 return subprocess.check_output(["git","-C",str(fork),*args],text=True).strip()
def main()->None:
 p=argparse.ArgumentParser()
 p.add_argument("--fork",type=Path,required=True)
 args=p.parse_args()
 original=args.fork.resolve()
 src=original/"src/sound/sound.c"
 cmake=original/"src/sound/CMakeLists.txt"
 here=Path(__file__).resolve().parent
 if not src.is_file() or not cmake.is_file():
  raise SystemExit("P006 ERROR: no 86Box source")
 if git(original,"rev-parse","HEAD")!=PINNED_COMMIT:
  raise SystemExit("P006 ERROR: wrong fork commit, refusing patch")
 if git(original,"hash-object",str(src))!=PINNED_SOUND_BLOB:
  raise SystemExit("P006 ERROR: original sound.c modified, refusing patch")
 sound=src.read_text(encoding="utf-8")
 cm=cmake.read_text(encoding="utf-8")
 target="void\nwavetable_poll("
 decl="extern void p006_capture_wavetable(void const *,int,uint32_t);\n\n"+target
 if sound.count(target)!=1:
  raise SystemExit("P006 ERROR: wavetable_poll anchor missing")
 before="""        if (sound_is_float)
            givealbuffer_wt(outbuffer_w_ex);
        else
            givealbuffer_wt(outbuffer_w_ex_int16);"""
 after="""        if (sound_is_float) {
            p006_capture_wavetable(outbuffer_w_ex,1,WTBUFLEN);
            givealbuffer_wt(outbuffer_w_ex);
        } else {
            p006_capture_wavetable(outbuffer_w_ex_int16,0,WTBUFLEN);
            givealbuffer_wt(outbuffer_w_ex_int16);
        }"""
 if sound.count(before)!=1 or cm.count("    snd_emu8k.c\n")!=1:
  raise SystemExit("P006 ERROR: pinned code layout differs")
 if "P006ForkAudioCapture.c" in cm:
  raise SystemExit("P006 ERROR: already patched")
 sound=sound.replace(target,decl).replace(before,after)
 cm=cm.replace("    snd_emu8k.c\n",
                "    snd_emu8k.c\n    P006ForkAudioCapture.c\n")
 # Verify every anchor before any on-disk source mutation.
 src.write_text(sound,encoding="utf-8",newline="\n")
 cmake.write_text(cm,encoding="utf-8",newline="\n")
 shutil.copyfile(here/"P006ForkAudioCapture.c",
                 original/"src/sound/P006ForkAudioCapture.c")
 shutil.copyfile(here.parent/"bridge/P006Wire.h",
                 original/"src/sound/P006Wire.h")
 print("P006_FORK_AUDIO_HOOK_APPLIED; ORIGINAL_EMU8000_UNCHANGED")
if __name__=="__main__":
 main()
