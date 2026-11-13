#pragma once
// P002: Standard RIFF SoundFont2 RAM-sample PCM inventory ONLY.
// This module does NOT assume how the 1996 Creative Win95 driver places
// samples into EMU8000 DRAM, adjusts addresses, pads loops, allocates
// memory, or writes chip port registers. Such effects need independent
// source/guest-register evidence. The view is read-only and points into
// caller-owned bytes; keep the source bank alive while using it.
//
// An uncompressed SF2 sdta/smpl chunk contains signed 16-bit, LE PCM.
// sm24 and linked/stereo sample types are NOT approved by this gate.
// All length, sample-header and memory bounds are checked before any
// sample data could be passed to an original-source RAM loader.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace awe64::creative {
struct OriginalSf2RamSamplePcmView {
    std::uint16_t sample_id=0u;
    std::uint32_t start_word=0u;
    std::uint32_t end_word=0u;
    std::uint32_t loop_start_word=0u;
    std::uint32_t loop_end_word=0u;
    std::uint32_t sample_rate_hz=0u;
    std::uint8_t original_pitch=0u;
    std::int8_t pitch_correction=0;
    std::uint32_t total_pool_words=0u;
    std::span<const std::uint8_t> sample_pcm_le16{};
    // SDTA sample bytes, without loop padding or guessed DRAM address.
    // Exactly 2*(end_word-start_word) bytes, including the original
    // waveform before its first/last source sample index.
};
[[nodiscard]] std::optional<OriginalSf2RamSamplePcmView>
candidateOriginalSf2ReadRamMonoSamplePcm(
    std::span<const std::uint8_t> raw_soundfont,
    std::uint16_t sample_id,
    std::size_t original_emu8000_ram_capacity_words) noexcept;
} // namespace awe64::creative
