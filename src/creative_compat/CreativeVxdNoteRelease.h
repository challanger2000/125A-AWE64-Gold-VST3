#pragma once
// P002: Original SBAWE.VXD per-voice NOTE RELEASE (LCOD +0x6072..+0x6185).
// This is distinct from its group-choke routine (+0x4AB5).
// Inputs are genuine VXD per-voice state and prior chip registers supplied
// by an informed original driver-state owner, not guessed from MIDI alone.
#include "CreativeVxdMonoVoiceRanking.h"
#include <array>
#include <cstdint>
#include <optional>

namespace awe64::creative {
struct OriginalVxdNoteReleaseEvidence {
    bool original_key_channel_match=false;
    bool original_sustain_held=false; // VXD [voice-note record +0x04] nonzero
    // Already encoded release-rate BYTES from original driver 32-byte
    // physical voice record (+0x16 vol, +0x15 mod). Do not substitute
    // unsolved raw SoundFont generator values.
    std::uint8_t encoded_volume_release=0;
    std::uint8_t encoded_mod_release=0;
    // Original per-voice DWORD +0x06: sample END used for special release
    // loop rewrite, zero => NO CSL/PSST writes.
    std::uint32_t stored_end_word=0;
    std::uint32_t observed_chip_csl=0;
    std::uint32_t observed_chip_psst=0;
    // A VXD storage-record snapshot and chip register reads are required.
    bool original_voice_record_verified=false;
    bool current_chip_registers_verified=false;
};
struct OriginalVxdNoteReleaseCommands {
    // True -> original guest marks the stored note identity low byte 0xFF.
    // It does not write chip registers until actual sustain release.
    bool deferred_by_sustain=false;
    // True -> original VXD resets physical voice's internal key marker
    // to 0x0FFE on releasing it. Native callers must mirror separately.
    bool original_voice_marker_idle=false;
    std::array<OriginalWin95VxdPortWrite,8> writes{};
    std::uint8_t count=0; // 0 deferred, 4 normal, 8 with end-loop rewrite
};
// The original VXD +0x60E4 emits:
// DCYSUSV=0x8000|voice[0x16], DCYSUS=0x8000|voice[0x15].
// If voice's stored END !=0 it then writes CSL=preservedHigh|END+4
// and PSST=preservedHigh|END, also writes internal stored END to +0x0A.
// It does NOT write IFATN=0xFF (that's forced group choke).
[[nodiscard]] std::optional<OriginalVxdNoteReleaseCommands>
candidateOriginalVxdMidiNoteRelease(
    std::uint8_t physical_voice,
    const OriginalVxdNoteReleaseEvidence& evidence) noexcept;
} // namespace awe64::creative
