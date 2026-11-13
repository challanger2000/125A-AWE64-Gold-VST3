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
#include <array>
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
// Inspect ALL standard format-2 sample headers at activation.
// A ROM-only experimental VST3 must reject an entire bank containing
// even one RAM/linked/unsupported sample: checking shdr[0] alone
// could otherwise silently render the wrong hardware waveform.
// Classification is read-only, allocation-free and never approves
// original Creative DRAM placement or the full bank's musical semantics.
struct OriginalSf2SampleMemoryInventory {
    std::size_t sample_headers=0u;
    std::size_t rom_mono=0u;
    std::size_t ram_mono=0u;
    std::size_t unsupported=0u;
};
[[nodiscard]] std::optional<OriginalSf2SampleMemoryInventory>
candidateOriginalSf2InventoryAllSampleTypes(
    std::span<const std::uint8_t> raw_soundfont) noexcept;
[[nodiscard]] std::optional<OriginalSf2RamSamplePcmView>
candidateOriginalSf2ReadRamMonoSamplePcm(
    std::span<const std::uint8_t> raw_soundfont,
    std::uint16_t sample_id,
    std::size_t original_emu8000_ram_capacity_words) noexcept;
// Original Creative format-2 SF2 preset/instrument zone selection
// connected to ACTUAL RAM sample PCM bytes. The existing source-verified
// layer scanner identifies at most six descriptor groups per MIDI NoteOn;
// this function DOES NOT approve a hardware RAM upload address or start
// a voice, nor infer a missing original Creative memory allocation.
inline constexpr std::size_t kOriginalCreativeRamNoteMaxLayers=6u;
struct OriginalSf2RamNotePcmPlan {
    std::array<OriginalSf2RamSamplePcmView,
               kOriginalCreativeRamNoteMaxLayers> layers{};
    std::uint8_t count=0u;
    std::uint16_t bank=0u;
    std::uint8_t program=0u,key=0u,velocity=0u;
};
[[nodiscard]] std::optional<OriginalSf2RamNotePcmPlan>
candidateOriginalSf2RamMonoNotePcmPlan(
    std::span<const std::uint8_t> raw_soundfont,
    std::uint16_t bank,
    std::uint8_t program,
    std::uint8_t midi_key,
    std::uint8_t midi_velocity,
    std::size_t original_emu8000_ram_capacity_words) noexcept;
// P002 link: independently authenticated original physical RAM placement
// + actual MIDI bank/program/velocity layer selection + source VXD
// NoteOn address geometry. The source bank is caller-owned, no heap or I/O.
// Only one original RAM bank and sample 82 were independently captured;
// address predictions for any OTHER sample are hypotheses requiring their
// own live Creative driver register/PCM comparison, NOT approved audio.
//
// Return physical WORD addresses only; original per-voice Q/pan/send,
// physical allocation, full 61-register NoteOn and clocks remain unknown.
struct OriginalSf2RamPhysicalLayer {
    std::uint16_t sample_id=0u;
    std::uint32_t physical_sample_start=0u;
    std::uint32_t physical_sample_end=0u;
    std::uint32_t physical_loop_start=0u;
    std::uint32_t physical_loop_end=0u;
    std::uint32_t candidate_initial_ccca=0u;
    std::uint32_t candidate_initial_psst=0u;
    std::uint32_t candidate_initial_csl=0u;
};
struct OriginalSf2RamPhysicalNotePlan {
    std::array<OriginalSf2RamPhysicalLayer,kOriginalCreativeRamNoteMaxLayers>
        layers{};
    std::uint8_t count=0u;
};
[[nodiscard]] std::optional<OriginalSf2RamPhysicalNotePlan>
candidateOriginalSf2RamPhysicalNotePlan(
    std::span<const std::uint8_t> raw_soundfont,
    std::uint16_t bank,
    std::uint8_t program,
    std::uint8_t midi_key,
    std::uint8_t midi_velocity,
    std::uint32_t independently_observed_ram_sample_base,
    std::size_t independently_observed_source_pcm_words,
    bool original_creative_driver_physical_ram_upload_verified) noexcept;

} // namespace awe64::creative
