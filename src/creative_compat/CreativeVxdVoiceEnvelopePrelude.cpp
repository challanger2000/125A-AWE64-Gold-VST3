#include "CreativeVxdVoiceEnvelopePrelude.h"
#include <cstdint>
#include <array>
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
} // namespace awe64::creative
