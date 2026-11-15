#pragma once
// P003 RESEARCH ONLY. Candidate, source-independent register pacing.
// This is NOT a proven emulation of Creative's Windows95 CPU timings.
#include "CreativeSampleClockPorts.h"
#include <cstddef>
#include <cstdint>
#include <span>
namespace awe64::creative {
struct P003AutonomousTimingStats {
    std::size_t writes=0u;
    std::size_t initial_voice_prelude_writes=0u;
    std::uint32_t final_relative_frame=0u;
};
// Independently generated MIDI->port words only, never a source note
// fixture, register trace, preset/key conditional, or captured timestamp.
// The model is bounded and deliberately refuses unsupported layouts.
// A successful P003 owner trace alone does NOT generalize to other
// Creative/Win95 instances or sample clock phases.
[[nodiscard]] bool p003CandidateAutonomousNoteTimeline(
    std::span<const awe64::ChipPortWrite> native,
    bool note_off,
    std::span<SourceClockedPort> out,
    std::size_t& count,
    P003AutonomousTimingStats* stats=nullptr) noexcept;
} // namespace awe64::creative
