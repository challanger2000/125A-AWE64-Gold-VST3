#pragma once
// P002: original Win95 SBAWE.VXD LCOD+0x3EA1..0x3FE8 physical
// EMU8000 voice selector against the pinned 86Box original chip READS.
//
// NO synthetic source scoring. Reuses the independently x86-verified
// original mono-priority selector, with all 30 physical voices read from
// original driver 0x20-byte voice state and genuine chip VTFT/CCCA/
// envelope registers. Does not fabricate hardware status, MIDI identity,
// call sample-stop side effects, or steal a playing note on its own.
//
// This supplies ONLY original selected voice/reason, not an implicit
// completed NoteOn or sample-exact Win95 read timing. Source state must
// be a caller-verified original VXD device snapshot; 30 voice records
// expected, two EMU8000 chip lanes 30 and 31 always reserved.
#include "CreativeSf2VxdStateSnapshot.h"
#include "CreativeVxdMonoVoiceRanking.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace awe64::creative {
[[nodiscard]] std::optional<OriginalWin95MonoSelection>
candidateOriginalSf2LiveVxdMonoVoice(
    awe64::Chip& chip,
    std::span<const std::uint8_t> verified_vxd_state,
    bool independently_verified_original_driver_state) noexcept;
} // namespace awe64::creative
