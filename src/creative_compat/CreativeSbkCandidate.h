#pragma once
// P002 bounded bridge: authenticated zone cutoff -> pre-existing MIDI IFATN
// candidate, WITHOUT guessing volume/attack/allocator/clock conversions.
#include "CreativeCandidateRegister.h"
#include "CreativeSbkDescriptor.h"
namespace awe64::creative {
struct UnresolvedCreativeVoiceTerms {
    std::uint8_t combined_volume_index = 0; // original driver mapping unknown
    std::uint8_t sample_attenuation = 0;     // do NOT pass raw SBK gen48 here
    bool velocity_cutoff_enabled = false;    // attack envelope gate unproven
};
[[nodiscard]] std::optional<std::array<PortWord,2>> candidateSbkIfatnWrites(
    const MidiMessage& midi, std::uint8_t physicalVoice,
    const SbkZoneDescriptor& selectedZone,
    const UnresolvedCreativeVoiceTerms& unresolved) noexcept;
} // namespace awe64::creative
