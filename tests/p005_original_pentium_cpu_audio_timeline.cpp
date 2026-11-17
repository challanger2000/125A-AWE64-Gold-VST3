// End-to-end source arithmetic: decoded P54C opcode cycles -> original
// 32:32 timer processing at CPU BLOCK END -> source order EMU OUT frames.
// Synthetic decoded instructions and ports only; no original guest data.
#include "P005OriginalPentiumCpuAudioTimeline.hpp"
#include <cstdint>
#include <cstdio>
using awe64::creative::P005OriginalPentiumCpuAudioTimeline;
using awe64::creative::P005OriginalP54cDecodedIntegerOp;
#define CK(x) do {if(!(x)){std::printf("P005_CPU_AUDIO_TIMELINE_FAILED line=%d",__LINE__);return 1;}}while(false)
int main(){
    P005OriginalPentiumCpuAudioTimeline t{};
    CK(!t.beginCpuBlock());
    t.reset();
    CK(t.startFreshChipEpoch());
    CK(!t.sourceOrderedPortPlan().size());
    CK(t.beginCpuBlock());
    CK(!t.nativeOut16(0xe22u,0x20u)); // no decoded x86 OUT site
    CK(t.failed() && !t.sourceOrderedPortPlan().size());
    t.reset();
    CK(t.startFreshChipEpoch());
    P005OriginalP54cDecodedIntegerOp op{};
    op.opcode=0x89u; // synthetic original-decoder timing category
    op.timing_word=3u; // original NON-PAIR 3-cycle entry
    auto submit=[&](unsigned n){
        for(unsigned i=0u;i<n;++i)
            if(!t.decodedIntegerInstruction(op))return false;
        return true;
    };
    CK(t.beginCpuBlock());
    CK(submit(1000u)); // 3000 CPU ticks, short of first 3023 tick
    CK(t.nativeOut16(0xe22u,0x20u));
    CK(t.endCpuBlock());
    CK(t.cpuTsc()==3000u && t.chipFrame()==0u);
    CK(t.beginCpuBlock());
    CK(submit(10u)); // 3030: timer callbacks only at block end
    CK(t.nativeOut16(0xa20u,0x1234u));
    CK(t.chipFrame()==0u);
    CK(t.endCpuBlock());
    CK(t.cpuTsc()==3030u && t.chipFrame()==1u);
    CK(t.beginCpuBlock());
    CK(submit(1000u)); // 6030: below 2nd sample deadline
    CK(t.nativeOut16(0xe22u,0x40u));
    CK(t.endCpuBlock());
    CK(t.cpuTsc()==6030u && t.chipFrame()==1u);
    CK(t.beginCpuBlock());
    CK(submit(10u)); // 6060: crosses second fractional deadline
    CK(t.nativeOut16(0x620u,0x5678u));
    CK(t.endCpuBlock());
    CK(t.cpuTsc()==6060u && t.chipFrame()==2u);
    const auto ports=t.sourceOrderedPortPlan();
    CK(ports.size()==4u);
    CK(ports[0].relative_frame==0u && ports[1].relative_frame==0u);
    CK(ports[2].relative_frame==1u && ports[3].relative_frame==1u);
    CK(ports[0].port==0xe22u && ports[0].value==0x20u);
    CK(ports[3].port==0x620u && ports[3].value==0x5678u);
    // A failed block never releases its staged OUTs as a playable plan.
    CK(t.beginCpuBlock());
    CK(submit(1u));
    CK(!t.nativeOut16(0x999u,1u));
    CK(t.failed() && t.sourceOrderedPortPlan().empty());
    std::puts("P005_CPU_P54C_BLOCK_32_32_EMU_FRAME_TIMELINE_PASS source_derived=TRUE timestamps_replayed=FALSE actual_original_x86_sequence=NOT_YET_BOUND");
    return 0;
}
