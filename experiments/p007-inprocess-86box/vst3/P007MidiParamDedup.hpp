#pragma once
// P007BA: A VST3 host can repeat identical automation points on EVERY
// audio block. A duplicate bank/program point must NOT restart the
// 20ms-per-message original Creative BankProgramPacer indefinitely.
#include <atomic>
#include <cstdint>
namespace p007::midi_param {
inline bool updateIfChanged(std::atomic<std::uint8_t>& dest,
                            std::uint8_t value) noexcept {
    return dest.exchange(value,std::memory_order_acq_rel)!=value;
}
}
