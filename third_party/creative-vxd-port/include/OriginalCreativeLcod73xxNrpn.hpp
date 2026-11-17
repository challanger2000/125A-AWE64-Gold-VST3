#pragma once
// Original Win95 SBAWE.VXD 4.38.0.2 LCOD+0x7309, +0x7350, +0x740A.
// Source-derived NRPN LSB/MSB callbacks and shared 30-parameter state.
// No source-driver data or artificial SoundFont/EMU8000 implementation.
// Research-only: caller must authenticate private source before use.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace awe64::original_vxd::research {
struct OriginalNrpnMemory {
    std::span<std::uint8_t> device{};
    bool original_source_authenticated=false; // caller assertion, not proof
};
namespace nrpn_detail {
constexpr std::size_t kChannels=16;
constexpr std::size_t kParameters=30;
constexpr std::size_t kMinimumDevice=0x151e;
[[nodiscard]] inline std::uint16_t r16(std::span<const std::uint8_t> d,
                                       std::size_t p) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(d[p]) |
        (static_cast<unsigned>(d[p+1u])<<8u));
}
inline void w16(std::span<std::uint8_t> d,std::size_t p,
                std::uint16_t value) noexcept {
    d[p]=static_cast<std::uint8_t>(value);
    d[p+1u]=static_cast<std::uint8_t>(value>>8u);
}
}
// LCOD+0x7350: returns original EAX (0 on a valid 0x7F:parameter
// selection, 1 on all other high-byte branches). High byte ZERO also
// resets all 30 parameter records and channel+0x450, then returns 1.
// This is NOT an ordinary boolean success/failure result.
[[nodiscard]] inline std::optional<std::uint32_t> originalLcod7350(
    OriginalNrpnMemory original,std::uint8_t channel,
    std::uint16_t selection) noexcept {
    using namespace nrpn_detail;
    if(!original.original_source_authenticated ||
       original.device.size()<kMinimumDevice || channel>=kChannels)
        return std::nullopt;
    auto d=original.device;
    const auto channelOffset=static_cast<std::size_t>(channel)*0x24u;
    const auto rowOffset=static_cast<std::size_t>(channel)*0x5cu;
    const auto high=static_cast<std::uint8_t>(selection>>8u);
    if(high==0u) {
        w16(d,0x0f5eu+rowOffset,0u);
        w16(d,0x0450u+channelOffset,0u);
        for(std::size_t k=0u;k<kParameters;++k){
            d[0x0f9cu+rowOffset+k]=0u;
            w16(d,0x0f60u+rowOffset+k*2u,0x2000u);
        }
    }
    if(high!=0x7fu || (selection&0x7fu)>=kParameters)
        return 1u;
    const auto parameter=static_cast<std::size_t>(selection&0x7fu);
    std::uint16_t sourceValue=0x2000u;
    if(r16(d,0x0f5eu+rowOffset)!=0u &&
       d[0x0f9cu+rowOffset+parameter]==1u)
        sourceValue=r16(d,0x0f60u+rowOffset+parameter*2u);
    w16(d,0x0464u+channelOffset,sourceValue);
    return 0u;
}
// LCOD+0x7309: NRPN low byte; original AND 0x7F00 (including
// clearing bit 15 of the existing high byte) and MOVZX of LOW byte.
[[nodiscard]] inline std::optional<std::uint32_t> originalLcod7309(
    OriginalNrpnMemory original,std::uint8_t channel,
    std::uint8_t value) noexcept {
    using namespace nrpn_detail;
    if(!original.original_source_authenticated ||
       original.device.size()<kMinimumDevice || channel>=kChannels)
        return std::nullopt;
    auto d=original.device;
    const auto base=0x0462u+static_cast<std::size_t>(channel)*0x24u;
    const auto old=r16(d,base);
    const auto next=static_cast<std::uint16_t>((old&0x7f00u)|value);
    w16(d,0x045eu+static_cast<std::size_t>(channel)*0x24u,0x0200u);
    w16(d,base,next);
    return originalLcod7350(original,channel,next);
}
// LCOD+0x740A: NRPN high byte; original SHL AX,8 followed by OR
// with original low byte (no 7-bit approximation at this boundary).
[[nodiscard]] inline std::optional<std::uint32_t> originalLcod740a(
    OriginalNrpnMemory original,std::uint8_t channel,
    std::uint8_t value) noexcept {
    using namespace nrpn_detail;
    if(!original.original_source_authenticated ||
       original.device.size()<kMinimumDevice || channel>=kChannels)
        return std::nullopt;
    auto d=original.device;
    const auto base=0x0462u+static_cast<std::size_t>(channel)*0x24u;
    const auto old=r16(d,base);
    const auto next=static_cast<std::uint16_t>((old&0x00ffu)|
                                               (static_cast<unsigned>(value)<<8u));
    w16(d,0x045eu+static_cast<std::size_t>(channel)*0x24u,0x0200u);
    w16(d,base,next);
    return originalLcod7350(original,channel,next);
}
} // namespace awe64::original_vxd::research
