#pragma once
// P002 EXPERIMENTAL, bounded by observed GM0/GM40 Win95 Creative data.
// This is a real standalone MIDI->initial IFATN register candidate, not yet
// a complete original driver. It DOES NOT allocate EMU8000 voices or samples.
// Instrument-dependent parameters MUST ultimately come from original bank
// structures, NEVER from a hard-coded GM program ID.
#include "CreativeMidi.h"
#include <array>
#include <cstdint>
#include <optional>

namespace awe64::creative {
struct CandidateVoiceParameters {
    std::uint8_t combined_volume_index = 0; // observed instrument/channel term; provenance pending
    std::uint8_t sample_attenuation = 0;    // original bank/zone input, not "per-preset patch"
    std::uint8_t initial_cutoff = 0;        // observed original per-zone filter base
    bool velocity_cutoff_enabled = false;   // original volume-attack modulation gate to establish
};
struct PortWord {
    std::uint16_t port = 0;
    std::uint16_t value = 0;
};
struct InitialIfatn {
    std::uint8_t cutoff = 0;
    std::uint8_t attenuation = 0;
    std::uint16_t word = 0;
};
// Requires 1 <= MIDI velocity <= 127. Zero velocity is NOTE OFF, not a voice.
// No heap operations, no IO and no unexpected fallback to a fake instrument.
[[nodiscard]] std::optional<InitialIfatn> candidateInitialIfatn(
    std::uint8_t velocity, const CandidateVoiceParameters& bank) noexcept;
// The actual original Windows 95 SBAWE.VXD (author's installed driver)
// branches on original bank < 128 and its already ENCODED volume-attack
// rate < 125 before scaling the initial filter cutoff by velocity.
// This is NOT a converter for raw SoundFont generator 34.
[[nodiscard]] std::optional<InitialIfatn> candidateOriginalIfatnFromEncodedAttack(
    std::uint8_t velocity, const CandidateVoiceParameters& terms,
    std::uint16_t creative_bank, std::uint16_t encoded_vol_attack_rate) noexcept;
// The physical voice is supplied by the as-yet unverified Creative allocator.
// Port writes are generated independently from MIDI and parameter inputs,
// not copied from original EMU8K_TRACE. Only IFATN is generated so far.
[[nodiscard]] std::optional<std::array<PortWord,2>> candidateInitialIfatnWrites(
    const MidiMessage& midi, std::uint8_t physicalVoice,
    const CandidateVoiceParameters& bank) noexcept;
}
