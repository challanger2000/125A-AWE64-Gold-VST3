#include "P003OriginalWavetableTimer.h"
#include <cstdint>
#include <limits>
namespace awe64::research {
bool OriginalWavetableTimer::init(
    const std::uint64_t timer_target_integer,
    const std::uint32_t timer_target_fraction,
    const std::uint64_t wavetable_latch_32_32,
    const std::uint64_t already_rendered_frames,
    const std::uint64_t starting_block_tsc) noexcept {
    ready_=false;
    // 86Box's period has 32 integer CPU ticks and 32 fractional bits.
    // Require a known, independently verified positive source latch.
    if((wavetable_latch_32_32>>32u)==0u ||
       timer_target_integer<starting_block_tsc ||
       already_rendered_frames==std::numeric_limits<std::uint64_t>::max())
        return false;
    integer_=timer_target_integer;
    fraction_=timer_target_fraction;
    latch_=wavetable_latch_32_32;
    frame_=already_rendered_frames;
    last_block_tsc_=starting_block_tsc;
    ready_=true;
    return true;
}
bool OriginalWavetableTimer::onCpuBlockEnd(
    const std::uint64_t current_tsc) noexcept {
    if(!ready_ || current_tsc<last_block_tsc_)return false;
    // Source 86Box timer_process() dispatches all expired callbacks.
    // Large gaps need other real 86Box timers; fail closed rather than
    // silently skipping sample frames or synthesizing CPU events.
    constexpr std::uint32_t kMaxCallbacksPerBlock=192000u;
    std::uint32_t callbacks=0;
    while(integer_<=current_tsc) {
        if(callbacks==kMaxCallbacksPerBlock ||
           frame_==std::numeric_limits<std::uint64_t>::max()) {
            ready_=false;
            return false;
        }
        const auto whole=latch_>>32u;
        const auto frac=static_cast<std::uint32_t>(latch_);
        const auto frac_sum=static_cast<std::uint64_t>(fraction_)+frac;
        const auto carry=frac_sum>>32u;
        if(integer_>std::numeric_limits<std::uint64_t>::max()-whole-carry) {
            ready_=false;
            return false;
        }
        fraction_=static_cast<std::uint32_t>(frac_sum);
        integer_+=whole+carry;
        ++frame_;
        ++callbacks;
    }
    last_block_tsc_=current_tsc;
    return true;
}
} // namespace awe64::research
