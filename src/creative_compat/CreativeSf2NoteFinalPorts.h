#pragma once
// P002 ROM-MONO FORMAT-2 SF2: the 22 remaining CONTIGUOUS typed OUTs in
// original Win95 Creative SBAWE.VXD LCOD+0x5EEB..+0x6043, immediately after
// the verified 18 + 16 OUT note-start stages.
//
// Output is 22 typed port writes: PSST, CSL, CCCA, Z1, Z2, CCCA+Q,
// VTFT, CVCF, PTRX, CPF, DCYSUSV (envelope trigger).
//
// Two target operands come from ORIGINAL device/voice allocation state:
//   VXD stack local -0x18: computed volume-envelope VTFT upper 16 bits
//   VXD stack local -0x1c: computed pointer pitch target, PTRX/CPF high 16
// These are not contained in the SoundFont voice descriptor. Require
// independently VERIFIED original values; never infer by MIDI program.
//
// No original Creative ROM/driver binary bytes are distributed here.
#include "CreativeSf2NoteContinuationPorts.h"
#include "CreativeVxdSampleAddress.h"
#include <array>
#include <cstdint>
#include <optional>
namespace awe64::creative {
inline constexpr std::size_t kOriginalSf2FinalNoteStartPortCount=22u;
struct OriginalSf2VerifiedFinalTargetState {
    bool original_driver_state_verified=false;
    std::uint16_t original_voice_volume_target=0; // stack -0x18
    std::uint16_t original_pitch_target=0;        // stack -0x1c
};
[[nodiscard]] std::optional<
    std::array<OriginalWin95VxdPortWrite,kOriginalSf2FinalNoteStartPortCount>>
candidateOriginalSf2FinalNoteStartPorts(
    const CreativeSf2VoiceDescriptorBytes& post_start_record,
    std::uint8_t independently_assigned_physical_voice,
    const OriginalSf2VerifiedFinalTargetState& original_driver_state) noexcept;
} // namespace awe64::creative
