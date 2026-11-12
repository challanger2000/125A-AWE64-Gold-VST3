#pragma once
// P002: ORIGINAL SBAWE.VXD NOTE-START contiguous register prefix, built
// from the already transformed 148-byte SF2 voice, not from a recorded
// driver event replay or a per-preset table.
//
// The genuine Win95 Creative trace has 14 typed OUTs for VTFT/CVCF and
// 5 envelope/LFO registers, followed immediately by two DATA3/IP OUTs
// and two DATA3/IFATN OUTs. Total = 18 typed port operations.
// Other NoteOn operations (CCCA, PSST/CSL, FX, oscillator state, timing,
// voice scheduling) are NOT supplied by this bounded prefix.
//
// Caller must supply the independently selected physical chip voice and
// already verified original note-start descriptor. No DSP code is changed.
#include "CreativeSf2VoiceDescriptor.h"
#include "CreativeVxdVoiceEnvelopePrelude.h"
#include <array>
#include <cstdint>
#include <optional>
namespace awe64::creative {
inline constexpr std::size_t kOriginalSf2NoteStartPrefixPortCount=18u;
// Original x86 SBAWE.VXD +0x5BE4..(IP, IFATN): exact contiguous 18 OUTs.
// Requires original ROM-mono qualifier to be already cleared to 0x0001;
// refuses raw 0x8001 and any unverified / unsupported descriptor encoding.
[[nodiscard]] std::optional<
    std::array<OriginalWin95VxdPortWrite,kOriginalSf2NoteStartPrefixPortCount>>
candidateOriginalSf2NoteStartRegisterPrefix(
    const CreativeSf2VoiceDescriptorBytes& original_encoded_note,
    std::uint8_t midi_note_key,
    std::uint8_t independently_assigned_physical_voice,
    bool original_note_descriptor_verified,
    std::int16_t verified_channel_tune_1_cents,
    std::int16_t verified_channel_tune_2_cents) noexcept;
} // namespace awe64::creative
