#pragma once
// P002 exact next original Win95 SBAWE.VXD FORMAT-2 NOTE START segment
// immediately after the already verified 18 typed register OUTs.
//
// Original LCOD +0x5CCF..+0x5EE6. Source x86 generator offsets:
// PEFE g7/g11, FMMOD g5+two real device-state bytes/g10,
// TREMFRQ g13/g22, FM2FRQ2 g6/g24, ENVVAL g25 (attack g26 exception),
// ENVVOL g33 (attack g34 exception), then PTRX and CPF reset DWORDs.
//
// These are 16 additional typed OUTs, contiguous, ordered, not trace
// replay. Other sample cursor/FX/trigger/timing fields are NOT covered.
// The x86 original is separately provided by the caller; the original
// bytes/tables must never be embedded in this public source.
#include "CreativeSf2NoteStartPorts.h"
#include <array>
#include <cstdint>
#include <optional>
namespace awe64::creative {
inline constexpr std::size_t kOriginalSf2AfterPitchPortCount=16u;
struct OriginalSf2VerifiedLfoChannelAmounts {
    bool source_original_state_verified=false;
    std::uint8_t lfo1_pitch_device_0a=0;
    std::uint8_t lfo1_pitch_device_0b=0;
};
[[nodiscard]] std::optional<
    std::array<OriginalWin95VxdPortWrite,kOriginalSf2AfterPitchPortCount>>
candidateOriginalSf2AfterPitchRegisters(
    const CreativeSf2VoiceDescriptorBytes& source_encoded_voice,
    std::uint8_t independently_assigned_physical_voice,
    const OriginalSf2VerifiedLfoChannelAmounts& verified_channel) noexcept;
} // namespace awe64::creative
