// Source P119: translation-pending delta != executed runtime signed ADD.
// Synthetic only. Independent guest JIT execution not yet supplied.
#include "P005OriginalJitDeferredCycleEmitter.hpp"
#include "P005OriginalExecutedPortClock.hpp"
#include <cstdint>
#include <cstdio>
#include <limits>
using awe64::creative::P005OriginalJitDeferredCycleEmitter;
using awe64::creative::P005OriginalEmittedCycleAdd;
using awe64::creative::P005OriginalExecutedPortClock;
using awe64::creative::P005OriginalPentiumCompiledBlockClock;
using awe64::creative::P005OriginalP54cDecodedIntegerOp;
using awe64::research_p114::OriginalDynarecBlockClock;
#define CHECK(x) do{if(!(x)){ \
    std::printf("P005_P119_TEST_FAIL line=%d\n",__LINE__);return 1; \
}}while(false)
int main(){
    P005OriginalJitDeferredCycleEmitter e{};
    CHECK(e.beginTranslation(1u));
    CHECK(e.accumulate(-3000));
    CHECK(e.accumulate(-20));
    CHECK(e.pendingCompilerDelta()==-3020);
    P005OriginalEmittedCycleAdd first{},branch{},zero{};
    CHECK(e.flush(first));
    CHECK(first.emitted && first.signed_delta==-3020 &&
          first.generation==1u && first.ordinal==1u);
    CHECK(e.pendingCompilerDelta()==0);
    CHECK(e.accumulate(+10));
    CHECK(e.flush(branch) && branch.emitted && branch.signed_delta==10);
    CHECK(e.endTranslation() && e.lastGeneration()==1u);
    CHECK(e.beginTranslation(2u));
    CHECK(e.flush(zero) && !zero.emitted && zero.signed_delta==0);
    CHECK(e.accumulate(-19));
    CHECK(e.resetWithoutEmission() && e.pendingCompilerDelta()==0);
    CHECK(e.endTranslation());
    // Compiler pending and even a generated ADD do not change the
    // native clock until the original JIT executes that ADD.
    CHECK(e.beginTranslation(3u));
    CHECK(e.accumulate(-100));
    OriginalDynarecBlockClock clock{};
    std::uint64_t tsc=0u;
    CHECK(clock.begin_block(10000,0u,true));
    CHECK(clock.port_timestamp(tsc) && tsc==0u);
    CHECK(e.pendingCompilerDelta()==-100);
    CHECK(clock.port_timestamp(tsc) && tsc==0u);
    CHECK(e.flush(zero) && zero.emitted && zero.signed_delta==-100);
    CHECK(clock.port_timestamp(tsc) && tsc==0u);
    CHECK(clock.apply_source_emitted_cycles_delta(zero.signed_delta));
    CHECK(clock.port_timestamp(tsc) && tsc==100u);
    CHECK(clock.apply_source_emitted_cycles_delta(+10));
    CHECK(clock.port_timestamp(tsc) && tsc==100u);
    CHECK(clock.finish_block(tsc) && tsc==100u);
    CHECK(e.endTranslation());
    // Separate USE_ACYCS channel must not be double-counted.
    OriginalDynarecBlockClock aux{};
    CHECK(aux.begin_block(1000,1000u,true));
    CHECK(aux.apply_source_emitted_cycles_delta(-3));
    CHECK(aux.set_acycs(2));
    CHECK(aux.port_timestamp(tsc) && tsc==1005u);
    CHECK(aux.finish_block(tsc) && tsc==1005u);
    // Signed emitted writes flow through existing P005 original
    // physical-port timestamps when marked actually executed.
    P005OriginalPentiumCompiledBlockClock cat{};
    CHECK(cat.startFreshVirtualEpoch());
    CHECK(cat.beginTranslation(1u,1u,0xc1019207u,0xa0000u));
    P005OriginalP54cDecodedIntegerOp ins{};
    ins.timing_word=3u;ins.opcode=0x89u;ins.op_32=0x300u;
    CHECK(cat.translationOpcode(ins));
    CHECK(cat.finishTranslation());
    P005OriginalExecutedPortClock ports{};
    CHECK(ports.startFreshVirtualEpoch());
    CHECK(ports.beginVerifiedCompiledBlock(
        cat,1u,1u,0xc1019207u,0xa0000u,10000,true));
    CHECK(ports.originalExecutedJitFlush(-3020));
    CHECK(ports.originalOut16(0xe22u,0x20u));
    CHECK(ports.originalExecutedJitFlush(-10));
    CHECK(ports.originalOut16(0xa20u,0x1234u));
    CHECK(ports.endVerifiedBlock());
    const auto plan=ports.orderedPortPlan();
    CHECK(plan.size()==2u);
    CHECK(plan[0].relative_frame==0u && plan[1].relative_frame==1u);
    CHECK(ports.cpuTsc()==3030u);
    // Prevent incomplete, overflowed and stale original generations.
    e.reset();
    CHECK(e.beginTranslation(1u) && e.accumulate(-7));
    CHECK(!e.endTranslation() && e.failed());
    e.reset();
    CHECK(e.beginTranslation(5u));
    CHECK(e.accumulate(std::numeric_limits<std::int32_t>::min()));
    CHECK(!e.accumulate(-1) && e.failed());
    e.reset();
    CHECK(e.beginTranslation(8u));
    CHECK(e.flush(zero) && e.endTranslation());
    CHECK(!e.beginTranslation(8u) && e.failed());
    std::puts("P005_P119_ORIGINAL_JIT_DEFERRED_CYCLES_PASS compiler_pending_not_runtime=TRUE signed_branch_refund=TRUE acycs_separate=TRUE source_executed_x86=NOT_VERIFIED");
    return 0;
}
