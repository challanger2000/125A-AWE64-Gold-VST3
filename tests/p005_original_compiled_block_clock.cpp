#include "P005OriginalPentiumCompiledBlockClock.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
using awe64::creative::P005OriginalPentiumCompiledBlockClock;
using awe64::creative::P005OriginalP54cDecodedIntegerOp;
using awe64::creative::P005OriginalPentiumIntegerTiming;
using awe64::creative::P005UntimedChipOut16;
#define CHECK(v) do { if(!(v)) { \
  std::printf("P005_COMPILED_P54C_TEST_FAIL line=%d: %s\n",__LINE__,#v); \
  return 1; }} while(false)

static P005OriginalP54cDecodedIntegerOp op(std::uint64_t timing,
                                            std::uint64_t deps=0u) {
    P005OriginalP54cDecodedIntegerOp result{};
    result.timing_word=timing;
    result.dependency_word=deps;
    result.op_32=0x300u;
    result.opcode=0x89u;
    result.fetchdat=0u; // [EAX] for memory references
    return result;
}
int main() {
    P005OriginalPentiumCompiledBlockClock clock{};
    CHECK(clock.startFreshVirtualEpoch());
    constexpr std::uint32_t pc1=0xc1019207u,pc2=0xc101920au;
    constexpr std::uint32_t phys1=0x000a0000u,phys2=0x000a0010u;
    // Compile block 1 first: its source writes EAX.
    CHECK(clock.beginTranslation(1u,1u,pc1,phys1));
    CHECK(clock.translationOpcode(op(2u,1ull<<12u)));
    CHECK(clock.finishTranslation());
    CHECK(clock.compiledCost(1u,1u)==2u);
    CHECK(clock.lastTranslationRegisterMask()==1u);
    // Compile block 2 second: the original source's register mask
    // carries EAX across the translation boundary. It has an AGI
    // stall even if at runtime it happens to execute FIRST.
    CHECK(clock.beginTranslation(2u,2u,pc2,phys2));
    CHECK(clock.translationOpcode(
        op(2u,P005OriginalPentiumIntegerTiming::kModRm)));
    CHECK(clock.finishTranslation());
    CHECK(clock.compiledCost(2u,2u)==3u);
    CHECK(clock.lastTranslationRegisterMask()==0u);
    constexpr std::array<P005UntimedChipOut16,1u> selector{{
        {0xe22u,0x0020u}
    }};
    CHECK(clock.executeCompiledBlock(2u,2u,pc2,phys2,selector));
    CHECK(clock.cpuTsc()==3u);
    CHECK(clock.executeCompiledBlock(1u,1u,pc1,phys1,{}));
    CHECK(clock.executeCompiledBlock(2u,2u,pc2,phys2,selector));
    CHECK(clock.cpuTsc()==8u); // 3+2+3, NOT retranslated AGI
    CHECK(clock.executedBlocks()==3u);
    CHECK(clock.compiledCost(2u,2u)==3u);
    CHECK(clock.lastTranslationRegisterMask()==0u);
    // A long, SOURCE-TRANSLATED non-pairable integer timing entry
    // advances the CPU almost to the first 44.1kHz sample boundary.
    CHECK(clock.beginTranslation(3u,3u,0xc1019210u,0xa0020u));
    CHECK(clock.translationOpcode(op(3013u)));
    CHECK(clock.finishTranslation());
    CHECK(clock.executeCompiledBlock(
        3u,3u,0xc1019210u,0xa0020u,{}));
    CHECK(clock.cpuTsc()==3021u && clock.chipFrame()==0u);
    constexpr std::array<P005UntimedChipOut16,1u> data{{
        {0xa20u,0x1234u}
    }};
    CHECK(clock.executeCompiledBlock(2u,2u,pc2,phys2,data));
    CHECK(clock.cpuTsc()==3024u && clock.chipFrame()==1u);
    CHECK(clock.executeCompiledBlock(1u,1u,pc1,phys1,selector));
    CHECK(clock.cpuTsc()==3026u && clock.chipFrame()==1u);
    const auto result=clock.orderedPortPlan();
    CHECK(result.size()==4u);
    CHECK(result[0].relative_frame==0u);
    CHECK(result[1].relative_frame==0u);
    CHECK(result[2].relative_frame==0u);
    CHECK(result[3].relative_frame==1u);
    CHECK(result[2].port==0xa20u && result[2].value==0x1234u);
    // Recompile SAME physical cache slot at a later epoch. The
    // original old-Dynarec block generation changes even with
    // repeated guest PC; old stored generation is INVALIDATED.
    CHECK(clock.beginTranslation(2u,4u,pc2,phys2));
    CHECK(clock.translationOpcode(op(5u)));
    CHECK(clock.finishTranslation());
    CHECK(clock.compiledCost(2u,2u)==0u);
    CHECK(clock.compiledCost(2u,4u)==5u);
    CHECK(clock.executeCompiledBlock(2u,4u,pc2,phys2,selector));
    CHECK(clock.cpuTsc()==3031u);
    CHECK(clock.executedBlocks()==7u);
    // Reject original JIT generation/slot aliasing atomically.
    CHECK(!clock.executeCompiledBlock(2u,2u,pc2,phys2,data));
    CHECK(clock.failed() && clock.orderedPortPlan().empty());
    clock.reset();
    CHECK(clock.startFreshVirtualEpoch());
    CHECK(clock.beginTranslation(1u,1u,pc1,phys1));
    CHECK(clock.translationOpcode(op(2u)));
    CHECK(clock.finishTranslation());
    // An interpreter-first-run is NOT equivalent to JIT code().
    CHECK(!clock.executeOriginalInterpreterWithoutModel());
    CHECK(clock.failed() && clock.orderedPortPlan().empty());
    std::puts("P005_ORIGINAL_P54C_COMPILE_EPOCH_CACHED_JIT_EXECUTION_PASS synthetic_only=TRUE stale_generation_rejected=TRUE interpreter_distinguished=TRUE no_original_guest_TSC_claim=TRUE");
    return 0;
}
