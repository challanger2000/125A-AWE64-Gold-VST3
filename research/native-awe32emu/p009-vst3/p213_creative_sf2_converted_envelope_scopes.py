#!/usr/bin/env python3
"""P213: original Creative VXD SF2 converted-scope volume envelope merge.

Data-backed with owner-private actual Creative-Win95 original EMU8000
register capture: bank1 program7 key60 velocity110. The same compiled
86Box C chip and owner ROM/SBK/SF2 media yielded 61 mismatched port
words before P213; only the independently converted instrument/preset
vol-envelope scopes are changed. NO owner media or captured words are
committed, and the genuine DSP is not modified.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: p213_creative_sf2_converted_envelope_scopes.py UPSTREAM")
root=Path(sys.argv[1])/"AWE32Emu/src"
h=root/"SoundFont.h"
cpp=root/"SoundFont.cpp"
hs=h.read_text(encoding="utf-8-sig")
cs=cpp.read_text(encoding="utf-8-sig")
old_header="""        int sf2FilterFcReg = -1;
        int sf2FilterQReg = -1;"""
new_header="""        int sf2FilterFcReg = -1;
        int sf2FilterQReg = -1;
        // P213: only scoped original Creative SF2 hardware-unit arithmetic.
        // Negative = no independent original instrument/preset sum.
        int sf2CombinedVolAttackRate = -1;
        int sf2CombinedVolHoldRate = -1;
        int sf2CombinedVolDelay = -1;"""
old_scope="""            // Keep SF1 observed FillFrom and the P195 restricted linear
            // SF2 merging unchanged. Only filter register output uses"""
new_scope="""            // P213: original Win95 VXD converts SF2 instrument and preset
            // timecents separately into chip register units, then adds
            // each scope before saturating. Discarding the preset volume
            // attack/hold/delay caused wrong initial envelope/attack and
            // darkened velocity-sensitive filters. This is a GENERIC
            // scope conversion; it does not encode owner sample data,
            // a preset identifier, a sound hash, or observed register words.
            // Key-scaling remains on the old path until independently
            // verified for both scopes, so no guessed key scaling is used.
            if (version == Version::Sf2)
            {
                if (r.gen.Has(Gen::AttackVolEnv)
                    && presetGen.Has(Gen::AttackVolEnv))
                    r.sf2CombinedVolAttackRate = std::min(127,
                        AttackRateFromMs(TimecentsToMs(
                            r.gen.Get(Gen::AttackVolEnv,0))) +
                        AttackRateFromMs(TimecentsToMs(
                            presetGen.Get(Gen::AttackVolEnv,0))));

                if (r.gen.Has(Gen::HoldVolEnv)
                    && presetGen.Has(Gen::HoldVolEnv)
                    && !r.gen.Has(Gen::KeynumToVolEnvHold)
                    && !presetGen.Has(Gen::KeynumToVolEnvHold))
                    r.sf2CombinedVolHoldRate = std::min(127,
                        HoldFromMs(TimecentsToMs(
                            r.gen.Get(Gen::HoldVolEnv,0))) +
                        HoldFromMs(TimecentsToMs(
                            presetGen.Get(Gen::HoldVolEnv,0))));

                // A missing instrument generator carries the original
                // Creative SF2 delay template (0x8000 = no delay), NOT
                // a replacement zero. The later preset register offset
                // is added, capped at the 0x8000 delay-none endpoint.
                if (presetGen.Has(Gen::DelayVolEnv))
                    r.sf2CombinedVolDelay = std::min(0x8000,
                        (r.gen.Has(Gen::DelayVolEnv)
                             ? DelayFromTimecents(
                                 r.gen.Get(Gen::DelayVolEnv,0))
                             : 0x8000) +
                        DelayFromTimecents(
                            presetGen.Get(Gen::DelayVolEnv,0)));
            }
            // Keep SF1 observed FillFrom and the P195 restricted linear
            // SF2 merging unchanged. Only filter register output uses"""
old_delay="    vp.envvol  = delayReg(Gen::DelayVolEnv);"
new_delay="""    vp.envvol  = static_cast<uint16_t>(region.sf2CombinedVolDelay>=0
        ? region.sf2CombinedVolDelay : delayReg(Gen::DelayVolEnv));"""
old_attack="""    vp.atkhldv = static_cast<uint16_t>(
        (HoldFromMs(keyScaled(Gen::HoldVolEnv, Gen::KeynumToVolEnvHold, false)) << 8)
        | attackRate(Gen::AttackVolEnv));"""
new_attack="""    vp.atkhldv = static_cast<uint16_t>(
        ((region.sf2CombinedVolHoldRate>=0 ? region.sf2CombinedVolHoldRate
          : HoldFromMs(keyScaled(Gen::HoldVolEnv, Gen::KeynumToVolEnvHold,
                                 false))) << 8)
        | (region.sf2CombinedVolAttackRate>=0
             ? region.sf2CombinedVolAttackRate
             : attackRate(Gen::AttackVolEnv)));"""
for label,old,neu in [
    ("header",old_header,new_header),("converted scope",old_scope,new_scope),
    ("delay",old_delay,new_delay),("attack and hold",old_attack,new_attack)]:
    source=hs if label=="header" else cs
    if source.count(old)!=1:
        raise SystemExit("P213 pin drift / duplicate patch anchor: "+label)
    if label=="header":
        hs=hs.replace(old,neu)
    else:
        cs=cs.replace(old,neu)
h.write_text(hs,encoding="utf-8")
cpp.write_text(cs,encoding="utf-8")
print("P213_ORIGINAL_CREATIVE_SF2_CONVERTED_VOLUME_ENVELOPE_SCOPES_PATCH_APPLIED")
