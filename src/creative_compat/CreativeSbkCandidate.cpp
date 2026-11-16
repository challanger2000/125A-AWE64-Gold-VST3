#include "CreativeSbkCandidate.h"
namespace awe64::creative {
std::optional<std::array<PortWord,2>> candidateSbkIfatnWrites(
    const MidiMessage& midi, std::uint8_t physicalVoice,
    const SbkZoneDescriptor& selectedZone,
    const UnresolvedCreativeVoiceTerms& unresolved) noexcept {
    // Caller is responsible for channel bank/program selection and passing a
    // resolved original 1MGM key zone. Never reuse it for a different key.
    if (midi.kind!=MidiKind::NoteOn || midi.data1!=selectedZone.selected_key ||
        midi.data2!=selectedZone.selected_velocity)
        return std::nullopt;
    CandidateVoiceParameters parameters{unresolved.combined_volume_index,
        unresolved.sample_attenuation,selectedZone.chip_cutoff_candidate,
        unresolved.velocity_cutoff_enabled};
    return candidateInitialIfatnWrites(midi,physicalVoice,parameters);
}
} // namespace awe64::creative
