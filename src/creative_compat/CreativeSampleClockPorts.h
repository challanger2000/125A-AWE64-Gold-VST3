#pragma once
// P002: realtime-safe clocked EMU8000 OUT executor, not a Creative timing
// generator. Only independently measured original chip-frame offsets may be
// submitted for an ORIGINAL Win95 driver-equivalence claim.
#include "Chip.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
namespace awe64::creative {
struct SourceClockedPort {
    std::uint32_t relative_frame=0;
    std::uint16_t port=0;
    std::uint16_t value=0;
    [[nodiscard]] bool operator==(const SourceClockedPort&) const noexcept = default;
};
// P002 original-driver owner fixture gate. This validates synthetic
// test vectors in CI and exactly SHA-attested Win95 note events privately.
// No source values are replayed: returned words are the independently
// generated native port/value pairs, with only authenticated frame
// offsets taken from the original driver's capture.
// Atomic: on any mismatch, no output is modified and count remains zero.
[[nodiscard]] bool checkedNativePortsAtSourceFrames(
    std::span<const SourceClockedPort> original,
    std::span<const awe64::ChipPortWrite> independently_generated,
    std::span<SourceClockedPort> scheduled,
    std::size_t& count) noexcept;
class SourceClockedPortPlayer final {
public:
    static constexpr std::size_t kMaxEvents=512u;
    // Reject malformed or unsorted plans atomically; no chip writes.
    // All offsets are measured in 44.1-kHz chip frames from NoteOn.
    [[nodiscard]] bool stage(awe64::Chip& chip,
        std::span<const SourceClockedPort> source) noexcept;
    // Port commands at frame T execute BEFORE rendering sample T.
    // May be called with arbitrary host block sizes, no heap or IO.
    void render(awe64::Chip& chip,std::int32_t* stereo,
                std::size_t frames) noexcept;
    void clear() noexcept {count_=0;next_=0;started_=false;timing_error_=false;}
    [[nodiscard]] bool finished() const noexcept {
        return started_ && !timing_error_ && next_==count_;
    }
    [[nodiscard]] bool missedDeadline() const noexcept { return timing_error_; }
    [[nodiscard]] std::size_t pending() const noexcept {
        return count_-next_;
    }
private:
    std::array<SourceClockedPort,kMaxEvents> events_{};
    std::size_t count_=0,next_=0;
    std::uint64_t first_frame_=0;
    bool started_=false,timing_error_=false;
};
}
