#pragma once
// Execute source-authentic ORIGINAL SBAWE.VXD ROM-mono NoteOn on the pinned,
// unmodified turican0 86Box EMU8000 chip. NO DSP patch and no generic
// SoundFont engine, ROM scheduler, or unsupported RAM/stereo path.
//
// ALL source-derived OUTs are prepared and checked before the first chip
// write. Original first "pointer-only" E22 command for each voice cannot
// be represented by the prior E22/data pair bridge; preserve it exactly.
#include "CreativeSf2FullNoteOnPorts.h"
#include "CreativeChipPortBridge.h"
#include <span>
namespace awe64::creative {
[[nodiscard]] bool applyOriginalCreativeSf2FullNoteOnPorts(
    awe64::Chip& chip,
    std::span<const OriginalSf2FullNoteOnVoice> source_verified_voices) noexcept;
} // namespace awe64::creative
