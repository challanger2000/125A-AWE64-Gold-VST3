#!/usr/bin/env python3
"""P205: source-backed Creative Win95 SF2 voice allocator VTFT reset."""
from pathlib import Path
import sys

def one(s,a,b,n):
    if s.count(a)!=1: raise RuntimeError(f"P205 pinned {n} anchor "+str(s.count(a)))
    return s.replace(a,b)

def main():
    if len(sys.argv)!=2: raise SystemExit("usage: P205.py <upstream>")
    p=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"/"Synth.cpp"
    s=p.read_text(encoding="utf-8")
    if "P205_ORIGINAL_ALLOC_VTFT" in s: raise RuntimeError("P205 duplicate")
    s=one(s,"""            const int base = AllocateStereoPairVxd(0xFFFE);
            if (base < 0) continue;
            // VXD physical row+0x17""","""            const int base = AllocateStereoPairVxd(0xFFFE);
            if (base < 0) continue;
            // P205_ORIGINAL_ALLOC_VTFT: pair allocator FIRST stops both
            // physical voices; original SBAWE.VXD then resets each
            // VTFT exactly once, before the next allocation or main.
#ifndef P205_NEGATIVE_SKIP_ALLOC_RESET
            m_core.Write(Reg::VTFT, base, 0x0000FFFFu);
            m_core.Write(Reg::VTFT, base + 1, 0x0000FFFFu);
#endif
            // VXD physical row+0x17""","pair")
    s=one(s,"""            const int voice = AllocateVoiceVxd(0xFFFE);
            if (voice < 0) continue;
            SetVxdPhysicalRomClass(voice,""","""            const int voice = AllocateVoiceVxd(0xFFFE);
            if (voice < 0) continue;
            // Original physical voice allocation stop followed by
            // VTFT=0000FFFF (distinct from two later main-body resets).
#ifndef P205_NEGATIVE_SKIP_ALLOC_RESET
            m_core.Write(Reg::VTFT, voice, 0x0000FFFFu);
#endif
            SetVxdPhysicalRomClass(voice,""","mono")
    p.write_text(s,encoding="utf-8")
    print("P205_CREATIVE_ORIGINAL_VXD_SF2_VTFT_ALLOC_PROLOGUES_PATCHED")
if __name__=="__main__": main()
