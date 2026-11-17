// Original CPU/chip clock regression, NOT empirical register-cost fitting.
// Source: real 86Box PIT + timer + sound code at tagged v6.0-AWE32-0.93.
#include "P003OriginalPitClock.h"
#include "P003OriginalWavetableTimer.h"
#include <cstdint>
#include <cstdio>
using namespace awe64::research;
#define CHECK(x) do {if(!(x)){std::printf("P003_FRACTIONAL_PIT_CLOCK_FAIL line=%d\\n",__LINE__);return 1;}}while(0)

int main(){
    constexpr auto hz=original86BoxCpuClockHz(133333333u);
    // Exact mathematical result is 400000000/3; a binary double
    // is only its nearest representation, as in the original 86Box.
    CHECK(hz>133333333.3333333 && hz<133333333.3333334);
    CHECK(original86BoxCpuClockHz(100000000u)==100000000.0);
    CHECK(original86BoxCpuClockHz(200000000u)==200000000.0);
    CHECK(original86BoxCpuClockHz(166666666u)>166666666.6666666);
    constexpr auto TIMER_USEC_32_32=original86BoxTimerUsec32_32(133333333u);
    constexpr auto WT_LATCH_32_32=original86BoxWavetableLatch32_32(133333333u);
    // Verified source 32:32 float/truncation sequence.
    CHECK(TIMER_USEC_32_32==572662306133ull);
    CHECK(WT_LATCH_32_32==12985539821609ull);
    CHECK((WT_LATCH_32_32>>32u)==3023ull);
    CHECK((WT_LATCH_32_32 & 0xffffffffull)!=0ull);
    // A 32:32 fractional source-sample timer should drift by less than
    // 0.01 CPU cycle over two million samples at this repaired clock.
    constexpr std::uint64_t n=2000000ull;
    const auto latch_elapsed=static_cast<double>(n)*
        static_cast<double>(WT_LATCH_32_32)/4294967296.0;
    const auto ideal_elapsed=
        static_cast<double>(n)*hz/44100.0;
    const auto signed_error=latch_elapsed-ideal_elapsed;
    CHECK(signed_error>-0.01 && signed_error<0.01);
    // Match explicit known original 86Box fixed-point timer recurrence.
    OriginalWavetableTimer timer;
    const std::uint64_t initial_tsc=1024u;
    const std::uint64_t target=initial_tsc+(WT_LATCH_32_32>>32u);
    const std::uint32_t frac=static_cast<std::uint32_t>(WT_LATCH_32_32);
    CHECK(timer.init(target,frac,WT_LATCH_32_32,0u,initial_tsc));
    CHECK(timer.onCpuBlockEnd(target-1u));
    CHECK(timer.frame()==0u);
    CHECK(timer.onCpuBlockEnd(target));
    CHECK(timer.frame()==1u);
    const auto want_next=initial_tsc+
        static_cast<std::uint64_t>((2u*WT_LATCH_32_32)>>32u);
    CHECK(timer.dueInteger()==want_next);
    CHECK(timer.onCpuBlockEnd(want_next));
    CHECK(timer.frame()==2u);
    std::puts("P003_PIT_133_1_3_CPU_HZ_SOURCE_LATCH32_32_PASS"
              " original_pit_set_clock=YES source_86box_unchanged=YES"
              " complete_driver_cpu_timing_proven=NO");
    return 0;
}
