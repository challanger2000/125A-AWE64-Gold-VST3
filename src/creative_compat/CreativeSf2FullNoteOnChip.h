#pragma once
// Execute source-authentic ORIGINAL SBAWE.VXD ROM-mono NoteOn on the pinned,
// unmodified turican0 86Box EMU8000 chip. NO DSP patch and no generic
// SoundFont engine, ROM scheduler, or unsupported RAM/stereo path.
//
// ALL source-derived OUTs are prepared and checked before the first chip
// write. Original first "pointer-only" E22 command for each voice cannot
// be represented by the prior E22/data pair bridge; preserve it exactly.
#include "CreativeSf2FullNoteOnPorts.h"
#include "CreativeVxdMonoVoiceRanking.h"
#include "CreativeChipPortBridge.h"
#include <span>
namespace awe64::creative {
[[nodiscard]] bool applyOriginalCreativeSf2FullNoteOnPorts(
    awe64::Chip& chip,
    std::span<const OriginalSf2FullNoteOnVoice> source_verified_voices) noexcept;
// Source-verified ORIGINAL VXD stop/reset immediately after VTFT
// selector read. The initial E22 VTFT selector has ALREADY been emitted
// by the live allocator; do not emit it twice.
[[nodiscard]] bool applyOriginalCreativeSf2StopSelectedIdleVoice(
    awe64::Chip& chip,
    const OriginalWin95MonoSelection& source_selection) noexcept;

// When the original live allocator interleaved all source stop/reset
// prologues, execute ONLY the remaining source-derived 56 main commands
// per voice; preflight all 61 typed source writes again before issuing.
[[nodiscard]] bool applyOriginalCreativeSf2MainPortsAfterStop(
    awe64::Chip& chip,
    std::span<const OriginalSf2FullNoteOnVoice> verified_voices) noexcept;
} // namespace awe64::creative
