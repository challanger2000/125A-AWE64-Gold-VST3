#include "CreativeSf2FullNoteOnPorts.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace awe64::creative {
std::optional<OriginalSf2FullNoteOnPlan>
candidateOriginalSf2FullNoteOnPorts(
    const std::span<const OriginalSf2FullNoteOnVoice> voices) noexcept {
    if(voices.empty() || voices.size()>kOriginalCreativeMaxNoteStartDescriptors)
        return std::nullopt;
    // Validate and compose ALL voices/segments before returning ANY output.
    // The caller can execute the completed list transactionally as one
    // ordered original VXD write plan. No heap allocation or I/O here.
    OriginalSf2FullNoteOnPlan plan{};
    plan.voices=static_cast<std::uint8_t>(voices.size());
    std::uint32_t assigned=0;
    for(const auto& v:voices) {
        const auto physical=v.physical_voice;
        if(!v.source_original_voice_verified ||
           physical>=kOriginalCreativeMusicVoices ||
           (assigned&(1u<<physical))!=0u ||
           v.played_midi_key>127u)
            return std::nullopt;
        assigned|=1u<<physical;
        // Original LCOD per-note allocation stop/voice-reset x86 OUTs:
        // pointer REG3, pointer REG5, DATA1 REG5=0xFF, pointer REG3,
        // DATA0 REG3=0x0000FFFF. Those first and fourth E22 OUTs
        // include a redundant pointer, not a data OUT. Preserve them.
        const std::uint32_t v3=0x60u|physical;
        const std::uint32_t v5=0xa0u|physical;
        const std::array<OriginalWin95VxdPortWrite,5> prologue{{
            {0xe22u,v3,8u},
            {0xe22u,v5,8u},
            {0xa20u,0xffu,16u},
            {0xe22u,v3,8u},
            {0x620u,0xffffu,32u}
        }};
        std::copy(prologue.begin(),prologue.end(),
                  plan.out.begin()+static_cast<std::ptrdiff_t>(plan.count));
        plan.count+=prologue.size();
    }
    for(const auto& v:voices) {
        const auto first=candidateOriginalSf2NoteStartRegisterPrefix(
            v.verified_after_start_record,v.played_midi_key,
            v.physical_voice,v.source_original_voice_verified,
            v.verified_channel_tune_1_cents,
            v.verified_channel_tune_2_cents);
        const auto middle=candidateOriginalSf2AfterPitchRegisters(
            v.verified_after_start_record,v.physical_voice,v.lfo);
        const auto last=candidateOriginalSf2FinalNoteStartPorts(
            v.verified_after_start_record,v.physical_voice,v.target);
        if(!first || !middle || !last)return std::nullopt;
        std::copy(first->begin(),first->end(),
                  plan.out.begin()+static_cast<std::ptrdiff_t>(plan.count));
        plan.count+=first->size();
        std::copy(middle->begin(),middle->end(),
                  plan.out.begin()+static_cast<std::ptrdiff_t>(plan.count));
        plan.count+=middle->size();
        std::copy(last->begin(),last->end(),
                  plan.out.begin()+static_cast<std::ptrdiff_t>(plan.count));
        plan.count+=last->size();
    }
    if(plan.count!=voices.size()*kOriginalSf2OneVoiceStartWrites)
        return std::nullopt;
    return plan;
}
} // namespace awe64::creative
