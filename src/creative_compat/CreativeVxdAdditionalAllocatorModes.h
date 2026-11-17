#pragma once
// P003: research-only, original SBAWE.VXD LCOD +0x3FE8 mode-1
// adjacent-voice pair ranking and +0x416C mode-2 bounded range search.
//
// This module is deliberately NOT wired into production VST3 MIDI:
// original device-state values, live chip reads, selected descriptor mode,
// the original release side effects and x86 CPU pacing are still required.
//
// CPU/EMU8000 measurements are not substituted by artificial state.
// The caller must provide one complete ORIGINAL driver+EMU8000 snapshot
// per voice actually traversed. "Unknown" fails closed.
#include "CreativeVxdMonoVoiceRanking.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace awe64::creative {
inline constexpr std::size_t kOriginalWin95PhysicalVoiceCapacity=32u;
inline constexpr std::uint8_t kOriginalWin95NoVoice=0xffu;

struct OriginalWin95AllocatorVoiceSnapshot {
    bool complete=false;
    std::uint16_t internal_key_marker=0x0ffeu;
    std::uint16_t volume_target_hi16=0u;
    std::uint16_t envelope_status_reg=0x8000u;
    std::uint32_t stored_sample_end=0u;
    std::uint32_t chip_current_address=0u;
    // Provenance of a real CCCA-only read. Distinct from 'complete':
    // original Mode-2 high-marker path never reads VTFT/DCYSUSV.
    // A nonzero numeric chip_current_address by itself is NOT proof.
    bool ccca_read_verified=false;
    // Independent original LCOD+41E0 VTFT and conditional +4228 DCYSUSV
    // provenance. An observed value alone is never a verified chip read.
    bool vtft_read_verified=false;
    bool dcysusv_read_verified=false;
    bool blocked_when_few_voices=false;
    // Original VXD [+0x4C + voice*0x20 +0x10], bit zero.
    // The meaning of this descriptor/driver bit is NOT asserted.
    bool pair_extra_penalty=false;
};

struct OriginalWin95PairSelection {
    // Physical voices first and first+1; original mode 1 releases BOTH.
    std::uint8_t first_voice=kOriginalWin95NoVoice;
    std::uint8_t second_voice=kOriginalWin95NoVoice;
    std::uint32_t priority=0u;
    bool immediate_zero_pair=false;
};

// LCOD +0x3FE8 original reverse scan; adjacent score
//   A+B+2*max(A,B)+((current_voice_state_flag&1)?0x0C00:0).
// Performs the original individual score weighting and optional
// sample-end early score=0, but not the associated hardware reads.
// Ties favor the later *visited* pair, i.e. lower first_voice.
[[nodiscard]] std::optional<OriginalWin95PairSelection>
candidateOriginalWin95Mode1Pair(
    const std::array<OriginalWin95AllocatorVoiceSnapshot,
                     kOriginalWin95PhysicalVoiceCapacity>& voices,
    std::uint16_t verified_device_capacity,
    std::uint32_t original_driver_mode_count) noexcept;

// LCOD +0x416C: packed lower byte=start, remaining bits=exclusive upper,
// clamped to original device capacity; evaluates ORIGINAL individual
// scoring, fully idle voice and sample-end early exits.
// No read, return or side effect is synthesized when required inputs
// are incomplete.
[[nodiscard]] std::optional<OriginalWin95MonoSelection>
candidateOriginalWin95Mode2Range(
    const std::array<OriginalWin95AllocatorVoiceSnapshot,
                     kOriginalWin95PhysicalVoiceCapacity>& voices,
    std::uint32_t packed_range,
    std::uint16_t verified_device_capacity,
    std::uint32_t original_driver_mode_count) noexcept;

// Mode1 original two calls to writer LCOD+0x3CB1 at +0x414A/+0x4158:
// E22 index byte (0xA0|voice), A20 16-bit value 0x00FF,
// first for chosen voice, then for adjacent voice+1.
// This is only the observed release pair, NOT complete NoteOn.
[[nodiscard]] std::optional<std::array<OriginalWin95VxdPortWrite,4>>
candidateOriginalWin95Mode1PairReleaseWrites(
    const OriginalWin95PairSelection& selection) noexcept;

// Mode2 original writer at +0x428E, one selected voice.
[[nodiscard]] std::optional<std::array<OriginalWin95VxdPortWrite,2>>
candidateOriginalWin95Mode2ReleaseWrites(
    const OriginalWin95MonoSelection& selection) noexcept;

// Research-only entry point for all three original x86 dispatcher modes
// (LCOD+0x3D6F). This is an explicit ORIGINAL STATE interface, not a
// heuristic that infers stereo from a patch or MIDI key.
// Distinguishes legitimately exhausted selection (return 0xFF) from
// missing/unauthenticated driver or live-chip state (nullopt).
struct OriginalWin95AllocatorDispatchInput {
    bool independently_verified_source_state=false;
    std::uint8_t original_x86_dispatch_mode=0xffu; // 0, 1, 2
    std::uint16_t original_device_physical_capacity=0u; // device+0x06
    std::uint32_t original_device_mode_count=0u; // device+0x18
    // Independent LCOD+3ED4 source bound: must match device+0x06.\n    // Original Win95 selects physical voices 30/31 in mode 0.\n    std::uint8_t independently_verified_mode0_scan_ceiling=0u;
    std::uint32_t original_mode2_packed_range=0u;
};
struct OriginalWin95AllocatorDispatchResult {
    std::uint8_t original_x86_dispatch_mode=0xffu;
    std::array<std::uint8_t,2> physical_voices{
        kOriginalWin95NoVoice,kOriginalWin95NoVoice};
    std::uint8_t voice_count=0u; // 0 means original 0xFF no candidate
    std::uint32_t priority=0u;
    bool immediate_idle_pair=false;
};
[[nodiscard]] std::optional<OriginalWin95AllocatorDispatchResult>
candidateOriginalWin95VxdDispatchAllModes(
    const std::array<OriginalWin95AllocatorVoiceSnapshot,
                     kOriginalWin95PhysicalVoiceCapacity>& verified_voices,
    const OriginalWin95AllocatorDispatchInput& verified_input) noexcept;

} // namespace awe64::creative
