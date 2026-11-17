#pragma once
// Shared source arithmetic, factored from CreativeSf2OriginalTargets.cpp.
// Original SBAWE.VXD LCOD+0x5B50..0x5B7E/+0x5D7B..0x5ECE.
// No private table values, floats, heap, I/O or chip processing.
#include <cstdint>
#include <optional>
#include <span>
namespace awe64::original_vxd::detail {
struct NotePitchTargets {std::uint16_t initial_ip=0,pitch_target=0;};
[[nodiscard]] inline NotePitchTargets note_pitch_targets(
    std::uint16_t base,std::int16_t channel_tune,std::int32_t bend,
    std::int16_t generator7) noexcept {
    const auto cap=[](std::uint32_t n) noexcept {return n>0xffffu?0xffffu:n;};
    const auto initial=cap(static_cast<std::uint32_t>(base)+
        static_cast<std::uint32_t>(static_cast<std::int32_t>(channel_tune))+
        static_cast<std::uint32_t>(bend));
    const auto normalized=cap(initial+static_cast<std::uint32_t>(
        static_cast<std::int32_t>(generator7)*16));
    std::uint32_t value=1u<<(normalized>>12u);
    if(normalized&0x0800u)value+=(value*0x102eu)/10000u;
    if(normalized&0x0400u)value+=(value*0x0764u)/10000u;
    if(normalized&0x0200u)value+=(value*0x0389u)/10000u;
    value+=value>>2u;
    return {static_cast<std::uint16_t>(initial),
            static_cast<std::uint16_t>(cap(value))};
}
[[nodiscard]] inline std::optional<std::uint16_t> attack_volume_target(
    std::int16_t attenuation,std::span<const std::uint16_t> positive16,
    std::span<const std::uint16_t> negative15) noexcept {
    if(positive16.size()!=16u)return std::nullopt;
    const auto a=static_cast<std::int32_t>(attenuation);
    const auto nibble=static_cast<unsigned>(a<0?-a:a)&15u;
    const bool negative=a<0 && nibble!=0u;
    if(negative && negative15.size()!=15u)return std::nullopt;
    const auto value=negative?negative15[15u-nibble]:positive16[nibble];
    const auto shift=static_cast<unsigned>(static_cast<std::uint8_t>(a/16))&31u;
    return shift>=16u?std::uint16_t{0}:static_cast<std::uint16_t>(value>>shift);
}
} // namespace awe64::original_vxd::detail
