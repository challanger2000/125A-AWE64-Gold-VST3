#include "CreativeVxdVoiceEnvelopePrelude.h"
#include <cstdint>
namespace awe64::creative {
namespace {
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
} // namespace awe64::creative
