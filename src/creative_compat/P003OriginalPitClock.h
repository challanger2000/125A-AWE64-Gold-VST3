#pragma once
// P003 RESEARCH: direct transcription of original turican0/86Box-AWE32
// v6.0-AWE32-0.93/src/pit.c pit_set_clock() CPU speed normalization,
// followed by src/sound/sound.c sound_speed_changed() 44.1kHz latch.
// Do NOT use cpu_s->rspeed=133333333 as exact fractional audio CPU Hz.
// The original code intentionally repairs thirds of 100MHz before
// calculating TIMER_USEC, and then truncates both 32:32 fixed-point values.
#include <cstdint>
namespace awe64::research {
inline constexpr double original86BoxCpuClockHz(
    const std::uint32_t source_cpu_rspeed_hz) noexcept {
    const auto remainder=source_cpu_rspeed_hz%100000000u;
    if(remainder==66666666u)
        return static_cast<double>(source_cpu_rspeed_hz-remainder)+
               (200000000.0/3.0);
    if(remainder==33333333u)
        return static_cast<double>(source_cpu_rspeed_hz-remainder)+
               (100000000.0/3.0);
    return static_cast<double>(source_cpu_rspeed_hz);
}
inline constexpr std::uint64_t original86BoxTimerUsec32_32(
    const std::uint32_t source_cpu_rspeed_hz) noexcept {
    return static_cast<std::uint64_t>(
        (original86BoxCpuClockHz(source_cpu_rspeed_hz)/1000000.0)*
        4294967296.0);
}
inline constexpr std::uint64_t original86BoxWavetableLatch32_32(
    const std::uint32_t source_cpu_rspeed_hz,
    const std::uint32_t original_wavetable_hz=44100u) noexcept {
    if(original_wavetable_hz==0u)return 0u;
    // Same operations/rounding order as actual 86Box sound_speed_changed.
    return static_cast<std::uint64_t>(
        static_cast<double>(
            original86BoxTimerUsec32_32(source_cpu_rspeed_hz))*
        (1000000.0/static_cast<double>(original_wavetable_hz)));
}
// Exact rational representation of the 133 MHz Pentium-clock repair:
// 100000000 + 100000000/3 = 400000000/3 Hz.
// Source pit_set_clock() validates this ratio, not a fitted coefficient.
inline constexpr std::uint64_t kOriginalPentium133ClockNumerator=400000000ull;
inline constexpr std::uint32_t kOriginalPentium133ClockDenominator=3u;
inline constexpr std::uint32_t kOriginalWavetableHz=44100u;
} // namespace awe64::research
