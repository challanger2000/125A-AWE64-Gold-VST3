#pragma once
// P002: specific authentic SBAWE.VXD sample-address update cases 5 and 6.
// This is NOT generic SoundFont key-on, and is not a guessed loop policy.
#include "CreativeSbkLayers.h"
#include "CreativeVxdMonoVoiceRanking.h"
#include <array>
#include <cstdint>
#include <optional>
namespace awe64::creative {
enum class OriginalVxdSampleAddressCase : std::uint8_t {
    DriverCase5 = 5,  // VXD LCOD +0x1B4A; loop end +1 / loop start
    DriverCase6 = 6   // VXD LCOD +0x1C19; sample end +8 / sample end +4
};
// Original driver reads the CURRENT 32-bit PSST value first and preserves
// bits 31..24 (pan / auxiliary send). Caller MUST obtain actual hardware
// PSST, rather than assuming a default zero upper byte.
// Each case emits CSL FIRST (DATA0 index7) and PSST SECOND (DATA0 index6),
// using original E22 OUT AL then 620 OUT EAX for each pair.
[[nodiscard]] std::optional<std::array<OriginalWin95VxdPortWrite,4>>
candidateOriginalVxdSampleAddressUpdate(
    const SbkRomAddress& sample, std::uint8_t physical_voice,
    OriginalVxdSampleAddressCase driver_case,
    std::uint32_t actual_prior_psst) noexcept;
} // namespace awe64::creative
