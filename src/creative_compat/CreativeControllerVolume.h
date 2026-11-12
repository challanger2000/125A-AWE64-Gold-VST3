#pragma once
// P002 native Windows-95 Creative SBAWE.VXD-derived CC7/CC11 volume path.
// This is a bounded, independently generated IFATN candidate. Original
// SF1 generator 48 -> driver encoded sample attenuation remains unresolved.
#include "CreativeCandidateRegister.h"
#include "CreativeMidi.h"
#include <array>
#include <cstdint>
#include <optional>

namespace awe64::creative {
struct OriginalCreativeVolumeTerms {
    std::uint8_t cc7 = 100;              // MIDI channel main volume
    std::uint8_t cc11 = 127;             // MIDI channel expression
    std::uint16_t encoded_sample_attenuation = 0; // NOT raw SBK gen48
    std::int16_t encoded_voice_amplitude = 0; // original driver's signed field
    bool original_1mgm_rom_sample = false; // VXD's 1MGM ROM-specific +16
};
struct OriginalCreativeIfatnTerms {
    OriginalCreativeVolumeTerms volume{};
    std::uint8_t initial_cutoff=0;      // already decoded chip cutoff base
    std::uint16_t bank=0;               // original Creative/SBK bank number
    std::uint16_t encoded_volume_attack_rate=127; // NOT raw SBK gen34
};
// Derives original VXD controller/velocity attenuation, no preset ID switch.
// Does not allocate chip voices or schedule sampleclock writes.
[[nodiscard]] std::optional<std::uint8_t> candidateOriginalCreativeAttenuation(
    std::uint8_t velocity, const OriginalCreativeVolumeTerms& terms) noexcept;
[[nodiscard]] std::optional<InitialIfatn> candidateOriginalCreativeIfatn(
    std::uint8_t velocity, const OriginalCreativeIfatnTerms& terms) noexcept;
// The caller must provide the assigned original physical EMU8000 voice,
// encoded instrument terms and original sample type. MIDI channel state is
// read directly from the MIDI decoder. Port pair is NOT sampled trace replay.
[[nodiscard]] std::optional<std::array<PortWord,2>> candidateOriginalCreativeIfatnWrites(
    const MidiMessage& midi, std::uint8_t physical_voice, const MidiState& state,
    const OriginalCreativeIfatnTerms& instrument) noexcept;
} // namespace awe64::creative
