#!/usr/bin/env python3
"""P195: restore real Creative SF2 preset/instrument addition.

Owner's SHA-verified original Win95 SBAWE.VXD v4.38.0.2 has its SF2
descriptor merge at LCOD+0x499B..0x49D8: it ADDS instrument and preset
generator words with signed-16 saturation (ADD ECX,EDI at +0x49AA;
bounds at +0x49AC..+0x49C8). Its template is copied at +0x48ED.
That behavior must not be extrapolated from Creative SF1/SBK tests, where
FillFrom was confirmed in completely different material.

The original binary and private traces are NEVER part of this script.
Only the exact pinned SoundFont.cpp is patched. Fail closed on drift.
"""
from pathlib import Path
import sys

def main():
    if len(sys.argv)!=2:
        raise SystemExit("usage: p195_creative_sf2_preset_additive.py <pinned-AWE32Emu-checkout>")
    p=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"/"SoundFont.cpp"
    s=p.read_text(encoding="utf-8")
    oldadd="""            if (present[i]) value[i] = static_cast<int16_t>(value[i] + other.value[i]);
            else            { value[i] = other.value[i]; present[i] = true; }"""
    newadd="""            // P195: original SF2 driver saturates the signed 16-bit sum
            // (LCOD+0x49AA..0x49CB); do not overflow/wrap at extremes.
            if (present[i]) value[i] = static_cast<int16_t>(std::clamp(
                static_cast<int>(value[i]) + static_cast<int>(other.value[i]),
                -32768, 32767));
            else            { value[i] = other.value[i]; present[i] = true; }"""
    oldselect="""            // The preset zone only **fills in** what the instrument zone
            // lacks - it is not added. Formerly we added per the SF2
            // specification, but the driver does not do that.
            //
            // Measured on CRAZY: channel 4 plays program 97 (SeaShore ->
            // Soundtrack), which has `coarseTune` on both levels - 1 on the
            // preset and 3 on the instrument. Adding gave us 4, the driver
            // uses 3 and plays a semitone lower; all 58 notes of that
            // channel were off. It showed because two layers of the same
            // tone had offsets of -341 and -342, i.e. unequal - the
            // signature of an offset in **cents before the conversion**,
            // not of a constant in IP units.
            //
            // After the change CRAZY matches 32/32 and nothing else got
            // worse: Georgia, JUMP, RELAX and MINUET stay at 32/32, Magic
            // Carpet 2 at 24/24 - over 22 000 notes in total.
            //
            // The attenuation is an exception, handled separately above,
            // because the driver adds it only in register units.
            r.gen.FillFrom(presetGen);"""
    newselect="""            // P195: previous CRAZY/Georgia note comparisons demonstrated
            // FillFrom for Creative SF1/SBK only. The ORIGINAL owner's
            // Win95 Creative SBAWE.VXD SF2 descriptor merge directly adds
            // preset and instrument generator words (LCOD+0x49AA), and
            // saturates to signed-16 (LCOD+0x49AC..0x49CB). Distinguish the
            // two versions; never change the verified SF1/ROM behavior.
            if (version == Version::Sf1)
                r.gen.FillFrom(presetGen);
            else
                r.gen.AddFrom(presetGen);"""
    if s.count(oldadd)!=1 or s.count(oldselect)!=1 or "P195: previous CRAZY" in s:
        raise RuntimeError("P195 upstream source mismatch or patch already applied")
    s=s.replace(oldadd,newadd).replace(oldselect,newselect)
    p.write_text(s,encoding="utf-8")
    print("P195_ORIGINAL_CREATIVE_SF2_PRESET_ADDITIVE_SATURATING_SF1_UNCHANGED_PATCHED")
if __name__=="__main__":
    main()
