#include "CreativeSf2OriginalTargets.h"
#include "OriginalCreativeSf2TargetMath.hpp"
#include <array>
#include <bit>
#include <cstdint>

namespace awe64::creative {
namespace {
std::uint16_t word(const CreativeSf2VoiceDescriptorBytes& b,
                   const unsigned id) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(b[2u*id]) |
        (static_cast<unsigned>(b[2u*id+1u])<<8u));
}
std::int32_t signedWord(const CreativeSf2VoiceDescriptorBytes& b,
                        const unsigned id) noexcept {
    const auto w=word(b,id);
    return w<=0x7fffu?static_cast<std::int32_t>(w):
                      static_cast<std::int32_t>(w)-65536;
}

}
std::optional<OriginalSf2VerifiedFinalTargetState>
candidateOriginalSf2ComputeFinalTargetsFromVxdState(
    const CreativeSf2VoiceDescriptorBytes& note,
    const OriginalSf2VxdFinalTargetInputs& source) noexcept {
    if(!source.source_original_device_voice_state_verified ||
       !source.original_lcod_2fa0_16word_table_verified ||
       word(note,0x8cu/2u)!=0x0001u ||
       word(note,34u)>128u)
        return std::nullopt;

    const auto targets=awe64::original_vxd::detail::note_pitch_targets(
        source.allocated_voice_word_plus_0e,source.channel_word_plus_0e,
        std::bit_cast<std::int32_t>(source.channel_dword_plus_14),
        static_cast<std::int16_t>(signedWord(note,7u)));
    std::uint16_t volume=0u;
    if(word(note,34u)==128u && word(note,33u)>=0x8000u) {
        const auto negative=source.original_lcod_2f82_negative_table_verified?
            std::span<const std::uint16_t>(source.source_lcod_2f82_negative_table):
            std::span<const std::uint16_t>{};
        const auto result=awe64::original_vxd::detail::attack_volume_target(
            static_cast<std::int16_t>(signedWord(note,19u)),
            source.source_lcod_2fa0_table,negative);
        if(!result)return std::nullopt;
        volume=*result;
    }
    return OriginalSf2VerifiedFinalTargetState{true,volume,targets.pitch_target};
}
std::optional<OriginalSf2FullNoteOnPlan>
candidateOriginalSf2FullNoteOnWithComputedOriginalTargets(
    const std::span<const OriginalSf2AssignedVoiceAndVxdState> voices) noexcept {
    if(voices.empty() ||
       voices.size()>kOriginalCreativeMaxNoteStartDescriptors)
        return std::nullopt;
    std::array<OriginalSf2FullNoteOnVoice,
               kOriginalCreativeMaxNoteStartDescriptors> resolved{};
    for(std::size_t i=0;i<voices.size();++i) {
        const auto target=candidateOriginalSf2ComputeFinalTargetsFromVxdState(
            voices[i].original_assigned_voice.verified_after_start_record,
            voices[i].original_device_voice_state);
        if(!target)return std::nullopt;
        resolved[i]=voices[i].original_assigned_voice;
        resolved[i].target=*target; // identical existing 56-OUT composer
    }
    return candidateOriginalSf2FullNoteOnPorts(
        std::span<const OriginalSf2FullNoteOnVoice>(resolved.data(),voices.size()));
}
} // namespace awe64::creative
