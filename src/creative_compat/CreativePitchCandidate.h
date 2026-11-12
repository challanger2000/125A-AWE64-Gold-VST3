#pragma once
// P002 measured Creative 1MGM IP register candidate. Not full driver parity.
// Generator 55's meaning across all SF1 banks remains to be confirmed with
// original Win95/EMU8000 traces. No GM-preset-specific constants or LUTs.
#include "CreativeMidiRomPlan.h"
#include "CreativeCandidateRegister.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace awe64::creative {
struct CreativePitchResult {
    std::uint16_t ip=0;
    std::int32_t base_steps=0;
    std::int32_t bend_steps=0;
    std::uint16_t source_pitch_raw=0;
};
// Experimental observed relationship for 1MGM zones without nondefault
// root/scale/coarse/fine/key overrides. Fail closed for unsupported tuning.
[[nodiscard]] std::optional<CreativePitchResult> candidateOriginalRomIp(
    const SbkLayer& layer, std::uint8_t midi_key, std::uint16_t bend14,
    std::uint16_t bend_range_cents=200) noexcept;

// Physical voice comes from the future Creative allocator; this routine
// independently calculates actual E22/E20 pitch register port writes.
[[nodiscard]] std::optional<std::array<PortWord,2>> candidateOriginalRomIpWrites(
    const CreativeRomNotePlan& note, std::size_t layer_index,
    std::uint8_t physical_voice, const MidiState& state,
    std::uint16_t bend_range_cents=200) noexcept;
} // namespace awe64::creative
