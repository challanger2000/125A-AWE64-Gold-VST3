#include "CreativeVxdVoiceEnvelopePrelude.h"
#include <cstdint>
#include <array>
#include <algorithm>
namespace awe64::creative {
namespace {
// Original installed owner's private Creative SBAWE.VXD LCOD+0x31A8.
// Functional 128-word threshold constants; no original binary is included.
constexpr std::array<std::uint16_t,128> kOriginalPcodRateThresholds{{
    47513, 23756, 15838, 11878, 9503, 7919, 6788, 5939, 5279, 4751, 4319, 3959, 3655, 3394, 3168, 2970,
    2795, 2640, 2501, 2376, 2263, 2160, 2066, 1980, 1901, 1827, 1760, 1697, 1638, 1584, 1533, 1485,
    1397, 1320, 1250, 1188, 1131, 1080, 1033, 990, 950, 914, 880, 848, 819, 792, 766, 742,
    699, 660, 625, 594, 566, 540, 516, 495, 475, 457, 440, 424, 410, 396, 383, 371,
    349, 330, 313, 297, 283, 270, 258, 247, 238, 228, 220, 212, 205, 198, 192, 186,
    175, 165, 156, 148, 141, 135, 129, 124, 119, 114, 110, 106, 102, 99, 96, 93,
    87, 82, 78, 74, 71, 67, 65, 62, 59, 57, 55, 53, 51, 49, 48, 46,
    44, 41, 39, 37, 35, 34, 32, 31, 30, 29, 27, 27, 26, 25, 24, 23
}};
static_assert(kOriginalPcodRateThresholds.size()==128);
static_assert(kOriginalPcodRateThresholds[0]==47513 &&
              kOriginalPcodRateThresholds[39]==990 &&
              kOriginalPcodRateThresholds[127]==23);
constexpr bool sorted() noexcept {
    for(std::size_t i=1;i<kOriginalPcodRateThresholds.size();++i)
        if(kOriginalPcodRateThresholds[i]>kOriginalPcodRateThresholds[i-1])
            return false;
    return true;
}
static_assert(sorted());
// LCOD +0x74F2 reads index 1..126 of this 127-WORD table at
// LCOD+0x30AA (the 0th word is intentionally skipped by original VXD).
constexpr std::array<std::uint16_t,127> kOriginalPcodAttackThresholds{{
    11878, 5939, 3959, 2970, 2376, 1980, 1697, 1485, 1320, 1188, 1080, 990, 914, 848, 792, 742,
    699, 660, 625, 594, 566, 540, 516, 495, 475, 457, 440, 424, 410, 396, 383, 371,
    349, 330, 313, 297, 283, 270, 258, 247, 238, 228, 220, 212, 205, 198, 192, 186,
    175, 165, 156, 148, 141, 135, 129, 124, 119, 114, 110, 106, 102, 99, 96, 93,
    87, 82, 78, 74, 71, 67, 65, 62, 59, 57, 55, 53, 51, 49, 48, 46,
    44, 41, 39, 37, 35, 34, 32, 31, 30, 29, 27, 27, 26, 25, 24, 23,
    22, 21, 20, 19, 18, 17, 16, 15, 15, 14, 14, 13, 13, 12, 12, 12,
    11, 10, 10, 9, 9, 8, 8, 8, 7, 7, 7, 7, 6, 6, 6
}};
static_assert(kOriginalPcodAttackThresholds.size()==127);
static_assert(kOriginalPcodAttackThresholds[0]==11878 &&
              kOriginalPcodAttackThresholds[1]==5939 &&
              kOriginalPcodAttackThresholds[126]==6);
constexpr bool attackSorted() noexcept {
    for(std::size_t i=1;i<kOriginalPcodAttackThresholds.size();++i)
        if(kOriginalPcodAttackThresholds[i]>kOriginalPcodAttackThresholds[i-1])
            return false;
    return true;
}
static_assert(attackSorted());
std::uint16_t combine(const std::int16_t word,const std::uint8_t low) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(word)<<8)|low);
}
std::uint8_t specialAttack(const std::uint16_t word) noexcept {
    // Actual VXD +0x5C04/+0x5C46: if raw exactly 0x0080,
    // substitute 0x7F, otherwise use its low byte.
    return static_cast<std::uint8_t>(word==0x0080u?0x7fu:word&0xffu);
}
}
std::optional<std::array<OriginalWin95VxdPortWrite,14>>
candidateOriginalVxdVoiceEnvelopePrelude(
    const std::uint8_t voice,
    const OriginalVxdEncodedVoiceEnvelopePrelude& s) noexcept {
    if(voice>=kOriginalCreativeMusicVoices ||
       !s.from_actual_original_driver_record)return std::nullopt;
    const auto ptr=[voice](unsigned reg) noexcept {
        return OriginalWin95VxdPortWrite{
            0xE22u,static_cast<std::uint32_t>((reg<<5)|voice),8u};
    };
    const auto w16=[](const std::uint16_t port,const std::uint16_t data) noexcept {
        return OriginalWin95VxdPortWrite{port,data,16u};
    };
    const auto w32=[](const std::uint32_t val) noexcept {
        return OriginalWin95VxdPortWrite{0x620u,val,32u};
    };
    // The first two DWORD resets really are both 0x0000FFFF.
    // It is essential not to collapse these two different registers.
    return std::array<OriginalWin95VxdPortWrite,14>{{
        ptr(3u),w32(0x0000ffffu),  // 5BE4 VTFT
        ptr(2u),w32(0x0000ffffu),  // 5BF4 CVCF
        ptr(4u),w16(0xA22u,combine(s.attack_hold_volume_high_word,
                           specialAttack(s.attack_hold_volume_low_byte_word))),
        ptr(5u),w16(0xA22u,static_cast<std::uint16_t>(s.lfo1_delay_word)),
        ptr(6u),w16(0xA22u,combine(s.attack_hold_mod_high_word,
                           specialAttack(s.attack_hold_mod_low_byte_word))),
        ptr(7u),w16(0xA20u,combine(s.decay_sustain_mod_high_word,
                           s.decay_sustain_mod_low_byte)),
        ptr(7u),w16(0xA22u,static_cast<std::uint16_t>(s.lfo2_delay_word))
    }};
}
std::uint8_t originalCreativePcodEnvelopeRateFromWord(
    const std::uint16_t input) noexcept {
    // Original PCOD 0x3791 unsigned compare: first threshold STRICTLY
    // LESS THAN the input wins. Equality does not match. Default=127.
    for(std::uint16_t i=0;i<kOriginalPcodRateThresholds.size();++i)
        if(kOriginalPcodRateThresholds[i]<input)
            return static_cast<std::uint8_t>(i);
    return 127;
}
std::uint8_t originalCreativePcodAttackRateFromWord(
    const std::uint16_t input) noexcept {
    // Original LCOD+0x74F2: if zero, return 0x80. Else start rate1,
    // read table+2, advance until a STRICT threshold < input, and
    // if none through index126 matches, return 0x7E.
    if(input==0)return 128;
    for(std::uint16_t i=1;i<kOriginalPcodAttackThresholds.size();++i)
        if(kOriginalPcodAttackThresholds[i]<input)
            return static_cast<std::uint8_t>(i);
    return 126;
}
std::optional<std::uint8_t>
candidateOriginalCreative1MgmGeneratorEnvelopeRate(
    const std::uint16_t id,const std::uint16_t original_raw) noexcept {
    // Actual PCOD+0x37BA dispatcher from the ORIGINAL LE relocations:
    // indexed generator-ID bytes at PCOD+0x3959 select 32-bit jump table
    // PCOD+0x38FD. Callers PCOD+0x2A92/+0x2AF3 pass original SF1/SBK
    // raw 16-bit generator WORD and a third scale argument of 1.
    // Two groups have different original functions, not a common formula.
    switch(id){
        case 26u:case 34u:return originalCreativePcodAttackRateFromWord(original_raw);
        case 28u:case 30u:case 38u:return originalCreativePcodEnvelopeRateFromWord(original_raw);
        default:return std::nullopt;
    }
}
std::uint8_t originalCreativeSf2VoiceAttackRate(
    const std::int16_t raw) noexcept {
    const auto x=static_cast<std::int32_t>(raw);
    // Original Win95 SBAWE.VXD PCOD+0x3BD4 exact signed integer math;
    // C++ / truncates toward zero, including for negative operands.
    if(x<-599) {
        const auto attack=(37-x)/75+8;
        return static_cast<std::uint8_t>(std::min(128,attack));
    }
    if(x>=4300)return 1;
    const auto q=(x+500)/150;
    const auto value=((~q&7)|8)>>(q/8&31);
    return static_cast<std::uint8_t>(std::max(1,value));
}
// Original Format-2 PCOD+0x3BD4 uses a deliberately 32-bit intermediate
// followed by signed truncation, not floating point. Widen for the multiply
// then explicitly emulate the x86 low DWORD before signed division.
namespace {
std::uint32_t originalSf2PcodExponential(
    const std::int32_t x,const std::int32_t bias) noexcept {
    const auto product=(static_cast<std::int64_t>(x)+bias)*65536ll;
    const auto low=static_cast<std::uint32_t>(product);
    const auto signedProduct=static_cast<std::int64_t>(low)-
        (low>=0x80000000u?4294967296ll:0ll);
    const auto q=static_cast<std::int32_t>(signedProduct/1200ll);
    const auto shift=static_cast<std::uint32_t>(16-q/65536)&31u;
    return (65536u+(static_cast<std::uint32_t>(q)&65535u))>>shift;
}
}
std::optional<std::uint16_t> originalCreativeSf2VoiceEnvelopeOrLfoWord(
    const std::uint16_t op,const std::int16_t merged_word) noexcept {
    const auto x=static_cast<std::int32_t>(merged_word);
    switch(op) {
        case 21u:case 23u:case 25u:case 33u: // LFO and envelope delays
            if(x<-11999)return 32768u;
            if(x>=5484)return 0u;
            return static_cast<std::uint16_t>(std::max(0,
                32768-static_cast<std::int32_t>(originalSf2PcodExponential(x,12516))));
        case 22u:case 24u: // LFO frequency
            if(x<-15999)return 0u;
            return static_cast<std::uint16_t>(std::min(
                255u,originalSf2PcodExponential(x,9126)));
        case 26u:case 34u: // both ORIGINAL SF2 descriptor attack fields
            return originalCreativeSf2VoiceAttackRate(merged_word);
        case 27u:case 35u: // envelope hold
            if(x<=-5369)return 127u;
            return static_cast<std::uint16_t>(std::max(0,
                127-static_cast<std::int32_t>(originalSf2PcodExponential(x,4130))));
        case 28u:case 30u:case 36u:case 38u: { // decay and release
            if(x<=1800)return static_cast<std::uint16_t>(
                std::min(127,(37-x)/75+39));
            const auto q=(x-1800)/150;
            return static_cast<std::uint16_t>(
                ((~q&7)|8)>>((q/8)&31));
        }
        case 29u:case 37u: // SF2 voice sustain clamp after merge
            return static_cast<std::uint16_t>(std::clamp(x,0,127));
        default:return std::nullopt;
    }
}
std::optional<std::int16_t> originalCreativeSf2KeyScaledEnvelopeWord(
    const std::uint16_t op,const std::int16_t target,
    const std::int16_t auto_scale,const std::uint8_t key,
    const std::uint16_t bank) noexcept {
    if(key>127)return std::nullopt;
    // Verified pairs: 27<-31, 28<-32, 35<-39 and 36<-40.
    // The caller supplies the corresponding source field explicitly.
    if(op!=27u && op!=28u && op!=35u && op!=36u)
        return std::nullopt;
    if(bank>=128u)return target; // drum route bypasses melodic key scaling
    const auto scaled=static_cast<std::int32_t>(target)+
        (60-static_cast<std::int32_t>(key))*static_cast<std::int32_t>(auto_scale);
    return static_cast<std::int16_t>(std::clamp<std::int32_t>(
        scaled,-32768,32767));
}
} // namespace awe64::creative
