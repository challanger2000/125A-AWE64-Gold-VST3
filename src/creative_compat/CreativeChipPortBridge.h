#pragma once
// BOUNDED P002 adapter from actual Creative Win95 VXD port writes to the
// pinned, UNMODIFIED original turican0/86Box EMU8000 core.
// This permits register-level actual-chip tests, not just candidate arrays.
// No original driver bytes or runtime trace data are included.
#include "CreativeVxdVoiceTermination.h"
#include "Chip.h"
#include <span>

namespace awe64::creative {
// Bounded VXD-verified voice-reset/VTFT/forced-group-stop operations and
// independently measured original MIDI-driven IP/IFATN word writes:
// pointer E22 OUT AL 8 bits, data A20/E20 OUT AX 16 bits,
// and data 620 OUT EAX 32 bits. Only the actual documented
// DATA3 register 0 (IP), DATA3 register 1 (IFATN), DATA1 register 5
// (DCYSUSV), DATA1 register7 (DCYSUS release), and DATA0
// registers 3 (VTFT), 6 (PSST), 7 (CSL)
 // are accepted after original-driver verification.
 // All other writes FAIL CLOSED.
// Reject the entire malformed sequence before issuing ANY chip I/O.
// The fork's word-only Host adapter intentionally mirrors its emu8k_outb
// low-byte behavior for E22; 32-bit VTFT is written as consecutive
// low/high word registers 620/622, as expected by the original fork.
// This function is not safe concurrently with Chip::render().
[[nodiscard]] bool applyOriginalCreativeVxdVoicePorts(
    awe64::Chip& chip,
    std::span<const OriginalWin95VxdPortWrite> commands) noexcept;
} // namespace awe64::creative
