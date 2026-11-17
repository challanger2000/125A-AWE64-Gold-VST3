#pragma once
// P005 research only: original live 86Box update_tsc() inside callbacks.
// Source original: 386_dynarec.c; P114 original C++23 arithmetic.
// Decoded/actually executed CPU cycle charges MUST come from authenticated
// source x86 execution, not P110 translation or recorded CPU timestamps.
// Calls original P114 at every physical OUT so the original 32:32
// wavetable timer may change BETWEEN ports of the same CPU block.
// Without a full executed x86 cost producer this is NOT product-ready.
#include "P114OriginalDynarecBlockClock.hpp"
#include "P005OriginalPentiumCompiledBlockClock.hpp"
#include "P003OriginalWavetableTimer.h"
#include "P003OriginalPitClock.h"
#include "CreativeSampleClockPorts.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
namespace awe64::creative {
class P005OriginalExecutedPortClock final {
public:
    static constexpr std::size_t kMaxPorts=SourceClockedPortPlayer::kMaxEvents;
    [[nodiscard]] bool startFreshVirtualEpoch() noexcept {
        if(ready_ || failed_)return false;
        constexpr auto latch=awe64::research::original86BoxWavetableLatch32_32(133333333u);
        static_assert(latch==12985539821609ull);
        if(!timer_.init(latch>>32u,static_cast<std::uint32_t>(latch),
                        latch,0u,0u))return fail();
        ready_=true;return true;
    }
    [[nodiscard]] bool beginVerifiedCompiledBlock(
        const P005OriginalPentiumCompiledBlockClock& catalog,
        std::size_t slot,std::uint64_t epoch,std::uint32_t pc,std::uint32_t phys,
        std::int32_t availableCycles,bool originalInRecomp) noexcept {
        if(!ready_ || failed_ || blockOpen_ ||
           !catalog.sourceGenerationMatches(slot,epoch,pc,phys) ||
           !block_.begin_block(availableCycles,cpuTsc_,originalInRecomp))
            return fail();
        blockOpen_=true;charges_=0u;return true;
    }
    // Only actual executed CPU charges; original compiler estimates alone
    // are NOT sufficient to call this at a genuine x86 execution point.
    [[nodiscard]] bool executedCycles(std::int32_t cost) noexcept {
        if(!blockOpen_ || failed_ || cost<0 ||
           !block_.charge_executed_cycles(cost))return fail();
        ++charges_;return true;
    }
    // P119 original flushed JIT CPU ADD, source execution required.
    // The untranslated or compiler-pending delta is NOT executable.
    [[nodiscard]] bool originalExecutedJitFlush(
        std::int32_t signed_delta) noexcept {
        if(!blockOpen_ || failed_ ||
           !block_.apply_source_emitted_cycles_delta(signed_delta))
            return fail();
        ++charges_;
        return true;
    }
    [[nodiscard]] bool originalAuxCycles(std::int32_t cost) noexcept {
        if(!blockOpen_ || failed_ || !block_.set_acycs(cost))return fail();
        return true;
    }
    [[nodiscard]] bool originalOut16(std::uint16_t port,std::uint16_t value) noexcept {
        if(!blockOpen_ || failed_ || charges_==0u ||
           count_==kMaxPorts || !validPort(port))return fail();
        std::uint64_t stamp=0u;
        if(!block_.port_timestamp(stamp) ||
           !timer_.onCpuBlockEnd(stamp) ||
           timer_.frame()>44100u*30u ||
           timer_.frame()>std::numeric_limits<std::uint32_t>::max())
            return fail();
        outputs_[count_++]={static_cast<std::uint32_t>(timer_.frame()),port,value};
        return true;
    }
    [[nodiscard]] bool endVerifiedBlock() noexcept {
        if(!blockOpen_ || failed_ || charges_==0u)return fail();
        std::uint64_t stamp=0u;
        if(!block_.finish_block(stamp) || !timer_.onCpuBlockEnd(stamp) ||
           timer_.frame()>44100u*30u)return fail();
        cpuTsc_=stamp;blockOpen_=false;++finishedBlocks_;return true;
    }
    [[nodiscard]] std::span<const SourceClockedPort> orderedPortPlan() const noexcept {
        if(!ready_ || failed_ || blockOpen_)return {};
        return {outputs_.data(),count_};
    }
    [[nodiscard]] std::uint64_t cpuTsc() const noexcept {return cpuTsc_;}
    [[nodiscard]] std::uint64_t chipFrame() const noexcept {return timer_.frame();}
    [[nodiscard]] std::uint64_t finishedBlocks() const noexcept {return finishedBlocks_;}
    [[nodiscard]] bool failed() const noexcept {return failed_;}
    void reset() noexcept { *this={}; }
private:
    [[nodiscard]] static constexpr bool validPort(std::uint16_t p) noexcept {
        return p==0xe22u || p==0xe20u || p==0xa20u || p==0xa22u ||
               p==0x620u || p==0x622u;
    }
    [[nodiscard]] bool fail() noexcept {
        failed_=true;ready_=false;blockOpen_=false;count_=0u;return false;
    }
    awe64::research_p114::OriginalDynarecBlockClock block_{};
    awe64::research::OriginalWavetableTimer timer_{};
    std::array<SourceClockedPort,kMaxPorts> outputs_{};
    std::size_t count_=0u,charges_=0u;
    std::uint64_t cpuTsc_=0u,finishedBlocks_=0u;
    bool ready_=false,failed_=false,blockOpen_=false;
};
}
