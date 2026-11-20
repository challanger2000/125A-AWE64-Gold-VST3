#pragma once
// Source-reconstructed complete ordered original SBAWE.VXD ROM-MONO SF2
// NOTE ON WRITE PLAN, *not* the full timing/read/effects/mixing engine.
//
// Original Win95 captured x86 format-2 NOTE START per MIDI note:
//   first 5 allocation/stop-reset OUTs for EACH allocated voice;
//   then 18+16+22 OUTs for EACH voice in bank's selected layer order.
// Thus an original 1-voice note has 61 OUTs and a 2-voice note 122 OUTs.
// Up to six descriptors are supported by the original bank-start builder.
//
// The two private original device-state target values are explicit inputs.
// This API does not manufacture a scheduler, PCM output, or RAM soundfont.
#include "CreativeSf2NoteFinalPorts.h"
#include "CreativeMidiRomPlan.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace awe64::creative {
inline constexpr std::size_t kOriginalSf2OneVoiceStartWrites=61u;
inline constexpr std::size_t kOriginalSf2OneVoiceMainWrites=56u;
inline constexpr std::size_t kOriginalSf2MaxStartWrites=
    kOriginalCreativeMaxNoteStartDescriptors*kOriginalSf2OneVoiceStartWrites;
struct OriginalSf2FullNoteOnVoice {
    CreativeSf2VoiceDescriptorBytes verified_after_start_record{};
    std::uint8_t played_midi_key=0;
    std::uint8_t physical_voice=0;
    bool source_original_voice_verified=false;
    std::int16_t verified_channel_tune_1_cents=0;
    std::int16_t verified_channel_tune_2_cents=0;
    OriginalSf2VerifiedLfoChannelAmounts lfo{};
    OriginalSf2VerifiedFinalTargetState target{};
    // Distinguish authenticated physical Creative RAM sample geometry
    // from the unchanged ROM-only VXD note-start address mode.
    bool original_ram_sample_verified=false;
};
struct OriginalSf2FullNoteOnPlan {
    std::array<OriginalWin95VxdPortWrite,kOriginalSf2MaxStartWrites> out{};
    std::size_t count=0;
    std::uint8_t voices=0;
};
[[nodiscard]] std::optional<OriginalSf2FullNoteOnPlan>
candidateOriginalSf2FullNoteOnPorts(
    std::span<const OriginalSf2FullNoteOnVoice> original_assigned_voices) noexcept;
} // namespace awe64::creative
