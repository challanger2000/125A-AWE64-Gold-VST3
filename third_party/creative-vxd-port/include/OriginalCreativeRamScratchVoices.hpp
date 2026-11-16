#pragma once
// Independently reconstructed 1996 Creative PCOD+0x26AF / +0x283D RAM
// scratch-voice lifecycle. No x86 binary, private tables or trace replay.
// Each 1024-word upload cycle borrows <=10 actual EMU8000 voices through
// the existing source-derived live allocator, initializes them, and restores
// their source-observed register state afterwards. Non-real-time only.
#include "OriginalCreativeVxdLive.hpp"
#include "OriginalCreativeSf2RamUpload.hpp"
#include <array>
#include <cstdint>

namespace awe64::original_vxd {
inline constexpr unsigned original_ram_scratch_slots=10u;
struct OriginalCreativeRamScratchCycle {
    DeviceState* device=nullptr;
    PortIo io{};
    std::array<std::uint8_t,original_ram_scratch_slots> selected{};
    bool active=false;
    bool faulted=false;
    std::uint32_t cycles_completed=0;
    std::uint32_t selected_voices=0;

    // PCOD+0x26AF: ten calls to original LCOD voice allocator, with
    // chip-register reset/setup and mutable original mode-count handling.
    // Returns false if any original chip read or write fails; cannot roll
    // back a partially transmitted hardware batch.
    [[nodiscard]] bool before_block() noexcept;
    // PCOD+0x283D: restore source 4x 32-bit register writes per voice,
    // mark original voice+0x42=0x0FFE, voice+0x59=1, device+0x18++.
    [[nodiscard]] bool after_block() noexcept;
    [[nodiscard]] OriginalCreativeRamBlockHooks hooks() noexcept;
};
} // namespace awe64::original_vxd
