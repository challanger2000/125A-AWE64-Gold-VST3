#!/usr/bin/env python3
"""P215 original 1996 Creative PCOD op27/35 and scoped SF2 vol-sustain.

Derived from independent privately-owned x86 PCOD clean-room numeric
contract: complete signed-16-bit input sweep, FNV64 8de7932315cfb4ea.
This patches only GENERIC converter arithmetic; no ROM/SF2/VXD assets,
no preset numbers or owner register measurements are embedded here.
"""
from pathlib import Path
import sys
if len(sys.argv)!=2: raise SystemExit("usage: P215 patch PINNED-UPSTREAM")
root=Path(sys.argv[1])/"AWE32Emu/src"
h=root/"SoundFont.h"
cpp=root/"SoundFont.cpp"
hs=h.read_text(encoding="utf-8-sig")
cs=cpp.read_text(encoding="utf-8-sig")
changes=[
("header namespace", "h", """namespace SoundFont
{
    enum class Version { Sf1, Sf2 };""",
"""namespace SoundFont
{
    // Exact numeric x86 PCOD HoldModEnv conversion (all signed16 inputs).
    // Public arithmetic only. No Creative binary/table shipped.
    int OriginalCreativePcodHoldRate(int timecents);
    enum class Version { Sf1, Sf2 };"""),
("header region", "h", """        int sf2PresetVolDecayRate = -1;""",
"""        int sf2PresetVolDecayRate = -1;
        // P215 original VXD descriptor-converted Win95 SF2 scopes.
        int sf2OriginalModHoldRate = -1;
        int sf2OriginalVolSustain = -1;"""),
("source function", "c", """namespace SoundFont
{
namespace
{""",
"""namespace SoundFont
{
// PCOD+0x3BD4 exact x86 signed-32 multiplication wrap, truncation and
// logical shift for hold generators 27/35. Independently cross-checked
// against an exhaustive 65,536-case original-driver numeric contract.
int OriginalCreativePcodHoldRate(int tc)
{
    if (tc < -5368) return 127;
    const auto word=static_cast<std::uint32_t>(
        (static_cast<std::int64_t>(tc)+4130LL)*65536LL);
    const auto signedWord=(word&0x80000000u)
      ? static_cast<std::int64_t>(word)-4294967296LL
      : static_cast<std::int64_t>(word);
    const auto q=static_cast<std::int32_t>(signedWord/1200LL);
    const unsigned shift=(16-q/65536)&31;
    const auto expanded=(65536u+(static_cast<std::uint32_t>(q)&65535u))>>shift;
    return std::max(0,127-static_cast<int>(expanded));
}
namespace
{"""),
("scope", "c", """            // Keep SF1 observed FillFrom and the P195 restricted linear
            // SF2 merging unchanged. Only filter register output uses""",
"""            // P215 original Creative PCOD scope conversion. A missing
            // SF2 instrument HoldModEnv has the -12000 timecents template,
            // not the SF1 value. Keynum-scaled paths stay on their
            // independently-tested implementation until separately audited.
            if(version == Version::Sf2) {
                if(!r.gen.Has(Gen::KeynumToModEnvHold)
                   && !presetGen.Has(Gen::KeynumToModEnvHold)) {
                    const int modHold=std::clamp(
                       r.gen.Get(Gen::HoldModEnv,-12000)
                       + presetGen.Get(Gen::HoldModEnv,0),-32768,32767);
                    r.sf2OriginalModHoldRate=OriginalCreativePcodHoldRate(modHold);
                }
                // Creative instrument descriptor template sustain=127.
                // Both instrument and preset scope are separately
                // converted into a -127*x/1000 delta, then summed.
                // Integer divisions MUST truncate toward zero, as in x86.
                const int ins=r.gen.Get(Gen::SustainVolEnv,0);
                const int pre=presetGen.Get(Gen::SustainVolEnv,0);
                r.sf2OriginalVolSustain=std::clamp(
                     127 + (-127*ins)/1000 + (-127*pre)/1000,0,127);
            }
            // Keep SF1 observed FillFrom and the P195 restricted linear
            // SF2 merging unchanged. Only filter register output uses"""),
("sustain", "c", """    vp.dcysusv = static_cast<uint16_t>(
        (sustainReg(Gen::SustainVolEnv) << 8)
        | (region.sf2PresetVolDecayRate>=0 ? region.sf2PresetVolDecayRate""",
"""    vp.dcysusv = static_cast<uint16_t>(
        ((drv==Awe32::Driver::Win95 && region.sf2OriginalVolSustain>=0
            ? region.sf2OriginalVolSustain
            : sustainReg(Gen::SustainVolEnv)) << 8)
        | (region.sf2PresetVolDecayRate>=0 ? region.sf2PresetVolDecayRate"""),
("modhold", "c", """    vp.atkhld = static_cast<uint16_t>(
        (HoldFromMs(keyScaled(Gen::HoldModEnv, Gen::KeynumToModEnvHold, false)) << 8)
        | attackRate(Gen::AttackModEnv));""",
"""    vp.atkhld = static_cast<uint16_t>(
        ((drv==Awe32::Driver::Win95 && region.sf2OriginalModHoldRate>=0
           ? region.sf2OriginalModHoldRate
           : HoldFromMs(keyScaled(Gen::HoldModEnv, Gen::KeynumToModEnvHold, false))) << 8)
        | attackRate(Gen::AttackModEnv));""")
]
for label,which,old,new in changes:
    s=hs if which=="h" else cs
    if s.count(old)!=1: raise SystemExit("P215 FAIL-CLOSED original source drift: "+label)
    if which=="h": hs=hs.replace(old,new)
    else: cs=cs.replace(old,new)
h.write_text(hs,encoding="utf-8")
cpp.write_text(cs,encoding="utf-8")
print("P215_ORIGINAL_PC0D_65536_HOLD_AND_SF2_SUSTAIN_SCOPES_APPLIED")
