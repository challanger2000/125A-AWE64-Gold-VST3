#!/usr/bin/env python3
"""P197: correct original Creative Win95 SF2 scaleTuning note-tracking.

Owner-original SBAWE.VXD 4.38.0.2, LCOD code extracted privately:
  LCOD+0x5709: read generator 56 (scaleTuning);
  LCOD+0x570F: multiply root key by scale;
  LCOD+0x5718: subtract it from pitch cents;
  LCOD+0x5729: multiply played key by scale;
  LCOD+0x5730: convert difference to native EMU8000 pitch increments.
Thus original Creative SF2 note pitch contribution is:
    (playedKey - rootKey) * scaleTuning_in_cents_per_key.
The old AWE32Emu code used 100 cents/key for ALL non-1 values and
applied an SF1-only scaleTuning==1 special case to SF2 as well.

Original SF1/SBK semantics are not altered; these have independent
driver trace evidence for the case scaleTuning==1 => half entire pitch.
No original ROM, driver binary, or private SoundFonts are published.
"""
from pathlib import Path
import sys

def main():
    if len(sys.argv)!=2:
        raise SystemExit("usage: p197_creative_sf2_scale_tuning.py <pinned checkout>")
    path=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"/"SoundFont.cpp"
    code=path.read_text(encoding="utf-8")
    old="""    // \`scaleTuning\` is **not in percent** as in SF2. The driver only tests
    // for equality with one and then **halves** the whole result -
    // transcribed from \`SBAWE.VXD\` (object at 0xC0FF7BE0),
    // C0FFAF54..C0FFAF87:
    //
    //     ecx = keynum - rootKey + coarseTune
    //     ecx = (ecx + 60) * 100 - samplePitch + fineTune
    //     cmp word [esi+0x70], 1        ; scaleTuning
    //     jne next
    //         eax = ecx; cdq; sub eax,edx; sar eax,1   ; divide by 2 towards zero
    //
    // The field names are from \`SFTYPE.H\` of the AWE32 SDK (0x6E
    // samplePitch, 0x70 scaleTuning, 0x74 rootKey).
    //
    // Measured: preset 122 SeaShore in SYNTHGM.SBK has \`scaleTuning 1\`, and
    // those were the last four mismatching notes of RELAX. For note 69 with
    // samplePitch 8781 it gives (69-60+60)*100 - 8781 = -1881, half is
    // -940 - exactly what the driver wrote.
    double pitchCents = (key - rootKey) * 100.0 + cents;
    if (g.Get(Gen::ScaleTuning, 100) == 1)
        pitchCents = static_cast<int>(pitchCents) / 2;"""
    new="""    // P197: Creative Win95 has DIFFERENT SF1 and SF2 pitch paths.
    // SF1/SBK preserves the measured special flag scaleTuning==1,
    // which halves the entire patch pitch (LCOD +0x4E17..0x4E28).
    // SF2 instead multiplies (note - rootKey) by generator 56 directly,
    // in cents per MIDI key: original LCOD +0x5709..0x5730.
    // The previous source applied the SF1-only flag to all SF2 banks:
    // e.g. SF2 scaleTuning 50 played as 100 (wrong by six semitones
    // over a 12-key interval), and 0 failed to disable key tracking.
    double pitchCents;
    if (sf1)
    {
        pitchCents = (key - rootKey) * 100.0 + cents;
        if (g.Get(Gen::ScaleTuning, 100) == 1)
            pitchCents = static_cast<int>(pitchCents) / 2;
    }
    else
    {
        pitchCents = (key - rootKey) *
                     static_cast<double>(g.Get(Gen::ScaleTuning, 100)) + cents;
    }"""
    if code.count(old)!=1 or "P197: Creative Win95" in code:
        raise RuntimeError("Pinned P197 upstream SF1/SF2 pitch anchor changed")
    path.write_text(code.replace(old,new),encoding="utf-8")
    print("P197_WIN95_CREATIVE_SF2_SCALE_TUNING_CENTS_PER_KEY_SF1_UNCHANGED_PATCHED")
if __name__=="__main__":
    main()
