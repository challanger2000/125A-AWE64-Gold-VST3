#!/usr/bin/env python3
"""P214: three independently observed Win95 Creative SF2 conversion gaps.

Only a genuine original-guest EMU8000 I/O comparison on private
owner-selected bank1/preset7/note60 exposed all three. This patch
implements GENERIC SF2 calculations, NOT one-preset value injection.
No private assets or original driver code go into the public repo.
"""
from pathlib import Path
import sys
if len(sys.argv)!=2:raise SystemExit("usage: p214_original_creative_sf2_decay_lfo_addresses.py UPSTREAM")
src=Path(sys.argv[1])/"AWE32Emu/src"
header=src/"SoundFont.h"
cpp=src/"SoundFont.cpp"
h=header.read_text(encoding="utf-8-sig")
c=cpp.read_text(encoding="utf-8-sig")
changes_h=[
("""        int sf2CombinedVolDelay = -1;""",
 """        int sf2CombinedVolDelay = -1;
        // Original Creative preset-level SF2 decay is applied after its
        // own timecents conversion; do not silently discard this scope.
        int sf2PresetVolDecayRate = -1;""")]
changes_cpp=[
("""            // Keep SF1 observed FillFrom and the P195 restricted linear
            // SF2 merging unchanged. Only filter register output uses""",
 """            // P214: the original guest changes DCYSUSV decay for each
            // SF2 preset-zone timecents value even when an instrument
            // global decay is present. The pre-P214 FillFrom silently
            // threw away that preset generator; select its converted
            // hardware rate. Do not guess keynum-dependent behavior.
            if(version==Version::Sf2
                && presetGen.Has(Gen::DecayVolEnv)
                && !r.gen.Has(Gen::KeynumToVolEnvDecay)
                && !presetGen.Has(Gen::KeynumToVolEnvDecay))
                r.sf2PresetVolDecayRate = DecayRateFromMs(
                    TimecentsToMs(presetGen.Get(Gen::DecayVolEnv,0)));

            // Keep SF1 observed FillFrom and the P195 restricted linear
            // SF2 merging unchanged. Only filter register output uses"""),
("""        start     = base + s.start + g.Get(Gen::StartAddrsOffset, 0)
                    + 32768u * g.Get(Gen::StartAddrsCoarseOffset, 0) - 1;
        loopStart = base + s.loopStart + g.Get(Gen::StartloopAddrsOffset, 0)
                    + 32768u * g.Get(Gen::StartloopAddrsCoarseOffset, 0) - 2;
        loopEnd   = base + s.loopEnd + g.Get(Gen::EndloopAddrsOffset, 0)
                    + 32768u * g.Get(Gen::EndloopAddrsCoarseOffset, 0) - 3;""",
 """        // Private genuine Win95 Creative CCCA/PSST/CSL tracing found
        // ALL three SF2 addresses one word ahead of the old SF1-derived
        // approximation; apply this at the SF2 address-conversion layer,
        // only for the measured Win95 driver. DOS and SF1 unchanged.
        const uint32_t creativeSf2AddressShift =
            drv==Awe32::Driver::Win95 ? 1u : 0u;
        start     = base + s.start + g.Get(Gen::StartAddrsOffset, 0)
                    + 32768u * g.Get(Gen::StartAddrsCoarseOffset, 0) - 1
                    + creativeSf2AddressShift;
        loopStart = base + s.loopStart + g.Get(Gen::StartloopAddrsOffset, 0)
                    + 32768u * g.Get(Gen::StartloopAddrsCoarseOffset, 0) - 2
                    + creativeSf2AddressShift;
        loopEnd   = base + s.loopEnd + g.Get(Gen::EndloopAddrsOffset, 0)
                    + 32768u * g.Get(Gen::EndloopAddrsCoarseOffset, 0) - 3
                    + creativeSf2AddressShift;"""),
("""    vp.dcysusv = static_cast<uint16_t>(
        (sustainReg(Gen::SustainVolEnv) << 8)
        | DecayRateFromMs(keyScaled(Gen::DecayVolEnv, Gen::KeynumToVolEnvDecay, true)));""",
 """    vp.dcysusv = static_cast<uint16_t>(
        (sustainReg(Gen::SustainVolEnv) << 8)
        | (region.sf2PresetVolDecayRate>=0 ? region.sf2PresetVolDecayRate
           : DecayRateFromMs(keyScaled(
               Gen::DecayVolEnv,Gen::KeynumToVolEnvDecay,true))));"""),
("""    const int lfo1Freq = sf1 ? (g.Has(Gen::FreqModLFO)""",
 """    // Original Creative Win95 SF2 template uses 0xCD for an omitted
    // frequency generator, not the generic Hz/step estimate 0xC3.
    // An explicit SF2 frequency remains on the previous conversion path.
    const int nativeDefaultLfo = std::clamp<int>(
        static_cast<int>(std::lround(8.176 / kLfoHzPerStep)),0,255);
    const int win95Sf2DefaultLfo =
        drv==Awe32::Driver::Win95 ? 0xCD : nativeDefaultLfo;
    const int lfo1Freq = sf1 ? (g.Has(Gen::FreqModLFO)"""),
("""                             : std::clamp<int>(static_cast<int>(std::lround(
                                   8.176 * std::pow(2.0, g.Get(Gen::FreqModLFO, 0) / 1200.0)
                                   / kLfoHzPerStep)), 0, 255);""",
 """                             : (g.Has(Gen::FreqModLFO)
                                  ? std::clamp<int>(static_cast<int>(std::lround(
                                   8.176 * std::pow(2.0, g.Get(Gen::FreqModLFO, 0) / 1200.0)
                                   / kLfoHzPerStep)), 0, 255)
                                  : win95Sf2DefaultLfo);"""),
("""                             : std::clamp<int>(static_cast<int>(std::lround(
                                   8.176 * std::pow(2.0, g.Get(Gen::FreqVibLFO, 0) / 1200.0)
                                   / kLfoHzPerStep)), 0, 255);""",
 """                             : (g.Has(Gen::FreqVibLFO)
                                  ? std::clamp<int>(static_cast<int>(std::lround(
                                   8.176 * std::pow(2.0, g.Get(Gen::FreqVibLFO, 0) / 1200.0)
                                   / kLfoHzPerStep)), 0, 255)
                                  : win95Sf2DefaultLfo);"""),
]
for label,old,new in [("h:"+str(i),o,n) for i,(o,n) in enumerate(changes_h)]+[
    ("c:"+str(i),o,n) for i,(o,n) in enumerate(changes_cpp)]:
    s=h if label[0]=='h' else c
    if s.count(old)!=1:raise SystemExit("P214 source drift / duplicate anchor "+label)
    if label[0]=='h':h=h.replace(old,new)
    else:c=c.replace(old,new)
header.write_text(h,encoding="utf-8")
cpp.write_text(c,encoding="utf-8")
print("P214_WIN95_CREATIVE_SF2_PRESET_DECAY_DEFAULT_LFO_AND_START_ADDRESSES_APPLIED")
