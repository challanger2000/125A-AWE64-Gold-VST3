// P003: original Win95 SBAWE.VXD MODE 0 uses 32 physical voices.
// Genuine source x86 LCOD+3ED4 loads [device+0x06] into ECX; the
// three-lane scan reaches physical voice 30 AND 31.
// Win95 CPU I/O at caller LCOD+3F13 VTFT writes E22 0x7E/0x7F
// and caller +3FCE DCYSUSV release E22 0xBE/0xBF in both private runs.
// Independent C++23 negative regression, not an EMU8000 DSP change.
#include "CreativeVxdAdditionalAllocatorModes.h"
#include "CreativeVxdMonoVoiceRanking.h"
#include <array>
#include <cstdint>
#include <cstdio>
using namespace awe64::creative;
int main() {
    std::array<OriginalWin95AllocatorVoiceSnapshot,32> voices{};
    for(auto &v:voices) {
        v.complete=true;
        v.internal_key_marker=0x2000u;
        v.stored_sample_end=0u;
        v.volume_target_hi16=100u;
    }
    OriginalWin95AllocatorDispatchInput input{};
    input.independently_verified_source_state=true;
    input.original_x86_dispatch_mode=0u;
    input.original_device_physical_capacity=32u;
    input.original_device_mode_count=32u;
    input.independently_verified_mode0_scan_ceiling=32u;
    // First true original idle voice is 30 (lane0: 0,3,...,30).
    voices[30].internal_key_marker=0x0ffeu;
    voices[30].volume_target_hi16=0u;
    auto r=candidateOriginalWin95VxdDispatchAllModes(voices,input);
    if(!r || r->voice_count!=1u || r->physical_voices[0]!=30u) {
        std::puts("P003_MODE0_ORIGINAL_PHYSICAL_30_NOT_SELECTED");
        return 42;
    }
    // Source +3FD8 gives first idle voice31 when voice30 excluded.
    voices[30].internal_key_marker=0x2000u;
    voices[31].internal_key_marker=0x0ffeu;
    voices[31].volume_target_hi16=0u;
    r=candidateOriginalWin95VxdDispatchAllModes(voices,input);
    if(!r || r->voice_count!=1u || r->physical_voices[0]!=31u) {
        std::puts("P003_MODE0_ORIGINAL_PHYSICAL_31_NOT_SELECTED");
        return 42;
    }
    // The originally verified scan ceiling cannot conflict with
    // authentic device+0x06. Refuse a fabricated 30 limit.
    input.independently_verified_mode0_scan_ceiling=30u;
    if(candidateOriginalWin95VxdDispatchAllModes(voices,input)) {
        std::puts("P003_MODE0_SOURCE_DEVICE_CAPACITY_CONFLICT_ACCEPTED");
        return 3;
    }
    input.independently_verified_mode0_scan_ceiling=32u;
    // Source +3FCE emits genuine DCYSUSV selector for physical31.
    auto writes=candidateOriginalWin95SelectedVoiceDcysusvWrites(
        OriginalWin95MonoSelection{31,
            OriginalWin95MonoSelectionReason::CompletelyIdle,0u});
    if(!writes || (*writes)[0].port!=0x0e22 ||
       (*writes)[0].value!=0x00bf ||
       (*writes)[1].port!=0x0a20 ||
       (*writes)[1].value!=0x00ff) {
        std::puts("P003_MODE0_ORIGINAL_PHYSICAL_31_RELEASE_PORTS_REJECTED");
        return 42;
    }
    auto vtft=candidateOriginalWin95AssignedVoiceVtftWrites(31u);
    if(!vtft || (*vtft)[0].port!=0x0e22 ||
       (*vtft)[0].value!=0x007f ||
       (*vtft)[1].port!=0x0620 ||
       (*vtft)[1].value!=0x0000ffffu ||
       (*vtft)[1].access_width_bits!=32u) {
        std::puts("P003_MODE0_ORIGINAL_PHYSICAL_31_VTFT_PORTS_REJECTED");
        return 42;
    }
    if(candidateOriginalWin95AssignedVoiceVtftWrites(32u))
        return 3;
    std::puts("P003_MODE0_32_PHYSICAL_VOICES_ORIGINAL_SOURCE_CONTRACT_PASS");
    return 0;
}
