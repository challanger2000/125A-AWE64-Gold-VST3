#!/usr/bin/env python3
"""P192: SF2 omitted sustain generators default to full sustain (0 attenuation).

The Creative SF1/SBK driver-specific defaults and the original 86Box
EMU8000 audio engine are intentionally left untouched.
Applies only to the exact pinned upstream SoundFont.cpp. Fails closed on drift.
"""
from pathlib import Path
import sys

def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: p192_sf2_missing_sustain_default.py <pinned AWE32Emu checkout>")
    path = Path(sys.argv[1]).resolve() / "AWE32Emu" / "src" / "SoundFont.cpp"
    code = path.read_text(encoding="utf-8")
    old = """    auto sustainReg = [&](int op) -> int
    {
        // A missing generator means sustain 0 (decay to silence), not 0x7F.
        // Measured in the voice block at +0x4A (DCYSUSV) and +0x3A (DCYSUS)
        // on all 242 notes; for the piano decaying is right.
        if (!g.Has(op)) return 0;
"""
    new = """    auto sustainReg = [&](int op) -> int
    {
        // P192: omitted SF2 sustainVolEnv/sustainModEnv = zero attenuation,
        // i.e. FULL sustain (SoundFont 2.04 default = 0).
        // The earlier zero-register default was measured on Creative SF1;
        // retain it ONLY for SF1/SBK. Without this distinction a valid SF2
        // looping bass with no explicit gen37 decays to silence immediately.
        if (!g.Has(op)) return sf1 ? 0 : 0x7F;
"""
    if code.count(old) != 1:
        raise RuntimeError("P192 pinned SF2 sustain anchor mismatch or patch already applied")
    path.write_text(code.replace(old, new), encoding="utf-8")
    print("P192_SF2_OMITTED_SUSTAIN_FULL_LEVEL_SF1_UNCHANGED_PATCHED")

if __name__ == "__main__":
    main()
