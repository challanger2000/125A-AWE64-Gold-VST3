#pragma once
// P005 original-source-based CPU block -> 44.1kHz EMU8000 clock bridge.
// The original 86Box old-x64 dynarec charges completed x86 blocks, then
// calls timer_process(); EMU port callbacks inside that block therefore
// observe the PREVIOUS wavetable timer epoch. This bridges:
//  - P54C original integer U/V/AGI translation-accounting semantics
//  - original pit_set_clock() 400MHz/3 and 32:32 wavetable latch
//  - source-ordered 16-bit EMU8000 OUT words, without guest trace times
//
// NOTE: This primitive charges an integer model at executed-block time.
// ORIGINAL 86Box does not do that when a JIT block is reused. For actual
// old-Dynarec code() reuse prefer P005OriginalPentiumCompiledBlockClock:
// translate once by compile_epoch, execute the cached cycle cost later.
// THIS primitive is deliberately a synthetic block-boundary test only.
//
// Caveats: the caller must still supply verified original decoded x86
// instructions and exact executed block/OUT ordering. Interpreter paths,
// other 86Box timers, FPU/MMX and actual instruction-site/branch replay
// are not represented. This mechanism alone does not prove native
// Win95 TSC equality or bit-identical DRY PCM. NO heuristic phase P.
#include "P005OriginalPentiumIntegerTiming.hpp"
#include "P003OriginalWavetableTimer.h"
#include "P003OriginalPitClock.h"
#include "CreativeSampleClockPorts.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace awe64::creative {
class P005OriginalPentiumCpuAudioTimeline final {
public:
    static constexpr std::size_t kMaxOuts=
        SourceClockedPortPlayer::kMaxEvents;
    static constexpr std::uint32_t kMaximumFrames=44100u*30u;

    // Host-defined deterministic fresh virtual CPU/EMU epoch.
    // This is NOT the fitted phase P of any original Win95 VM session.
    [[nodiscard]] bool startFreshChipEpoch() noexcept {
        if(ready_ || failed_)return false;
        constexpr auto latch=awe64::research::
            original86BoxWavetableLatch32_32(133333333u);
        static_assert(latch==12985539821609ull);
        if(!timer_.init(latch>>32u,
                        static_cast<std::uint32_t>(latch),
                        latch,0u,0u))
            return fail();
        ready_=true;
        return true;
    }
    void reset() noexcept { *this={}; }
    [[nodiscard]] bool beginCpuBlock() noexcept {
        if(!ready_ || failed_ || open_ ||
           !integer_.beginBlock())return fail();
        pending_count_=0u;
        decoded_count_=0u;
        open_=true;
        return true;
    }
    [[nodiscard]] bool decodedIntegerInstruction(
        const P005OriginalP54cDecodedIntegerOp& op) noexcept {
        if(!open_ || failed_ || !integer_.append(op))
            return fail();
        ++decoded_count_;
        return true;
    }
    // Actual original x86 OUT dispatch should call this at its ORIGINAL
    // position inside a decoded JIT block. The timer is not advanced by
    // either the opcode or the OUT itself; 86Box does so on block exit.
    [[nodiscard]] bool nativeOut16(
        const std::uint16_t port,const std::uint16_t value) noexcept {
        if(!open_ || failed_ || decoded_count_==0u ||
           pending_count_>=kMaxOuts ||
           (port!=0xe22u && port!=0xe20u &&
            port!=0xa20u && port!=0xa22u &&
            port!=0x620u && port!=0x622u) ||
           timer_.frame()>kMaximumFrames)
            return fail();
        pending_[pending_count_++]={
            static_cast<std::uint32_t>(timer_.frame()),port,value};
        return true;
    }
    [[nodiscard]] bool endCpuBlock() noexcept {
        if(!open_ || failed_ || !integer_.endBlock())return fail();
        const auto spent=integer_.lastBlockCycles();
        if(spent<=0 ||
           static_cast<std::uint64_t>(spent)>
               std::numeric_limits<std::uint64_t>::max()-cpu_tsc_ ||
           delivered_count_>kMaxOuts-pending_count_)
            return fail();
        const auto nextTsc=cpu_tsc_+static_cast<std::uint64_t>(spent);
        if(!timer_.onCpuBlockEnd(nextTsc) ||
           timer_.frame()>kMaximumFrames)return fail();
        cpu_tsc_=nextTsc;
        for(std::size_t i=0;i<pending_count_;++i)
            delivered_[delivered_count_++]=pending_[i];
        pending_count_=0u;
        open_=false;
        return true;
    }
    [[nodiscard]] std::span<const SourceClockedPort>
    sourceOrderedPortPlan() const noexcept {
        if(!ready_ || failed_ || open_)return {};
        return {delivered_.data(),delivered_count_};
    }
    [[nodiscard]] std::uint64_t cpuTsc() const noexcept {return cpu_tsc_;}
    [[nodiscard]] std::uint64_t chipFrame() const noexcept {
        return timer_.frame();
    }
    [[nodiscard]] bool ready() const noexcept {
        return ready_ && !failed_;
    }
    [[nodiscard]] bool failed() const noexcept {return failed_;}
private:
    [[nodiscard]] bool fail() noexcept {
        failed_=true;
        ready_=false;
        open_=false;
        delivered_count_=pending_count_=0u;
        return false;
    }
    P005OriginalPentiumIntegerTiming integer_{};
    awe64::research::OriginalWavetableTimer timer_{};
    std::array<SourceClockedPort,kMaxOuts> delivered_{};
    std::array<SourceClockedPort,kMaxOuts> pending_{};
    std::uint64_t cpu_tsc_=0u;
    std::size_t delivered_count_=0u,pending_count_=0u;
    std::size_t decoded_count_=0u;
    bool ready_=false,failed_=false,open_=false;
};
} // namespace awe64::creative
