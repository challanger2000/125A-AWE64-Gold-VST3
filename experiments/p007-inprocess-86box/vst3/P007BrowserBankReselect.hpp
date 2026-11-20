#pragma once
// P007BQ: same-value host automation may be coalesced by a VST3 host.
// Re-selecting a bank in the P165 browser must explicitly queue the
// original Creative CC0/CC32/PC selection, including both CH10 kit lanes.
// This is a UI->audio atomic request, NOT UI-thread UART access.
#include <atomic>
#include <cstdint>
namespace p007::browser_bank {
inline constexpr std::uint16_t maxLogicalBank=129u;
inline bool reselect(std::uint16_t logicalBank,
                     std::atomic<bool>& resend) noexcept {
    if(logicalBank>maxLogicalBank)return false;
    resend.store(true,std::memory_order_release);
    return true;
}
} // namespace p007::browser_bank
