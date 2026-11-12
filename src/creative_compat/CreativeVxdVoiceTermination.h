#pragma once
// P002: original installed Win95 SBAWE.VXD forced group-voice termination.
// Not general MIDI NoteOff, and NOT a shortcut to a fully correct CC120.
#include "CreativeVxdMonoVoiceRanking.h"
#include <array>
#include <cstdint>
#include <optional>

namespace awe64::creative {
struct OriginalVxdGroupChokeEvidence {
    // The actual VXD compares the already resolved note group against
    // its per-channel original-group byte and original voice identity.
    bool matched_original_group=false;
    // VXD +0x4B32 & 0x4B38: AND of two separate per-voice stereo flags.
    // Neither signal may be inferred from MIDI key, bank or sample width.
    bool lead_voice_stereo_flag=false;
    bool following_voice_paired_flag=false;
};
struct OriginalVxdGroupChokeWrites {
    std::array<OriginalWin95VxdPortWrite,8> writes{};
    std::uint8_t count=0; // 4 mono port writes or 8 explicit stereo-pair writes
};
// The real VXD +0x4B0C/+0x4B1F writes IFATN=0x00FF (DATA3) then
// DCYSUSV=0x807F (DATA1), and clears its group bookkeeping. The exact
// stereo branch repeats the two 16-bit voice writes for physical voice+1.
// All port widths are from actual original x86 OUT AL/AX instructions.
// Caller MUST independently establish original group-match predicates.
// This function does not own channel state, schedule sample clocks,
// update VXD bookkeeping or simulate unrelated NoteOff/sustain messages.
[[nodiscard]] std::optional<OriginalVxdGroupChokeWrites>
candidateOriginalWin95VxdGroupChokeWrites(
    std::uint8_t first_physical_voice,
    OriginalVxdGroupChokeEvidence verified) noexcept;
} // namespace awe64::creative
