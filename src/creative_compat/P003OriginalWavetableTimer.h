#pragma once
// P003 research only: original 86Box 32:32 wavetable timer arithmetic.
// Source: turican0/86Box-AWE32 v6.0-AWE32-0.93:
//   src/include/86box/timer.h timer_advance_u64()
//   src/timer.c timer_process()
//   src/sound/sound.c wavetable_poll(), sound_speed_changed()
//   src/sound/snd_emu8k.c emu8k_update_chunk()
// This is NOT a full Creative Pentium instruction-cycle scheduler.
// Genuine guest sample timer state/latch and exact CPU block boundaries
// must be independently supplied: they cannot be invented from MIDI pitches.
#include <cstdint>
namespace awe64::research {
class OriginalWavetableTimer final {
public:
    bool init(std::uint64_t timer_target_integer,
              std::uint32_t timer_target_fraction,
              std::uint64_t wavetable_latch_32_32,
              std::uint64_t already_rendered_frames,
              std::uint64_t starting_block_tsc) noexcept;
    // CPU has just completed an execution block; source timer_process()
    // invokes wavetable_poll() repeatedly for every due timer target.
    // Other timer callbacks are NOT simulated here.
    bool onCpuBlockEnd(std::uint64_t current_tsc) noexcept;
    [[nodiscard]] std::uint64_t frame() const noexcept {return frame_;}
    [[nodiscard]] std::uint64_t dueInteger() const noexcept {return integer_;}
    [[nodiscard]] std::uint32_t dueFraction() const noexcept {return fraction_;}
    [[nodiscard]] bool ready() const noexcept {return ready_;}
private:
    std::uint64_t integer_=0;
    std::uint32_t fraction_=0;
    std::uint64_t latch_=0;
    std::uint64_t frame_=0;
    std::uint64_t last_block_tsc_=0;
    bool ready_=false;
};
} // namespace awe64::research
