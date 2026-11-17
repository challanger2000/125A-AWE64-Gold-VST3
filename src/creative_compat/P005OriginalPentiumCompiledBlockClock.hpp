#pragma once
// P005: source-driven Pentium old-Dynarec COMPILE vs EXECUTE split.
//
// Original 86Box v6.0-AWE32-0.93 codegen_timing_pentium.c accounts
// U/V/AGI while TRANSLATING a block, and its regmask_modified persists
// across translations. src/cpu/386_dynarec.c executes a previously
// translated generation via code() and may reuse the block repeatedly.
// Never re-run compiler AGI accounting for every runtime invocation.
//
// Requirements before an actual Win95 Creative-equivalence claim:
//   verified original guest code decoding, x86 branches and block
//   generation order, interpreter-first-execution timing, full I/O and
//   original CPU/EMU clock phase. These inputs are NOT available here.
// This bounded, allocation-free compiled-block clock is a source-based
// integration research component, NOT an enabled product audio mode.
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
struct P005UntimedChipOut16 final {
    std::uint16_t port=0u;
    std::uint16_t value=0u;
};

class P005OriginalPentiumCompiledBlockClock final {
public:
    static constexpr std::size_t kMaxSlots=128u;
    static constexpr std::size_t kMaxOutputs=
        SourceClockedPortPlayer::kMaxEvents;
    static constexpr std::uint64_t kFrameLimit=44100u*30u;

    [[nodiscard]] bool startFreshVirtualEpoch() noexcept {
        if(ready_ || failed_)return false;
        constexpr auto latch=awe64::research::
            original86BoxWavetableLatch32_32(133333333u);
        static_assert(latch==12985539821609ull);
        if(!timer_.init(latch>>32u,static_cast<std::uint32_t>(latch),
                        latch,0u,0u))return fail();
        ready_=true;
        return true;
    }

    // Exact original translation order must be supplied by an
    // independent, original-source-authenticated guest x86 decoder.
    // epoch is monotonic, slot is the original codeblock cache slot.
    [[nodiscard]] bool beginTranslation(
        std::size_t slot,std::uint64_t epoch,
        std::uint32_t guestPc,std::uint32_t physicalAddress) noexcept {
        if(!ready_ || failed_ || translating_ || slot>=kMaxSlots ||
           epoch==0u || epoch<=lastEpoch_ ||
           !translator_.beginBlock())return fail();
        translation_={slot,epoch,guestPc,physicalAddress};
        translating_=true;
        translationInstructions_=0u;
        return true;
    }
    [[nodiscard]] bool translationOpcode(
        const P005OriginalP54cDecodedIntegerOp& opcode) noexcept {
        if(!translating_ || failed_ || !translator_.append(opcode))
            return fail();
        ++translationInstructions_;
        return true;
    }
    [[nodiscard]] bool finishTranslation() noexcept {
        if(!translating_ || failed_ || translationInstructions_==0u ||
           !translator_.endBlock())return fail();
        const auto cycles=translator_.lastBlockCycles();
        if(cycles<=0 || cycles>10'000'000)return fail();
        blocks_[translation_.slot]={
            true,translation_.epoch,translation_.guestPc,
            translation_.physicalAddress,
            static_cast<std::uint32_t>(cycles)
        };
        lastEpoch_=translation_.epoch;
        translating_=false;
        return true;
    }

    // Execute ALREADY COMPILED JIT code() generation. Uses recorded
    // compiler block cost without re-running the integer/AGI model.
    // Source I/O is supplied as untimed words, NEVER as replayed TSCs.
    // First-time original interpreter executions are not this API.
    [[nodiscard]] bool executeCompiledBlock(
        std::size_t slot,std::uint64_t epoch,
        std::uint32_t guestPc,std::uint32_t physicalAddress,
        std::span<const P005UntimedChipOut16> untimedOuts) noexcept {
        if(!ready_ || failed_ || translating_ || slot>=kMaxSlots ||
           !blocks_[slot].valid || blocks_[slot].epoch!=epoch ||
           blocks_[slot].guestPc!=guestPc ||
           blocks_[slot].physicalAddress!=physicalAddress ||
           untimedOuts.size()>kMaxOutputs-outputCount_ ||
           untimedOuts.size()>kMaxOutputs ||
           cpuTsc_>std::numeric_limits<std::uint64_t>::max()-
               blocks_[slot].cycleCost ||
           timer_.frame()>kFrameLimit)
            return fail();
        // Original port callbacks run BEFORE timer_process() after
        // this CPU block, so all their chip-frame stamps are taken
        // from the current wavetable_pos_global.
        for(const auto& out:untimedOuts)
            if(!validChipPort(out.port))return fail();
        const auto currentFrame=timer_.frame();
        if(currentFrame>std::numeric_limits<std::uint32_t>::max())
            return fail();
        const auto nextTsc=cpuTsc_+blocks_[slot].cycleCost;
        if(!timer_.onCpuBlockEnd(nextTsc) ||
           timer_.frame()>kFrameLimit)return fail();
        for(const auto& out:untimedOuts)
            outputs_[outputCount_++]={
                static_cast<std::uint32_t>(currentFrame),
                out.port,out.value
            };
        cpuTsc_=nextTsc;
        ++executedBlocks_;
        return true;
    }

    // The original old-Dynarec first-run interpreter is distinct
    // from later code(). Do not silently assign JIT compiler costs
    // to that execution path.
    [[nodiscard]] bool executeOriginalInterpreterWithoutModel() noexcept {
        return fail();
    }
    [[nodiscard]] std::span<const SourceClockedPort>
    orderedPortPlan() const noexcept {
        if(!ready_ || failed_ || translating_)return {};
        return {outputs_.data(),outputCount_};
    }
    [[nodiscard]] std::uint64_t cpuTsc() const noexcept {return cpuTsc_;}
    [[nodiscard]] std::uint64_t chipFrame() const noexcept {
        return timer_.frame();
    }
    [[nodiscard]] std::uint64_t executedBlocks() const noexcept {
        return executedBlocks_;
    }
    [[nodiscard]] std::uint32_t lastTranslationRegisterMask() const noexcept {
        return translator_.lastModifiedRegisterMask();
    }
    [[nodiscard]] std::uint64_t newestCompiledEpoch() const noexcept {
        return lastEpoch_;
    }
    [[nodiscard]] bool sourceGenerationMatches(
        std::size_t slot,std::uint64_t epoch,std::uint32_t pc,
        std::uint32_t phys) const noexcept {
        return ready_ && !failed_ && !translating_ &&
               slot<kMaxSlots && blocks_[slot].valid &&
               blocks_[slot].epoch==epoch &&
               blocks_[slot].guestPc==pc &&
               blocks_[slot].physicalAddress==phys;
    }
    [[nodiscard]] std::uint32_t compiledCost(
        std::size_t slot,std::uint64_t epoch) const noexcept {
        if(slot>=kMaxSlots || !blocks_[slot].valid ||
           blocks_[slot].epoch!=epoch)return 0u;
        return blocks_[slot].cycleCost;
    }
    [[nodiscard]] bool ready() const noexcept {
        return ready_ && !failed_;
    }
    [[nodiscard]] bool failed() const noexcept {return failed_;}
    void reset() noexcept { *this={}; }

private:
    struct Translation {
        std::size_t slot=0u;
        std::uint64_t epoch=0u;
        std::uint32_t guestPc=0u;
        std::uint32_t physicalAddress=0u;
    };
    struct CompiledBlock {
        bool valid=false;
        std::uint64_t epoch=0u;
        std::uint32_t guestPc=0u;
        std::uint32_t physicalAddress=0u;
        std::uint32_t cycleCost=0u;
    };
    [[nodiscard]] static constexpr bool validChipPort(
        std::uint16_t port) noexcept {
        return port==0xe22u || port==0xe20u ||
               port==0xa20u || port==0xa22u ||
               port==0x620u || port==0x622u;
    }
    [[nodiscard]] bool fail() noexcept {
        failed_=true;
        ready_=false;
        translating_=false;
        outputCount_=0u;
        return false;
    }
    P005OriginalPentiumIntegerTiming translator_{};
    awe64::research::OriginalWavetableTimer timer_{};
    std::array<CompiledBlock,kMaxSlots> blocks_{};
    std::array<SourceClockedPort,kMaxOutputs> outputs_{};
    Translation translation_{};
    std::uint64_t lastEpoch_=0u,cpuTsc_=0u,executedBlocks_=0u;
    std::size_t outputCount_=0u,translationInstructions_=0u;
    bool ready_=false,failed_=false,translating_=false;
};
} // namespace awe64::creative
