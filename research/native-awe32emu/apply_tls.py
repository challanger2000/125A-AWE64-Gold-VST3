#!/usr/bin/env python3
"""Isolated test-only TLS prototype. Do NOT patch upstream or P007."""
import sys
from pathlib import Path
def replace(p,a,b):
    s=p.read_text(encoding="utf-8")
    if s.count(a)!=1: raise RuntimeError("source mismatch "+str(p))
    p.write_text(s.replace(a,b),encoding="utf-8")
r=Path(sys.argv[1]).resolve()/"AWE32Emu/src"
replace(r/"86box/include/86box/sound.h",
  "extern int music_pos_global;\nextern int wavetable_pos_global;",
  "#if defined(_MSC_VER)\n# define P008_TLS __declspec(thread)\n"
  "#elif defined(__cplusplus)\n# define P008_TLS thread_local\n"
  "#else\n# define P008_TLS _Thread_local\n#endif\n"
  "extern P008_TLS int music_pos_global;\n"
  "extern P008_TLS int wavetable_pos_global;")
replace(r/"Emu8000Box.cpp",
  "    const char* g_romPath = nullptr;",
  "    thread_local const char* g_romPath = nullptr;")
replace(r/"Emu8000Box.cpp",
  "int music_pos_global     = 0;\nint wavetable_pos_global = 0;",
  "thread_local int music_pos_global     = 0;\n"
  "thread_local int wavetable_pos_global = 0;")
print("P008_TLS_NATIVE_PROTOTYPE_ONLY_NO_P007_MODIFICATIONS")
