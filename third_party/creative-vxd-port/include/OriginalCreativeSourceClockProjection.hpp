#pragma once
// ORIGINAL 86Box WIN95 CAPTURE-CLOCK PROJECTION ONLY.
// Exactly source-observed CPU TSC -> 44.1-kHz EMU8000 frame conversion for
// the specified 133 1/3 MHz Win95 guest session. This does NOT generate
// Creative function-entry times, execution cycles, register writes, or
// a native VST3 scheduler. Capture-specific phase MUST be authenticated.
// Host output rates (48/88.2/96 kHz) have a separate rate adapter.
#include <cstdint>
#include <limits>
#include <optional>

namespace awe64::original_vxd {
struct OriginalCreativeSourceClockProjection final {
    static constexpr std::uint64_t cycles_numerator=400'000'000ull;
    static constexpr std::uint64_t cycles_denominator=132'300ull;

    // The original recording's anchor. No cross-session absolute TSC values
    // may be embedded as general library defaults.
    std::uint64_t anchor_cpu_tsc=0;
    std::uint64_t anchor_chip_frame=0;
    // Fractional CPU/audio phase at the anchor, scaled by cycles_numerator;
    // 0 <= phase < cycles_numerator.
    std::uint32_t anchor_phase=0;
    bool source_session_calibrated=false;

    [[nodiscard]] constexpr std::optional<std::uint64_t>
    frame_at_cpu_tsc(std::uint64_t cpu_tsc) const noexcept {
        if(!source_session_calibrated ||
           anchor_phase>=cycles_numerator ||
           cpu_tsc<anchor_cpu_tsc)
            return std::nullopt;
        const auto elapsed=cpu_tsc-anchor_cpu_tsc;
        // Keep multiplication safe for even very large 64-bit TSC inputs.
        const auto whole=elapsed/cycles_numerator;
        const auto part=elapsed%cycles_numerator;
        if(whole>std::numeric_limits<std::uint64_t>::max()/cycles_denominator)
            return std::nullopt;
        const auto main=whole*cycles_denominator;
        // part*132300 + anchor_phase < 5.3e13, safely within uint64.
        const auto rest=(part*cycles_denominator+anchor_phase)/cycles_numerator;
        if(main>std::numeric_limits<std::uint64_t>::max()-rest)
            return std::nullopt;
        const auto additional=main+rest;
        if(anchor_chip_frame>std::numeric_limits<std::uint64_t>::max()-additional)
            return std::nullopt;
        return anchor_chip_frame+additional;
    }
};
} // namespace awe64::original_vxd
