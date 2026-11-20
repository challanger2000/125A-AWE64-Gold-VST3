#pragma once
// P003 exact empirically observed Creative Win95 source-bank sample
// conversion: 3-tap original-trace oracle. FULL original Win95 SMALW/SMLD
// captures from 2026-11-13/15 now validate two different SF2 pools
// (260921 and 32768 words) bit-for-bit; not yet generalized to
// arbitrary SoundFont layouts or all original Creative driver variants.
// Pure PCM transform; NOT a DSP, address allocator or voice renderer.
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace awe64::creative {
// Requires even-size PCM16LE, first and last source sample zero.
// Long >=3-zero runs pass through zero; other words are transformed via
// floor((6*current - previous - next) / 8) with exact signed arithmetic.
// Source-specific behavior must be externally source-SHA-attested.
[[nodiscard]] std::optional<std::vector<std::int16_t>>
p003PredictOwnerObservedThreeTapRamWords(
    std::span<const std::uint8_t> source_smpl_pcm_le16);
} // namespace awe64::creative
