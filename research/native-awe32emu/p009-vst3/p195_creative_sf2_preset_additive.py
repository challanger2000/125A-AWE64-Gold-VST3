#!/usr/bin/env python3
"""P195 restricted Creative SF2 generator merge: proven linear operations ONLY.

Original SBAWE.VXD v4.38.0.2 LCOD+0x49AA saturating ADDs *converted*
instrument and preset values. For linear SF2 sources coarseTune (51),
fineTune (52), and initialAttenuation (48), addition BEFORE conversion is
equivalent to separately converting then summing. Do not apply the same
raw-add shortcut to nonlinear filter resonance/cutoff/envelope parameters:
independent original-code analysis exposes rounding counterexamples.

Creative SF1/SBK FillFrom unchanged; nonlinear SF2 overlaps remain explicitly
UNVERIFIED and are NOT silently claimed correct by P195.
"""
from pathlib import Path
import sys

def main():
    if len(sys.argv)!=2:
        raise SystemExit("usage: p195_creative_sf2_preset_additive.py <pinned-AWE32Emu-checkout>")
    p=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"/"SoundFont.cpp"
    s=p.read_text(encoding="utf-8")
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
    newselect="""            // P195: original Creative SF2 VXD adds *converted* instrument
            // and preset generator words with signed-16 saturation.
            // Raw SF2 addition reproduces this only for proven LINEAR
            // generators. Scope deliberately limited to (48,51,52);
            // nonlinear SF2 filter/FX/envelope overlap is NOT VERIFIED.
            // Do not modify Creative SF1/SBK measured FillFrom at all.
            if (version != Version::Sf1)
            {
                for (const int op : {Gen::InitialAttenuation,
                                     Gen::CoarseTune, Gen::FineTune})
                {
                    if (r.gen.Has(op) && presetGen.Has(op))
                    {
                        r.gen.value[op] = static_cast<int16_t>(std::clamp(
                            static_cast<int>(r.gen.value[op])
                              + static_cast<int>(presetGen.value[op]),
                            -32768, 32767));
                    }
                }
            }
            r.gen.FillFrom(presetGen);"""
    if s.count(oldselect)!=1 or "P195: original Creative SF2" in s:
        raise RuntimeError("P195 pinned original source changed; refuse patch")
    p.write_text(s.replace(oldselect,newselect),encoding="utf-8")
    print("P195_SF2_LINEAR_TUNE_ATTEN_ONLY_SF1_AND_NONLINEAR_UNCHANGED_PATCHED")
if __name__=="__main__":
    main()
