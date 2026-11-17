// P003 original Win95 SBAWE.VXD: physical voices 30 and 31 are not
// mere addressable registers. Both were SELECTED by original +3FCE,
// including selection after full three-lane scan ended on voice29.
// These are source-derived synthetic regression vectors, NOT replayed PCM.
#include "CreativeVxdAdditionalAllocatorModes.h"
#include <array>
#include <cstdint>
#include <cstdio>
using namespace awe64::creative;
static std::array<OriginalWin95AllocatorVoiceSnapshot,32> voices() {
    std::array<OriginalWin95AllocatorVoiceSnapshot,32> out{};
    for(auto& v:out){
        v.complete=true;
        v.internal_key_marker=0x0ffeu;
        v.volume_target_hi16=3000u; // all occupied (not idle)
        v.envelope_status_reg=0x8000u;
        v.stored_sample_end=0;
        v.chip_current_address=0;
    }
    return out;
}
static OriginalWin95AllocatorDispatchInput device() {
    OriginalWin95AllocatorDispatchInput in{};
    in.independently_verified_source_state=true;
    in.original_x86_dispatch_mode=0u;
    in.original_device_physical_capacity=32u;
    in.original_device_mode_count=32u;
    in.independently_verified_mode0_scan_ceiling=32u;
    return in;
}
static bool choose(const std::array<OriginalWin95AllocatorVoiceSnapshot,32>& v,
                   std::uint8_t target,std::uint32_t priority) {
    const auto chosen=candidateOriginalWin95VxdDispatchAllModes(v,device());
    if(!chosen || chosen->voice_count!=1u ||
       chosen->physical_voices[0]!=target ||
       chosen->priority!=priority) {
        std::printf("P003_MODE0_PHYSICAL_BUSY_MISMATCH want=%u score=%u got=%d\n",
                    target,priority,chosen?int(chosen->physical_voices[0]):-1);
        return false;
    }
    return true;
}
int main() {
    auto v=voices();

    // Genuine 3-pass traversal: voice30 in lane0, voice31 in lane1,
    // voice29 checked last in lane2; equality replaces earlier choice.
    v[30].volume_target_hi16=91u;
    v[31].volume_target_hi16=90u;
    if(!choose(v,31u,90u)) return 1;
    v[30].volume_target_hi16=89u;
    if(!choose(v,30u,89u)) return 2;
    v[30].volume_target_hi16=90u; // both 90: voice31 visited later
    if(!choose(v,31u,90u)) return 3;

    // Original cached sample-end branch can also nominate each
    // physical voice (before finishing the three-lane scan).
    v=voices();
    v[30].stored_sample_end=100u;
    v[30].chip_current_address=101u;
    {
        const auto result=candidateOriginalWin95VxdDispatchAllModes(v,device());
        if(!result || result->physical_voices[0]!=30u || result->voice_count!=1u)
            return 4;
    }
    v=voices();
    v[31].stored_sample_end=100u;
    v[31].chip_current_address=100u;
    {
        const auto result=candidateOriginalWin95VxdDispatchAllModes(v,device());
        if(!result || result->physical_voices[0]!=31u || result->voice_count!=1u)
            return 5;
    }

    // Source +3F5F short circuit: when 30 is definitely idle
    // before lane1, unknown voice31 must NOT be read at all.
    v=voices();
    v[30].volume_target_hi16=0u;
    v[31].complete=false;
    {
        const auto result=candidateOriginalWin95VxdDispatchAllModes(v,device());
        if(!result || result->physical_voices[0]!=30u || result->voice_count!=1u)
            return 6;
    }

    // Conversely, no valid CCCA can be invented for voice30 when
    // its genuine sample end is positive and the scan reaches it.
    v=voices();
    v[30].stored_sample_end=100u;
    v[30].complete=false;
    if(candidateOriginalWin95VxdDispatchAllModes(v,device()))
        return 7;
    std::puts("P003_ORIGINAL_MODE0_HIGH_VOICE_BUSY_TIE_SAMPLEEND_PASS");
    return 0;
}
