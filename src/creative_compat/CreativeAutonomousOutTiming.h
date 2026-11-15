#pragma once
// P003 PRIVATE RESEARCH ONLY. The source-independent CPU-cycle pacing model
// is not a proven replacement for Creative's Win95 virtual-device driver.
#include "CreativeSampleClockPorts.h"
#include <cstddef>
#include <cstdint>
#include <span>
namespace awe64::creative {
struct P003AutonomousTimingStats {
    std::size_t writes=0u;
    std::size_t initial_voice_prelude_writes=0u;
    std::uint32_t final_relative_frame=0u;
    std::uint64_t estimated_guest_cpu_cycles=0u;
    std::uint32_t assumed_intrasample_phase_cycles=0u;
};
// Original Win95 Pentium (from owner VM configuration): 133,333,333 Hz.
// EMU8000 internal chip rate: 44,100 Hz.
inline constexpr std::uint64_t kP003GuestCpuHz=133333333u;
inline constexpr std::uint64_t kP003ChipHz=44100u;

// The scheduler accepts a *modeled* CPU phase at a native MIDI event
// in guest cycles after the start of the 44.1kHz chip sample. No source
// fixture, original trace, MIDI pitch, preset ID or note-specific clock
// table can be used to generate these timestamps.
// The caller CANNOT infer the real Windows 95 task scheduler phase from
// a sample-accurate VST3 MIDI event. An explicit experimental phase is
// therefore a research parameter, NOT authenticated original timing.
[[nodiscard]] bool p003CandidateAutonomousNoteTimelineAtPhase(
    std::span<const awe64::ChipPortWrite> native,
    bool note_off,
    std::uint32_t phase_cycles,
    std::span<SourceClockedPort> out,
    std::size_t& count,
    P003AutonomousTimingStats* stats=nullptr) noexcept;

// Existing source-free private VST3 experiment uses a deterministic
// half-sample phase in the absence of a guest CPU time origin.
// Other guest phases require independent tests before any release claim.
[[nodiscard]] bool p003CandidateAutonomousNoteTimeline(
    std::span<const awe64::ChipPortWrite> native,
    bool note_off,
    std::span<SourceClockedPort> out,
    std::size_t& count,
    P003AutonomousTimingStats* stats=nullptr) noexcept;
} // namespace awe64::creative
