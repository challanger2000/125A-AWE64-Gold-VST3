#pragma once
// P007BO: bound untrusted VST3 host iterator sizes in the realtime callback.
// A malicious/corrupt host may report INT_MAX entries without allocating
// them. Never loop to that count; recovery remains on original MIDI UART.
#include <algorithm>
#include <cstdint>
namespace p007::host_budget {
inline constexpr std::int32_t maxEventDescriptors=4096;
inline constexpr std::int32_t maxParameterQueues=64;
inline constexpr std::int32_t maxPointsPerQueue=4096;
inline constexpr std::int32_t safeCount(std::int32_t reported,
                                       std::int32_t maximum) noexcept {
    return reported>0 ? std::min(reported,maximum):0;
}
inline constexpr bool invalidCount(std::int32_t reported,
                                   std::int32_t maximum) noexcept {
    return reported<0||reported>maximum;
}
} // namespace p007::host_budget
