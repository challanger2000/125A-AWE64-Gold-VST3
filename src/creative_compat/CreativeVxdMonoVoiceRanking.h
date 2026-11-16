#pragma once
// Original Win95 Creative SBAWE.VXD mode-0 MONO voice priority evaluator.
// *Caller* must supply the original driver's per-slot bookkeeping and chip
// register reads. Without those the returned result is NOT driver-equivalent.
// Selection is a bounded candidate; original driver release/steal is NOT
// inferred. Its immediately following word-write to DCYSUSV is independently
// extracted from actual original VXD and observed in the Win95 trace.
#include "CreativeIdleVoiceAllocator.h"
#include "CreativeCandidateRegister.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
namespace awe64::creative {
struct OriginalWin95MonoVoiceSnapshot {
    bool complete=false; // false = missing driver/hardware state: fail closed
    bool blocked_when_few_voices=false; // original per-slot gate at low active count
    std::uint16_t internal_key_marker=0x0ffe;
    std::uint16_t volume_target_hi16=0; // actual EMU8000 register read
    std::uint16_t envelope_status_reg=0x8000; // VXD bit-15 test
    std::uint32_t stored_sample_end=0; // driver's per-voice bookkeeping
    std::uint32_t chip_current_address=0; // EMU8000 register read, 24-bit
};
enum class OriginalWin95MonoSelectionReason : std::uint8_t {
    CompletelyIdle, SampleEndReached, LowestEnvelopePriority
};
struct OriginalWin95MonoSelection {
    std::uint8_t physical_voice=0;
    OriginalWin95MonoSelectionReason reason=
        OriginalWin95MonoSelectionReason::CompletelyIdle;
    std::uint32_t priority=0;
};
// Reconstructs ONLY the deterministic selection decision from original
// SBAWE.VXD LCOD +0x3EA1..+0x3FE8, not the register I/O side effects.
// Unlike the idle-only helper, this can nominate an occupied voice, so a
// caller must implement original stop/release semantics before using it.
[[nodiscard]] std::optional<OriginalWin95MonoSelection> candidateOriginalWin95Mode0MonoVoice(
    const std::array<OriginalWin95MonoVoiceSnapshot,kOriginalCreativeMusicVoices>& snapshots,
    std::size_t available_music_voices,
    std::uint32_t original_driver_mode_count) noexcept;
struct OriginalWin95VxdPortWrite {
    std::uint16_t port=0;
    std::uint32_t value=0; // exact 8-, 16-, or 32-bit I/O data word
    std::uint8_t access_width_bits=0;
};
// On *both* the completely-idle and selected-busy VXD paths, the actual
// code invokes its 16-bit hardware writer with (5 << 5)|voice and value 0x00FF.
// The original VXD emits an 8-bit OUT AL to E22 followed by a 16-bit
// OUT AX to A20; port writes must preserve this access width when replaying.
// This selects original EMU8000 DCYSUSV (DATA1) via E22/A20; it is not a
// replacement for the complete voice start/stop and envelope sequence.
// Caller at original VXD +0x3DD5, after the selector has returned,
 // writes VTFT=0x0000FFFF to the newly assigned physical voice,
 // via byte pointer select and DWORD DATA0 register output.
 // The stereo extra-voice and conditionally paired release paths are
 // NOT part of this mode-0 mono helper.
[[nodiscard]] std::optional<std::array<OriginalWin95VxdPortWrite,2>>
candidateOriginalWin95AssignedVoiceVtftWrites(
    std::uint8_t physical_voice) noexcept;
[[nodiscard]] std::optional<std::array<OriginalWin95VxdPortWrite,2>>
candidateOriginalWin95SelectedVoiceDcysusvWrites(
    const OriginalWin95MonoSelection& selected) noexcept;
} // namespace awe64::creative
