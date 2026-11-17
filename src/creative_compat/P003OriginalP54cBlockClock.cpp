#include "P003OriginalP54cBlockClock.h"
#include <cstdint>
#include <limits>
namespace awe64::research {
bool OriginalP54cBlockClock::initialize(
    const std::uint64_t guest_tsc,const std::int32_t original_cpu_budget,
    const std::uint64_t first_due_tsc,
    const std::uint32_t first_due_fraction,
    const std::uint64_t original_wavetable_latch_32_32,
    const std::uint64_t initial_chip_frame) noexcept {
    initialized_=false;
    active_=false;
    if(original_cpu_budget<=0 ||
       !timer_.init(first_due_tsc,first_due_fraction,
                    original_wavetable_latch_32_32,
                    initial_chip_frame,guest_tsc))
        return false;
    tsc_=guest_tsc;
    block_start_tsc_=guest_tsc;
    cycles_=original_cpu_budget;
    block_start_cycles_=original_cpu_budget;
    initialized_=true;
    return true;
}
bool OriginalP54cBlockClock::beginBlock() noexcept {
    if(!initialized_ || active_)return false;
    active_=true;
    block_start_tsc_=tsc_;
    block_start_cycles_=cycles_;
    return true;
}
bool OriginalP54cBlockClock::debitTranslatorCycles(
    const std::uint32_t n) noexcept {
    if(!initialized_ || !active_ || n==0u ||
       n>static_cast<std::uint32_t>(
           static_cast<std::int64_t>(cycles_)-
           std::numeric_limits<std::int32_t>::min()))
        return false;
    // Equivalent to the original JIT's ACCREG_cycles arithmetic.
    // 86Box can overshoot zero at the last block; it is not
    // legitimate to clamp remaining cycles to zero.
    cycles_-=static_cast<std::int64_t>(n);
    return true;
}
std::optional<OriginalP54cIoStamp> OriginalP54cBlockClock::emuPort(
    const std::uint16_t port,const std::uint16_t word) const noexcept {
    if(!initialized_ || !active_)return std::nullopt;
    switch(port) {
        case 0xe22u:case 0xe20u:case 0xa20u:case 0xa22u:
        case 0x620u:case 0x622u:break;
        default:return std::nullopt;
    }
    // Original src/io.c calls update_tsc only for certain special
    // unhandled delay ports; valid registered EMU8000 port callbacks
    // see the already latched tsc. Remaining CPU budget may fall
    // between callbacks despite their identical guest_tsc.
    return OriginalP54cIoStamp{tsc_,cycles_,timer_.frame(),port,word};
}
bool OriginalP54cBlockClock::reconcile(const bool after_block) noexcept {
    if(!initialized_ || !active_ || tsc_<block_start_tsc_)return false;
    const auto spent=static_cast<std::int64_t>(block_start_cycles_)-
        static_cast<std::int64_t>(cycles_);
    if(spent<0)return false;
    const auto interim=tsc_-block_start_tsc_;
    // Exact source accounting: spent cycles LESS previously
    // advanced tsc. Do not charge CPU time twice.
    std::int64_t remainder=0;
    if(interim>static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max())) {
        remainder=std::numeric_limits<std::int64_t>::min();
    }else remainder=spent-static_cast<std::int64_t>(interim);
    if(interim==0u && after_block) {
        // Original exec386_dynarec else branch: tsc += cycdiff.
        remainder=spent;
    }
    if(remainder>0) {
        if(tsc_>std::numeric_limits<std::uint64_t>::max()-
           static_cast<std::uint64_t>(remainder))
            return false;
        tsc_+=static_cast<std::uint64_t>(remainder);
        // Original timer_process executes all due CPU-independent
        // timer callbacks. This restricted research clock only
        // tracks the 44.1kHz wavetable callback, not other guest IRQs.
        if(!timer_.onCpuBlockEnd(tsc_)) {
            initialized_=false;
            return false;
        }
    }
    return true;
}
bool OriginalP54cBlockClock::sourceUpdateTsc() noexcept {
    return reconcile(false);
}
bool OriginalP54cBlockClock::finishBlock() noexcept {
    if(!reconcile(true))return false;
    active_=false;
    return true;
}
} // namespace awe64::research
