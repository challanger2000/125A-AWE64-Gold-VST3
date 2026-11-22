#!/usr/bin/env python3
"""P194: preserve Creative SF1 while applying original SF2 attack defaults.

Private provenance (NOT committed): owner's SBAWE.VXD v4.38.0.2 SHA-256
d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a.
LE LCOD+0x48EE relocates to the original table at LCOD+0x2D88; the
hardware-driver code copies 37 dwords. Slots 26/34 both contain -12000
timecents, while slots 29/37 contain internal sustain default 127.
No private source data, ROM, or trace is needed in the public build.

Test: --synthetic native Windows host compares PCM of an omitted SF2
generator 34 to the same generator explicitly set to -12000.
"""
from pathlib import Path
import sys

def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: p194_creative_sf2_attack_default.py <pinned-AWE32Emu-checkout>")
    path=Path(sys.argv[1]).resolve()/"AWE32Emu"/"src"/"SoundFont.cpp"
    s=path.read_text(encoding="utf-8")
    anchor="    vp.envvol  = delayReg(Gen::DelayVolEnv);"
    helper="""    // P194: the original Creative SF2 generator default template uses
    // -12000 timecents for both volume and modulation attacks (gen 34/26).
    // Its provenance is the owner's SHA-verified SBAWE.VXD v4.38.0.2,
    // LCOD +0x2D88, copied into voice descriptors at LCOD +0x48ED.
    // In the native emulation the missing-generator path formerly used
    // the *SF1/SBK* default register 0x7D for SF2 too. For valid SF2,
    // omitted -12000 must follow the same conversion as explicit -12000.
    // Preserve original measured SF1 default exactly.
    auto attackRate = [&](int op) -> int
    {
        if (!g.Has(op))
            return sf1 ? kAttackDefaultRate :
                AttackRateFromMs(TimecentsToMs(-12000));
        return AttackRateFromMs(timeMs(op, 0.0));
    };

"""
    if s.count(anchor)!=1 or "P194: the original Creative SF2" in s:
        raise RuntimeError("P194 original source anchor drift or patch already applied")
    s=s.replace(anchor,helper+anchor)
    oldvol="""        | (g.Has(Gen::AttackVolEnv)
               ? AttackRateFromMs(timeMs(Gen::AttackVolEnv, 0.0))
               : kAttackDefaultRate));"""
    newvol="""        | attackRate(Gen::AttackVolEnv));"""
    oldmod="""        | (g.Has(Gen::AttackModEnv)
               ? AttackRateFromMs(timeMs(Gen::AttackModEnv, 0.0))
               : kAttackDefaultRate));"""
    newmod="""        | attackRate(Gen::AttackModEnv));"""
    if s.count(oldvol)!=1 or s.count(oldmod)!=1:
        raise RuntimeError("P194 expected exactly one SF1/SF2 attack conversion call per envelope")
    s=s.replace(oldvol,newvol).replace(oldmod,newmod)
    path.write_text(s,encoding="utf-8")
    print("P194_CREATIVE_SF2_ATTACK_DEFAULT_MINUS12000_ROM_SF1_UNCHANGED_PATCHED")
if __name__=="__main__":
    main()
