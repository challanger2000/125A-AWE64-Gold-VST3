#pragma once
// P119 source-derived old x64 86Box compiler-side deferred clock.
//
// src/codegen/codegen_accumulate_x86-64.c, pinned original fork:
//   codegen_accumulate => acc_regs[ACCREG_cycles].count += delta
//   codegen_accumulate_flush => emit ADD signed immediate to _cycles
//                              then clear compiler pending count
//   codegen_accumulate_reset => clear pending with NO emitted ADD
//
// Flush emits HOST MACHINE CODE; it does not mean the ADD executed.
// A later conditional branch can skip that emission. Do not add
// pending, committed, or acycs to TSC without original runtime events.
// Research only; no Creative VXD, ROM, SF2, private sample or live PCM.
#include <cstdint>
#include <limits>

namespace awe64::creative {
struct P005OriginalEmittedCycleAdd final {
    std::uint64_t generation=0u;
    std::uint32_t ordinal=0u;
    std::int32_t signed_delta=0;
    bool emitted=false;
};
class P005OriginalJitDeferredCycleEmitter final {
public:
    [[nodiscard]] bool beginTranslation(std::uint64_t generation) noexcept {
        if(failed_ || active_ || generation==0u ||
           generation<=lastGeneration_)return fail();
        active_=true;
        generation_=generation;
        pending_=0;
        ordinal_=0u;
        return true;
    }
    [[nodiscard]] bool accumulate(std::int32_t delta) noexcept {
        const auto next=static_cast<std::int64_t>(pending_)+delta;
        if(failed_ || !active_ ||
           next<std::numeric_limits<std::int32_t>::min() ||
           next>std::numeric_limits<std::int32_t>::max())
            return fail();
        pending_=static_cast<std::int32_t>(next);
        return true;
    }
    // Emits metadata for a generated ADD immediate, NOT a runtime TSC.
    [[nodiscard]] bool flush(P005OriginalEmittedCycleAdd& out) noexcept {
        if(failed_ || !active_ ||
           ordinal_==std::numeric_limits<std::uint32_t>::max())
            return fail();
        out={generation_,++ordinal_,pending_,pending_!=0};
        pending_=0;
        return true;
    }
    [[nodiscard]] bool resetWithoutEmission() noexcept {
        if(failed_ || !active_)return fail();
        pending_=0;
        return true;
    }
    [[nodiscard]] bool endTranslation() noexcept {
        if(failed_ || !active_ || pending_!=0)return fail();
        lastGeneration_=generation_;
        active_=false;
        return true;
    }
    [[nodiscard]] std::int32_t pendingCompilerDelta() const noexcept {
        return pending_;
    }
    [[nodiscard]] bool failed() const noexcept {return failed_;}
    [[nodiscard]] std::uint64_t lastGeneration() const noexcept {
        return lastGeneration_;
    }
    void reset() noexcept {*this={};}
private:
    [[nodiscard]] bool fail() noexcept {
        failed_=true;active_=false;return false;
    }
    std::uint64_t generation_=0u,lastGeneration_=0u;
    std::uint32_t ordinal_=0u;
    std::int32_t pending_=0;
    bool active_=false,failed_=false;
};
} // namespace awe64::creative
