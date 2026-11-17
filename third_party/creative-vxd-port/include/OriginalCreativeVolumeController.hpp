#pragma once
// Source x86 original Creative SBAWE.VXD LCOD+0x640A/+0x6439 live CC7/CC11.
// Original VXD lookup TABLES are injected from verified local source; NO
// proprietary table bytes are distributed or guessed in this module.
#include "OriginalCreativeMidiEngine.hpp"
#include <optional>
#include <span>

namespace awe64::original_vxd {
using OriginalVolumeResources = MidiEngine::VolumeSource;

// Original LCOD+0x5753..0x5800 and +0x648E..0x652A use these
// same signed arithmetic terms. Tables are private, authenticated inputs;
// cc11_table_128 is LCOD+0x2EA0 and cc7_table_128 is +0x2F20.
[[nodiscard]] std::optional<std::uint8_t> original_creative_voice_attenuation(
    OriginalVolumeResources const&,std::uint8_t original_velocity_level,
    std::int16_t original_amplitude_word,std::uint8_t cc7,
    std::uint8_t cc11,bool original_rom_sample) noexcept;

struct OriginalNoteVolumeResources {
    bool original_velocity_table_verified=false;
    // Original LCOD+0x2E20, exactly 128 entries. Caller owns the table
    // until after all audio callbacks have stopped. No table is bundled.
    std::span<const std::uint8_t> velocity_table{};
    OriginalVolumeResources controllers{};
};
// NoteOn-specific entry: derive the velocity level from the original
// table rather than requesting a separately recorded per-note value.
[[nodiscard]] std::optional<std::uint8_t> original_note_attenuation(
    OriginalNoteVolumeResources const&,std::uint8_t velocity,
    std::int16_t original_amplitude_word,std::uint8_t cc7,
    std::uint8_t cc11,bool original_rom_sample) noexcept;

// CC7 / CC11 are original per-channel state, each active physical voice
// recalculates IFATN's LOW byte while retaining its current HIGH filter byte.
[[nodiscard]] MidiResult original_cc7_cc11_live(MidiEngine&,
    std::uint8_t channel,std::uint8_t cc,std::uint8_t value,PortIo) noexcept;
} // namespace awe64::original_vxd
