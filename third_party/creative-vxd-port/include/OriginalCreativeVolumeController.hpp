#pragma once
// Source x86 original Creative SBAWE.VXD LCOD+0x640A/+0x6439 live CC7/CC11.
// Original VXD lookup TABLES are injected from verified local source; NO
// proprietary table bytes are distributed or guessed in this module.
#include "OriginalCreativeMidiEngine.hpp"

namespace awe64::original_vxd {
using OriginalVolumeResources = MidiEngine::VolumeSource;

// CC7 / CC11 are original per-channel state, each active physical voice
// recalculates IFATN's LOW byte while retaining its current HIGH filter byte.
[[nodiscard]] MidiResult original_cc7_cc11_live(MidiEngine&,
    std::uint8_t channel,std::uint8_t cc,std::uint8_t value,PortIo) noexcept;
} // namespace awe64::original_vxd
