#pragma once
// Original Windows 95 SBAWE.VXD LCOD +0x5B4D/+0x5D7B..+0x5E5D:
// compute VXD stack-local -0x1c PTRX/CPF pitch target AND -0x18
// special attack128 VTFT volume target from source-verified driver state.
// No formula from a modern SoundFont spec is substituted here.
// The original 16-WORD LCOD+0x2FA0 attenuation lookup is NEVER embedded.
#include "CreativeSf2FullNoteOnPorts.h"
#include "CreativeSf2NoteFinalPorts.h"
#include <array>
#include <cstdint>
#include <optional>

namespace awe64::creative {
struct OriginalSf2VxdFinalTargetInputs {
    bool source_original_device_voice_state_verified=false;
    bool original_lcod_2fa0_16word_table_verified=false;
    // Direct source references: [channel+0x0E] signed WORD,
    // [allocated_voice+0x0E] unsigned WORD, [channel+0x14] DWORD.
    // These are independent original VXD state values; do not infer them
    // from a program number or fabricate a default controller state.
    std::int16_t channel_word_plus_0e=0;
    std::uint16_t allocated_voice_word_plus_0e=0;
    std::uint32_t channel_dword_plus_14=0;
    std::array<std::uint16_t,16> source_lcod_2fa0_table{};
    // The signed nibble lookup for NEGATIVE gen19 indexes 1..15 WORDs
    // BEFORE +0x2FA0: actual source x86 re-applies the negative sign
    // after abs(gen19)&15. This is contiguous original LCOD+0x2F82.
    bool original_lcod_2f82_negative_table_verified=false;
    std::array<std::uint16_t,15> source_lcod_2f82_negative_table{};
};
[[nodiscard]] std::optional<OriginalSf2VerifiedFinalTargetState>
candidateOriginalSf2ComputeFinalTargetsFromVxdState(
    const CreativeSf2VoiceDescriptorBytes& post_start_record,
    const OriginalSf2VxdFinalTargetInputs& verified_source) noexcept;
// Fully source-backed 61/122 NoteOn preparation with both former
// final target words DERIVED from original VXD source operands. Still
// requires a separately verified physical-voice allocator and device
// state; does not guess these from MIDI program/bank or synthesize PCM.
struct OriginalSf2AssignedVoiceAndVxdState {
    OriginalSf2FullNoteOnVoice original_assigned_voice{};
    OriginalSf2VxdFinalTargetInputs original_device_voice_state{};
};
[[nodiscard]] std::optional<OriginalSf2FullNoteOnPlan>
candidateOriginalSf2FullNoteOnWithComputedOriginalTargets(
    std::span<const OriginalSf2AssignedVoiceAndVxdState> voices) noexcept;
} // namespace awe64::creative
