#include "P003OriginalWavetableTimer.h"
#include "P003OriginalPitClock.h"
#include <cstdint>
#include <cstdio>
using awe64::research::OriginalWavetableTimer;
#define CK(cond) do {if(!(cond)){std::printf("P003_SOURCE_TIMER_FAILED line=%d\\n",__LINE__);return 1;}} while(0)
int main(){
    OriginalWavetableTimer timer;
    CK(!timer.init(100,0,0,0,0)); // Unknown latch: fail closed.
    CK(!timer.init(100,0,3ull<<32,0,101)); // Invalid initial timestamp.
    // Original 86Box uses integer timer_target comparator, though
    // recurrence advances the fraction without losing the carry.
    const std::uint64_t latch=(3ull<<32)|(1ull<<31); // 3.5 CPU ticks
    CK(timer.init(10,0x40000000u,latch,0,9));
    CK(timer.onCpuBlockEnd(9)); CK(timer.frame()==0);
    // Any OUT inside a CPU block sees the PREVIOUS sample epoch;
    // the timer callback is not processed until the block end.
    CK(timer.frame()==0);
    CK(timer.onCpuBlockEnd(10)); CK(timer.frame()==1);
    CK(timer.dueInteger()==13 && timer.dueFraction()==0xc0000000u);
    CK(timer.onCpuBlockEnd(13)); CK(timer.frame()==2);
    CK(timer.dueInteger()==17 && timer.dueFraction()==0x40000000u);
    CK(timer.onCpuBlockEnd(17)); CK(timer.frame()==3);
    CK(timer.onCpuBlockEnd(17)); CK(timer.frame()==3);
    CK(!timer.onCpuBlockEnd(16)); // TSC regression not acceptable.
    CK(timer.onCpuBlockEnd(24)); CK(timer.frame()==5);
    // No per-sample integer rounding drift after 3000 callbacks.
    OriginalWavetableTimer long_run;
    // Exact source-latch from pit_set_clock's 133 1/3 MHz repair,
    // and sound_speed_changed 44,100 Hz float/truncation sequence.
    constexpr std::uint64_t kLatch=
        awe64::research::original86BoxWavetableLatch32_32(133333333u);
    CK(kLatch==12985539821609ull);
    CK(long_run.init(100000u+(kLatch>>32u),
                     static_cast<std::uint32_t>(kLatch),
                     kLatch,0u,100000u));
    for(std::uint64_t n=1;n<=3000;++n) {
        const auto due=100000u+((n*kLatch)>>32u);
        CK(long_run.onCpuBlockEnd(due));
        CK(long_run.frame()==n);
    }
    CK(long_run.dueInteger()>100000u+(3000ull*kLatch>>32u));
    CK(!long_run.onCpuBlockEnd(100000u));
    std::puts("P003_86BOX_ORIGINAL_32_32_CPU_BLOCK_WAVETABLE_TIMER_PASS no_heap=YES original_midi_timing_verified=NO");
    return 0;
}
