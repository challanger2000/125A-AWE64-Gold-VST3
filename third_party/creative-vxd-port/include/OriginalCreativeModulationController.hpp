#pragma once
// Creative SBAWE.VXD original LCOD+0x65D7..0x6681: CC1 hardware update.
// Uses source-authenticated channel+0x44D and voice row+0x14 values,
// avoiding any guessed mod-wheel scaling or secondary modulation DSP.
#include "OriginalCreativeMidiEngine.hpp"
namespace awe64::original_vxd {
[[nodiscard]] MidiResult original_cc1_modulation_live(
    MidiEngine&,std::uint8_t channel,std::uint8_t value,PortIo) noexcept;
} // namespace awe64::original_vxd
