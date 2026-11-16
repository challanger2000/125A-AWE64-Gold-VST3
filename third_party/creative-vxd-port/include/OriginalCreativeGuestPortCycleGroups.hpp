#pragma once
// ORIGINAL WIN95 V4 guest-port callback timestamp semantics.
// Multiple physical EMU8000 port callbacks may share one latched TSC while
// the guest's cycles-remaining counter decreases by 12/19/22 cycles.
// This helper does NOT synthesize original driver instruction timing.
// It keeps the source-captured audio frame distinct from an OPTIONAL
// diagnostic reconstruction using intra-group elapsed cycles.
#include "OriginalCreativeSourceClockProjection.hpp"
#include <cstdint>
#include <limits>
#include <optional>

namespace awe64::original_vxd {
struct OriginalCreativeGuestPortClockEvent {
    std::uint64_t captured_cpu_tsc = 0;
    std::int32_t guest_cycles_remaining = 0;
};
struct OriginalCreativeGuestPortClockPlacement {
    std::uint64_t captured_frame = 0;
    std::uint64_t hypothetical_fine_frame = 0;
    std::uint64_t hypothetical_fine_cpu_tsc = 0;
    std::uint8_t position_within_tsc_group = 0;
    std::uint8_t cycles_since_group_first_event = 0;
    bool hypothetical_frame_differs = false;
};

// For capture-parity checking, use captured_frame and keep all ordered
// callbacks of one TSC group together. The hypothetical_fine_* fields
// are diagnostics ONLY: no original Win95 frame logger has certified
// their use as separate audio-event scheduling times.
class OriginalCreativeGuestPortCycleGroups final {
public:
    explicit constexpr OriginalCreativeGuestPortCycleGroups(
            OriginalCreativeSourceClockProjection projection) noexcept
        : clock_(projection) {}

    [[nodiscard]] constexpr std::optional<OriginalCreativeGuestPortClockPlacement>
    append(OriginalCreativeGuestPortClockEvent event) noexcept {
        const bool same=initialized_ && event.captured_cpu_tsc==last_tsc_;
        if(initialized_ && event.captured_cpu_tsc<last_tsc_)
            return std::nullopt;
        // The original V4 source has group sizes 1,2,3 only. Reject an
        // unproven 4th callback rather than guessing how it was timestamped.
        if(same && group_count_>=3u)
            return std::nullopt;
        const auto new_first_remaining=same
            ? group_first_remaining_ :event.guest_cycles_remaining;
        const auto signed_offset=static_cast<std::int64_t>(new_first_remaining)-
            static_cast<std::int64_t>(event.guest_cycles_remaining);
        // Actual source offsets are only 0..26. This 64-cycle validation
        // margin permits recording slight source variations while failing
        // closed on counter wrap, refill or unexplained grouping.
        if(signed_offset<0 || signed_offset>64)
            return std::nullopt;
        const auto offset=static_cast<std::uint8_t>(signed_offset);
        if(same && offset<last_offset_)
            return std::nullopt;
        if(event.captured_cpu_tsc>
           std::numeric_limits<std::uint64_t>::max()-offset)
            return std::nullopt;
        const auto hypothetical_tsc=event.captured_cpu_tsc+offset;
        if(initialized_ && hypothetical_tsc<last_hypothetical_tsc_)
            return std::nullopt;
        const auto frame=clock_.frame_at_cpu_tsc(event.captured_cpu_tsc);
        const auto hypothetical_frame=clock_.frame_at_cpu_tsc(hypothetical_tsc);
        if(!frame || !hypothetical_frame)
            return std::nullopt;
        const auto slot=static_cast<std::uint8_t>(same?group_count_:0u);
        // Commit state only after all validation succeeds.
        initialized_=true;
        last_tsc_=event.captured_cpu_tsc;
        group_first_remaining_=new_first_remaining;
        group_count_=static_cast<std::uint8_t>(slot+1u);
        last_offset_=offset;
        last_hypothetical_tsc_=hypothetical_tsc;
        return OriginalCreativeGuestPortClockPlacement{
            *frame,*hypothetical_frame,hypothetical_tsc,slot,offset,
            *frame!=*hypothetical_frame};
    }
    constexpr void reset() noexcept {
        initialized_=false;
        last_tsc_=0;
        last_hypothetical_tsc_=0;
        group_first_remaining_=0;
        group_count_=0;
        last_offset_=0;
    }
private:
    OriginalCreativeSourceClockProjection clock_{};
    std::uint64_t last_tsc_=0;
    std::uint64_t last_hypothetical_tsc_=0;
    std::int32_t group_first_remaining_=0;
    std::uint8_t group_count_=0;
    std::uint8_t last_offset_=0;
    bool initialized_=false;
};
} // namespace awe64::original_vxd
