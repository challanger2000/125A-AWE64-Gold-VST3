#!/usr/bin/env python3
"""P196: apply genuine Creative SF2 generators 1/12 to hardware sample end.

Evidence: owner-original SBAWE.VXD 4.38.0.2, LCOD object:
  +0x49EE  reads [edx+0x02]  (EndAddrsOffset, generator 1),
  +0x49F2  adds sample.end from parsed sample description,
  +0x49F8  reads [edx+0x18]  (EndAddrsCoarseOffset, generator 12),
  +0x49FC  shifts coarse offset 15 bits, +0x49FF adds it,
  +0x4A01  stores the genuine effective sample end.

The pinned AWE32Emu SoundFont::MakeVoiceParams() accidentally ignores
both generators when constructing sampleEndAddr and the chip's one-shot
loop after sample end. SF1 ROM/SBK behavior is deliberately unchanged.
No original ROM, driver binary or private sample data is distributed.
"""
from pathlib import Path
import sys

def main():
    if len(sys.argv)!=2:
        raise SystemExit("usage: p196_creative_sf2_sample_end_offsets.py <pinned-AWE32Emu-checkout>")
    path=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"/"SoundFont.cpp"
    s=path.read_text(encoding="utf-8")
    old1="""    vp.sampleEndAddr = (sf1 && inRom) ? s.end : (base + s.end);
    vp.loopEndAddr   = loopEnd;"""
    new1="""    // P196: genuine original Creative SF2 driver (SBAWE.VXD LCOD
    // +0x49EE..0x4A02) adds gen1 and (gen12 << 15) to sample END.
    // The previous path applied sample START and LOOP offsets, but
    // accidentally omitted END offsets. SF1 paths remain byte-identical.
    // Hardware addresses use 32-bit modular arithmetic like the VXD.
    const uint32_t sampleEndAddr = (sf1 && inRom) ? s.end :
        (base + s.end + (sf1 ? 0u :
            static_cast<uint32_t>(g.Get(Gen::EndAddrsOffset, 0)) +
            static_cast<uint32_t>(32768ll *
                g.Get(Gen::EndAddrsCoarseOffset, 0))));
    vp.sampleEndAddr = sampleEndAddr;
    vp.loopEndAddr   = loopEnd;"""
    old2="        const uint32_t end = (sf1 && inRom) ? s.end : (base + s.end);"
    new2="""        // P196: the original driver moves the one-shot silence loop to
        // the effective sample end, not the unadjusted shdr endpoint.
        const uint32_t end = sampleEndAddr;"""
    for needle in (old1,old2):
        if s.count(needle)!=1:
            raise RuntimeError("Pinned P196 upstream SampleEnd anchor changed: "+needle[:60])
    if "P196: genuine original Creative SF2" in s:
        raise RuntimeError("P196 already applied")
    # The original LCOD+0x49AA saturating sum also applies when gen1/gen12
    # are present at BOTH instrument and preset scope. The earlier P195
    # restricted linear merge covers only tune and attenuation. Extend it
    # only for these two independently evidenced address-offset generators.
    old3="""for (const int op : {Gen::InitialAttenuation,
                                     Gen::CoarseTune, Gen::FineTune})"""
    new3="""for (const int op : {Gen::InitialAttenuation,
                                     Gen::CoarseTune, Gen::FineTune,
                                     Gen::EndAddrsOffset,
                                     Gen::EndAddrsCoarseOffset})"""
    if s.count(old3)!=1:
        raise RuntimeError("P196 expected P195 linear-merge anchor missing")
    path.write_text(s.replace(old1,new1).replace(old2,new2)
                     .replace(old3,new3),encoding="utf-8")
    print("P196_ORIGINAL_CREATIVE_SF2_END_OFFSET_1_12_ONESHOT_AND_NOTEEND_SF1_PRESERVED_PATCHED")
if __name__=="__main__":
    main()
