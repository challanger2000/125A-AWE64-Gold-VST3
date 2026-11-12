#pragma once
// Original Windows 95 SBAWE.VXD LCOD +0x5B4D/+0x5D7B..+0x5E5D:
// compute VXD stack-local -0x1c PTRX/CPF pitch target AND -0x18
// special attack128 VTFT volume target from source-verified driver state.
// No formula from a modern SoundFont spec is substituted here.
// The original 16-WORD LCOD+0x2FA0 attenuation lookup is NEVER embedded.
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
};
[[nodiscard]] std::optional<OriginalSf2VerifiedFinalTargetState>
candidateOriginalSf2ComputeFinalTargetsFromVxdState(
    const CreativeSf2VoiceDescriptorBytes& post_start_record,
    const OriginalSf2VxdFinalTargetInputs& verified_source) noexcept;
} // namespace awe64::creative
