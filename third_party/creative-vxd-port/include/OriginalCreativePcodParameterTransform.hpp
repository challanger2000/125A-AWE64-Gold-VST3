#pragma once
// Source-derived C++23 research port of SBAWE.VXD 4.38.0.2 PCOD+0x37BA.
// Caller offsets: LCOD+0x7136, +0x720A and +0x7237.
// NO private Creative code or lookup data is embedded. Not yet live in VST3.
#include <algorithm>
#include <bit>
#include <cstdint>
#include <optional>
#include <span>
namespace awe64::original_vxd::research {
using PcodTable=std::span<const std::uint16_t>;
[[nodiscard]] constexpr std::int32_t x86s32(std::uint32_t x) noexcept {
    return std::bit_cast<std::int32_t>(x);
}
[[nodiscard]] constexpr std::int32_t x86mul(
    std::int32_t a,std::int32_t b) noexcept {
    return x86s32(static_cast<std::uint32_t>(a)*static_cast<std::uint32_t>(b));
}
[[nodiscard]] constexpr std::int32_t x86sub(
    std::int32_t a,std::int32_t b) noexcept {
    return x86s32(static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b));
}
[[nodiscard]] constexpr std::int16_t x86word(std::int32_t a) noexcept {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(a));
}
// Original PCOD+0x3791 uses 128 unsigned WORDs from LCOD+0x31A8.
// Require independently source-authenticated input. Strict unsigned <.
[[nodiscard]] inline std::optional<std::int32_t> originalPcodIndex(
    std::uint16_t input,PcodTable original) noexcept {
    if(original.size()!=128u)return std::nullopt;
    if(input==0u)return 127;
    for(std::uint32_t k=0u;k<128u;++k)
        if(original[k]<input)return static_cast<std::int32_t>(k);
    return 127;
}
// All input parameters are 32-bit stack values. Third parameter is
// sign-extended from AX in the original handlers that actually use it.
// x86 imul wraps at 32 bits, and idiv truncates toward zero.
[[nodiscard]] inline std::optional<std::int32_t> originalPcod37ba(
    std::uint32_t op,std::int32_t value,std::int32_t aux,
    PcodTable original={}) noexcept {
    const auto a=x86word(aux);
    switch(op) {
      case 5u:case 6u:case 7u:
        return std::clamp(value,-127,127);
      case 8u:case 10u:case 11u:case 13u:case 22u:case 24u:
        return x86mul(value,2);
      case 9u:case 14u:
        return value/8; // original CDQ/AND EDX,7/ADD/SAR
      case 21u:case 23u:case 25u:case 33u: {
        const auto p=x86mul(x86mul(a,value),1000);
        return x86sub(32768,x86word(p/725));
      }
      case 26u:case 34u:
        return originalPcodIndex(static_cast<std::uint16_t>(value),original);
      case 27u:
        return std::clamp(value/-92+127,0,127);
      case 28u:case 30u:case 38u:
        return originalPcodIndex(
            static_cast<std::uint16_t>(x86mul(a,value)),original);
      case 29u:case 37u:
        return std::clamp(x86mul(value,100)/75,0,127);
      case 36u:
        return x86mul(a,value);
      case 48u:
        return x86mul(x86sub(127,value),75)/10;
      default:
        return value;
    }
}
} // namespace awe64::original_vxd::research
