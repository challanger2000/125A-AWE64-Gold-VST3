#include "P005OriginalExecutedPortClock.hpp"
#include <cstdint>
#include <cstdio>
using awe64::creative::P005OriginalExecutedPortClock;
using awe64::creative::P005OriginalPentiumCompiledBlockClock;
using awe64::creative::P005OriginalP54cDecodedIntegerOp;
#define CHECK(x) do{if(!(x)){std::printf("P005_EXECUTED_PORT_CLOCK_FAIL line=%d\n",__LINE__);return 1;}}while(false)
int main() {
    constexpr std::uint32_t pc=0xc1019207u,phys=0x000a0000u;
    P005OriginalPentiumCompiledBlockClock compiled{};
    CHECK(compiled.startFreshVirtualEpoch());
    CHECK(compiled.beginTranslation(3u,1u,pc,phys));
    P005OriginalP54cDecodedIntegerOp inst{};
    inst.opcode=0x89u;inst.op_32=0x300u;inst.timing_word=3025u;
    CHECK(compiled.translationOpcode(inst));
    CHECK(compiled.finishTranslation());
    CHECK(compiled.compiledCost(3u,1u)==3025u);
    P005OriginalExecutedPortClock c{};
    CHECK(c.startFreshVirtualEpoch());
    CHECK(c.beginVerifiedCompiledBlock(compiled,3u,1u,pc,phys,10000,false));
    CHECK(c.executedCycles(3020));
    CHECK(c.originalOut16(0xe22u,0x0020u));
    CHECK(c.originalOut16(0xa20u,0x1234u));
    CHECK(c.chipFrame()==0u);
    CHECK(c.executedCycles(5));
    CHECK(c.originalOut16(0xe22u,0x0030u)); // SAME CPU BLOCK, NEXT FRAME
    CHECK(c.chipFrame()==1u);
    CHECK(c.endVerifiedBlock());
    CHECK(c.cpuTsc()==3025u && c.finishedBlocks()==1u);
    CHECK(c.beginVerifiedCompiledBlock(compiled,3u,1u,pc,phys,10000,false));
    CHECK(c.executedCycles(3025));
    CHECK(c.originalOut16(0x620u,0x5678u));
    CHECK(c.endVerifiedBlock());
    CHECK(c.cpuTsc()==6050u && c.chipFrame()==2u);
    const auto plan=c.orderedPortPlan();
    CHECK(plan.size()==4u);
    CHECK(plan[0].relative_frame==0u && plan[1].relative_frame==0u);
    CHECK(plan[2].relative_frame==1u && plan[3].relative_frame==2u);
    CHECK(plan[3].port==0x620u && plan[3].value==0x5678u);
    CHECK(!c.beginVerifiedCompiledBlock(compiled,3u,2u,pc,phys,10000,false));
    CHECK(c.failed() && c.orderedPortPlan().empty());
    c.reset();
    CHECK(c.startFreshVirtualEpoch());
    CHECK(!c.beginVerifiedCompiledBlock(compiled,3u,1u,pc,phys+4u,10000,false));
    CHECK(c.failed());
    c.reset();CHECK(c.startFreshVirtualEpoch());
    CHECK(c.beginVerifiedCompiledBlock(compiled,3u,1u,pc,phys,10000,false));
    CHECK(!c.originalOut16(0xe22u,0x0020u)); // no actual executed cost
    CHECK(c.failed() && c.orderedPortPlan().empty());
    std::puts("P005_ORIGINAL_LIVE_CPU_TSC_EMU_PORT_FRAMES_PASS synthetic_instruction_charges=TRUE intra_block_timer=TRUE source_driver_autonomy=NOT_PROVEN");
}
