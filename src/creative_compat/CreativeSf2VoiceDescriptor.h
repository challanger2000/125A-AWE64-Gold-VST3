#pragma once
// P002: original Creative SF2 0x94-byte note-start descriptor transition.
// Input is an ALREADY MERGED genuine format-2 voice descriptor, not a bank.
// This does not manufacture template defaults, sample data, or NoteOn timing.
#include "CreativeControllerVolume.h"
#include <array>
#include <cstdint>
#include <optional>
namespace awe64::creative {
inline constexpr std::size_t kCreativeSf2VoiceDescriptorBytes=0x94u;
using CreativeSf2VoiceDescriptorBytes =
    std::array<std::uint8_t,kCreativeSf2VoiceDescriptorBytes>;
struct OriginalSf2VoiceStartConditions {
    std::uint16_t creative_bank;
    std::uint8_t midi_key, midi_velocity;
    std::uint8_t channel_pan, channel_chorus, channel_reverb;
    std::uint8_t device_effects_flag;
    OriginalCreativeVolumeTerms volume; // CC7/CC11 and original device +0x10
};
// Returns the original format-2 descriptor after PCOD+0x3BD4 and the
// recovered VXD per-voice arithmetic, or nullopt for unsupported conditions.
// Word offsets are 2*operator for operators 0..58; sample metadata starts at
// 0x76. Everything not explicitly transformed is preserved byte-for-byte.
// Only ROM-mono sampleType 0x8001 has been verified. The original note-start
// clears that qualifier in its outgoing 148-byte record to 0x0001.
[[nodiscard]] std::optional<CreativeSf2VoiceDescriptorBytes>
candidateOriginalSf2EncodedVoiceDescriptor(
    const CreativeSf2VoiceDescriptorBytes& merged,
    const OriginalSf2VoiceStartConditions& context) noexcept;
}
