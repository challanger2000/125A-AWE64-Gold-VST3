#include "OriginalCreativeSourceClockProjection.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

using awe64::original_vxd::OriginalCreativeSourceClockProjection;
#define require(expr) do { if (!(expr)) { std::fprintf(stderr, "failed line %d: %s\n", __LINE__, #expr); std::abort(); } } while(false)

int main() {
    // Entirely synthetic clock origin. Real original guest clock values are
    // private and are checked only by the SHA-pinned Python V4 trace audit.
    OriginalCreativeSourceClockProjection projection{};
    require(!projection.frame_at_cpu_tsc(1'000u));
    projection.anchor_cpu_tsc=1'000u;
    projection.anchor_chip_frame=4'000u;
    projection.anchor_phase=123'456'789u;
    projection.source_session_calibrated=true;
    require(projection.frame_at_cpu_tsc(1'000u)==4'000u);
    std::uint64_t previous=4'000u;
    for(std::uint64_t i=0;i<10'000;++i) {
        const auto delta=(i*1'234'567ull)%4'000'000'000ull;
        const auto expected=4'000ull+
            (delta*132'300ull+123'456'789ull)/400'000'000ull;
        require(projection.frame_at_cpu_tsc(1'000ull+delta)==expected);
    }
    for(std::uint64_t i=0;i<10'000;++i) {
        const auto delta=i*123'456ull;
        const auto value=projection.frame_at_cpu_tsc(1'000ull+delta);
        require(value && *value>=previous);
        previous=*value;
    }
    require(!projection.frame_at_cpu_tsc(999ull));
    projection.anchor_phase=400'000'000u;
    require(!projection.frame_at_cpu_tsc(1'000u));
    projection.anchor_phase=123'456'789u;
    const auto far=projection.frame_at_cpu_tsc(
        std::numeric_limits<std::uint64_t>::max());
    require(far && *far>4'000ull);
    projection.anchor_chip_frame=std::numeric_limits<std::uint64_t>::max();
    require(!projection.frame_at_cpu_tsc(projection.anchor_cpu_tsc+4000ull));
    projection.source_session_calibrated=false;
    require(!projection.frame_at_cpu_tsc(1'000u));
}
