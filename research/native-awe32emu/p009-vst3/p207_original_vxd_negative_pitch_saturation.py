#!/usr/bin/env python3
"""P207: original Win95 SBAWE.VXD unsigned pitch-word saturation.

LCOD+0x6F56 signed bend delta is added to the unsigned voice base;
the original driver compares the resulting x86 DWORD *unsigned* to
0xFFFF before checking its sign. Therefore a negative total wraps to
0xFFFFFFFF... and becomes IP=0xFFFF, not zero as std::clamp did.
This behavior belongs to the Win95 VXD only; DOS/SDK remain untouched.
Both an already sounding voice and a note started under held bend must
use this identical original mapping. See original independent port in
third_party/creative-vxd-port/src/OriginalCreativeMidiEngine.cpp.
"""
from pathlib import Path
import sys

def one(s,o,n,label):
    count=s.count(o)
    if count!=1: raise RuntimeError(f"P207 pinned {label} anchor count={count}")
    return s.replace(o,n)

def main():
    if len(sys.argv)!=2: raise SystemExit("usage: p207.py <upstream>")
    path=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"/"Synth.cpp"
    s=path.read_text(encoding="utf-8")
    if "P207_ORIGINAL_VXD_PITCH_SAT" in s: raise RuntimeError("P207 repeated")
    s=one(s,"""// ---------------------------------------------------------------------------
// Starting a voice
// ---------------------------------------------------------------------------""","""// P207_ORIGINAL_VXD_PITCH_SAT: original Win95 LCOD+0x6F56 executes
// ADD then unsigned CMP to 0xffff, JA saturating to 0xffff.
// Negative signed sums wrap to large *unsigned* DWORDs and must
// saturate HIGH to FFFF, not LOW to zero. No alternative DSP.
namespace {
uint16_t OriginalVxdPitchWord(int base, int delta) noexcept
{
#ifdef P207_NEGATIVE_OLD_SIGNED_CLAMP
    return static_cast<uint16_t>(std::clamp(base + delta, 0, 65535));
#else
    const uint32_t sum = static_cast<uint32_t>(
        static_cast<int32_t>(base) + static_cast<int32_t>(delta));
    return static_cast<uint16_t>(sum > 0xFFFFu ? 0xFFFFu : sum);
#endif
}
}

// ---------------------------------------------------------------------------
// Starting a voice
// ---------------------------------------------------------------------------""","helper")
    s=one(s,"""        : std::clamp(vp.ip + PitchBendOffset(channel), 0, 65535);""",
"""        : OriginalVxdPitchWord(vp.ip, PitchBendOffset(channel));""","bent note start")
    s=one(s,"""        const uint16_t pitch = static_cast<uint16_t>(std::clamp(a.basePitch + bend, 0, 65535));""",
"""        const uint16_t pitch = vxd
            ? OriginalVxdPitchWord(a.basePitch, bend)
            : static_cast<uint16_t>(std::clamp(a.basePitch + bend, 0, 65535));""","live pitch bend")
    path.write_text(s,encoding="utf-8")
    print("P207_ORIGINAL_CREATIVE_VXD_UNSIGNED_PITCH_SATURATION_BOTH_PATHS_PATCHED")

if __name__=="__main__": main()
