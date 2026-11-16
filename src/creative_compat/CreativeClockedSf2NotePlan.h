#pragma once
// Bridge between independently compiled source-shaped original SF2
// MIDI->VXD typed OUTs and the sample-clock chip executor. The register
// VALUES come only from native original-bank decoding. Frame offsets
// must be established by independent original Win95 trace evidence.
// This builder alone does not establish any Win95 audio equivalence.
#include "CreativeSf2FullNoteOnPorts.h"
#include "CreativeSampleClockPorts.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace awe64::creative {
struct ClockedSf2NotePlan {
    std::array<SourceClockedPort,SourceClockedPortPlayer::kMaxEvents> words{};
    std::size_t count=0;
    [[nodiscard]] std::span<const SourceClockedPort> events() const noexcept {
        return {words.data(),count};
    }
};
// External offsets correspond to EVERY actual 16-bit physical port write,
// including each second half of an original OUT EAX. The caller must
// independently verify them against a genuine original-driver clock trace;
// a guessed C4/D4 schedule must never enter a production parity claim.
// Preflights all six potential voices before yielding a single write.
// Does NOT perform chip IO, copy any original trace values or change DSP.
[[nodiscard]] std::optional<ClockedSf2NotePlan>
candidateClockedSf2TypedOuts(
    const OriginalSf2FullNoteOnPlan& independently_compiled_native,
    std::span<const std::uint32_t> externally_verified_relative_frames) noexcept;
}
