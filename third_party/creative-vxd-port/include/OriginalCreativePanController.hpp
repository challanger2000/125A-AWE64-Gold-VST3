#pragma once
// Original SBAWE.VXD LCOD+0x68F3 / +0x6980 channel-pan hardware updates.
// This is the immediate one-step routine. The original +0x3B43 timer
// callback is NOT calibrated; owner must call service_pan_tick only at
// independently justified guest-compatible times.
#include "OriginalCreativeMidiEngine.hpp"

namespace awe64::original_vxd {
// Applies CC10 target to live matched voice rows, then executes original
// first one-step PSST/PTRX update. Returns Incomplete while future timer
// callbacks are required. Missing original pan state fails closed.
[[nodiscard]] MidiResult original_channel_pan_cc10(MidiEngine&,
    std::uint8_t channel,std::uint8_t value,PortIo) noexcept;

// Research-only single follow-on timer invocation. This does NOT assert
// the Win95 hardware timer cadence and must never be called per audio frame
// as a guessed original clock. The only hardware mutation is one pan step.
[[nodiscard]] MidiResult service_original_pan_voice_step(MidiEngine&,
    std::uint8_t physical,PortIo) noexcept;
} // namespace awe64::original_vxd
