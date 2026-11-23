#!/usr/bin/env python3
"""P217: original Creative SBAWE.VXD SF2 IP conversion for 44.1k source samples.

Evidence: original LCOD x86 control path has IMUL ... 0x36A (874) and
SAR ... 8, versus the legacy SF1-derived 4096/1200 approximation.
The former differs by an LSB on negative octave-shifted SF2 notes.
The full original VXD is NOT copied; only independently verified
numeric arithmetic is implemented. No private user SoundFont or ROM
bytes are embedded here.
"""
from pathlib import Path
import sys
if len(sys.argv)!=2:
    raise SystemExit("usage: p217_original_creative_vxd_pitch.py PINNED_UPSTREAM")
source=Path(sys.argv[1])/"AWE32Emu/src/SoundFont.cpp"
data=source.read_text(encoding="utf-8-sig")
anchor="""    vp.ip = static_cast<uint16_t>(std::clamp(PitchFromCents(centsTotal), 0, 65535));"""
new="""    // P217 original SBAWE.VXD LCOD: signed cents multiplied by 0x36A
    // (=874), then SAR 8. An arithmetic right-shift ROUNDS NEGATIVE
    // fixed-point values DOWN, unlike C++ signed division or the older
    // octave/rem 4096/1200 formula. The measured Win95 SF2 case
    // -1206 cents produces 0xCFEA, not 0xCFEB; -6 remains 0xDFEB.
    // Only the Win95 SF2 44100-source path is evidenced by original
    // driver data. Other sample rates and all SF1/SBK/DOS semantics
    // deliberately retain their previously verified conversion until
    // source-specific rate conversion evidence is available.
    if (!sf1 && drv==Awe32::Driver::Win95 && s.sampleRate==44100u)
    {
        const int64_t fixed =
            static_cast<int64_t>(std::lround(centsTotal))*874LL;
        const int64_t sar8 = fixed>=0 ? fixed/256LL
                            : -((-fixed+255LL)/256LL);
        vp.ip=static_cast<uint16_t>(
            std::clamp<int64_t>(0xE000LL+sar8,0LL,65535LL));
    }
    else
        vp.ip = static_cast<uint16_t>(
            std::clamp(PitchFromCents(centsTotal), 0, 65535));"""
if data.count(anchor)!=1:
    raise SystemExit("P217 original SF2 VXD IP arithmetic source drift")
source.write_text(data.replace(anchor,new),encoding="utf-8")
print("P217_ORIGINAL_CREATIVE_VXD_IP_IMUL_874_SAR8_PATCH_APPLIED")
