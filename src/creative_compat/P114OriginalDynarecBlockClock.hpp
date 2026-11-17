#pragma once
// P114: research-only accounting of the ORIGINAL old 86Box x64 dynarec TSC.
// Basis: turican0/86Box-AWE32 v6.0-AWE32-0.93, src/cpu/386_dynarec.c,
// update_tsc() and exec386_dynarec() block-end accounting.
// Source of actual x86 instruction costs is NOT implemented here. Do not use
// this component to infer missing costs, source NoteOn traces or product times.
#include <cstdint>
#include <limits>

namespace awe64::research_p114 {

class OriginalDynarecBlockClock final {
    std::int32_t cycles_old_ = 0;
    std::int32_t cycles_ = 0;
    std::int32_t acycs_ = 0;
    std::uint64_t tsc_old_ = 0;
    std::uint64_t tsc_ = 0;
    std::uint64_t ports_ = 0;
    bool inrecomp_ = false;
    bool active_ = false;
    bool valid_ = true;

    [[nodiscard]] bool reconcile(const bool include_acycs) noexcept {
        if (!valid_ || !active_ || tsc_ < tsc_old_) {
            valid_ = false;
            return false;
        }
        // Exact source ordering: cycdiff=(cycles_old-cycles), optionally
        // +acycs while inrecomp, then -= (tsc-tsc_old) if interim time
        // has already been counted. 64-bit intermediates avoid C overflow.
        const std::int64_t spent =
            static_cast<std::int64_t>(cycles_old_) - cycles_ +
            ((include_acycs && inrecomp_) ? acycs_ : 0);
        const std::uint64_t booked = tsc_ - tsc_old_;
        if (spent <= 0 || static_cast<std::uint64_t>(spent) <= booked)
            return true;
        const auto add = static_cast<std::uint64_t>(spent) - booked;
        if (tsc_ > std::numeric_limits<std::uint64_t>::max() - add) {
            valid_ = false;
            return false;
        }
        tsc_ += add;
        // Source also processes timer_target here. Timer execution is NOT
        // modeled; callers may explicitly advance the interim clock.
        return true;
    }

public:
    [[nodiscard]] bool begin_block(const std::int32_t available_cycles,
                                   const std::uint64_t initial_tsc,
                                   const bool jit_code) noexcept {
        if (active_ || !valid_ || available_cycles < 0) {
            valid_ = false;
            return false;
        }
        cycles_old_ = cycles_ = available_cycles;
        tsc_old_ = tsc_ = initial_tsc;
        acycs_ = 0;
        ports_ = 0;
        inrecomp_ = jit_code;
        active_ = true;
        return true;
    }
    // Input must be the ORIGINAL actually executed x86/JIT CPU cost (not
    // a guessed port-gap interval). Many physical OUTs may occur between
    // two charges. No synthetic cost is added by port_timestamp().
    [[nodiscard]] bool charge_executed_cycles(const std::int32_t cost) noexcept {
        if (!valid_ || !active_ || cost < 0 ||
            static_cast<std::int64_t>(cycles_) - cost <
                 std::numeric_limits<std::int32_t>::min()) {
            valid_ = false;
            return false;
        }
        cycles_ -= cost;
        return true;
    }
    // P119: generated x64 ADD signed_delta to cpu_state._cycles.
    // Call only when that ORIGINAL generated ADD actually executes,
    // NEVER when its compiler-side delta is merely pending.
    // Positive deltas are legal branch refunds; acycs is separate.
    [[nodiscard]] bool apply_source_emitted_cycles_delta(
        std::int32_t delta) noexcept {
        const auto next=static_cast<std::int64_t>(cycles_)+delta;
        if(!valid_ || !active_ ||
           next<std::numeric_limits<std::int32_t>::min() ||
           next>std::numeric_limits<std::int32_t>::max()) {
            valid_=false;
            return false;
        }
        cycles_=static_cast<std::int32_t>(next);
        return true;
    }
    // Optional source USE_ACYCS; default build of source may omit it.
    [[nodiscard]] bool set_acycs(const std::int32_t cycles) noexcept {
        if (!valid_ || !active_ || !inrecomp_) {
            valid_ = false;
            return false;
        }
        acycs_ = cycles;
        return true;
    }
    // Models interim advancement by an external, already running timer.
    // Does not infer timer callbacks, interrupts, or the host scheduler.
    [[nodiscard]] bool advance_interim_tsc(const std::uint64_t elapsed) noexcept {
        if (!valid_ || !active_ ||
            tsc_ > std::numeric_limits<std::uint64_t>::max() - elapsed) {
            valid_ = false;
            return false;
        }
        tsc_ += elapsed;
        return true;
    }
    [[nodiscard]] bool port_timestamp(std::uint64_t& out_tsc) noexcept {
        if (!reconcile(true)) return false;
        if (ports_ == std::numeric_limits<std::uint64_t>::max()) {
            valid_ = false;
            return false;
        }
        ++ports_;
        out_tsc = tsc_;
        return true;
    }
    // Original loop-end uses oldcyc-cycles and oldtsc, WITHOUT +acycs.
    [[nodiscard]] bool finish_block(std::uint64_t& final_tsc) noexcept {
        if (!reconcile(false)) return false;
        final_tsc = tsc_;
        active_ = false;
        return true;
    }
    [[nodiscard]] bool valid() const noexcept { return valid_; }
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] std::uint64_t port_count() const noexcept { return ports_; }
    [[nodiscard]] static constexpr bool autonomous_noteon_ready() noexcept {
        return false; // still requires executed path, per-op costs and phase
    }
};

} // namespace awe64::research_p114
