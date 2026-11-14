#pragma once
// BOUNDED P002 adapter from actual Creative Win95 VXD port writes to the
// pinned, UNMODIFIED original turican0/86Box EMU8000 core.
// This permits register-level actual-chip tests, not just candidate arrays.
// No original driver bytes or runtime trace data are included.
#include "CreativeVxdVoiceTermination.h"
#include "Chip.h"
#include <span>

namespace awe64::creative {
// Bounded VXD-verified voice operations only. The original SBAWE.VXD
// SF2 NOTE START registers now include all six DATA3 (E20) voice words
// IP/IFATN/PEFE/FMMOD/TREMFRQ/FM2FRQ2, all four DATA1 (A20) envelope
// words ENVVOL/DCYSUSV/ENVVAL/DCYSUS, all eight source-backed DATA0
// (620) DWORDs CPF/PTRX/CVCF/VTFT/Z2/Z1/PSST/CSL, and DATA1 (A20)
// CCCA. Pointer E22 is original OUT AL; 16-bit data uses OUT AX,
// 32-bit data uses OUT EAX with low/high halfwords on the pinned host.
// Unsupported port/selector combinations and widths FAIL CLOSED.
// Reject the entire malformed sequence before issuing ANY chip I/O.
// The fork's word-only Host adapter intentionally mirrors its emu8k_outb
// low-byte behavior for E22; 32-bit VTFT is written as consecutive
// low/high word registers 620/622, as expected by the original fork.
// This function is not safe concurrently with Chip::render().
// Pure preflight permits validating the *entire* multi-voice NoteOn
// (including all its later 56-OUT segments) BEFORE the first I/O write.
[[nodiscard]] bool validateOriginalCreativeVxdVoicePorts(
    std::span<const OriginalWin95VxdPortWrite> commands) noexcept;
[[nodiscard]] bool applyOriginalCreativeVxdVoicePorts(
    awe64::Chip& chip,
    std::span<const OriginalWin95VxdPortWrite> commands) noexcept;
} // namespace awe64::creative
