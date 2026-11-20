#include "CreativeIdleVoiceAllocator.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(x) do{if(!(x)){std::cerr<<"P002_IDLE_ALLOCATOR_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
int main() {
    std::array<OriginalVxdIdleVoiceProbe,kOriginalCreativeMusicVoices> probes{};
    std::array<bool,kOriginalCreativeMusicVoices> occupied{};
    // Default fail closed: an unobserved VXD eligibility bit is NOT free.
    CHECK(!candidateOriginalWin95IdleMonoVoice(probes,occupied));
    for(auto& p:probes)p.driver_eligible=true;
    unsigned visited=0;
    for(unsigned lane=0;lane<3;++lane) {
        for(unsigned expected=lane;expected<kOriginalCreativeMusicVoices;expected+=3) {
            const auto next=candidateOriginalWin95IdleMonoVoice(probes,occupied);
            CHECK(next && *next==expected);
            occupied[expected]=true;
            ++visited;
        }
    }
    CHECK(visited==30);
    CHECK(!candidateOriginalWin95IdleMonoVoice(probes,occupied));
    // Original Win95 matrix included real physical 0,3,6,9,12.
    // Free the corresponding original hardware slot and verify reuse.
    occupied[3]=false;
    CHECK(candidateOriginalWin95IdleMonoVoice(probes,occupied)==3);
    occupied[3]=true;
    occupied[0]=false;
    CHECK(candidateOriginalWin95IdleMonoVoice(probes,occupied)==0);
    occupied[0]=true;
    // Original idle case needs both the internal assigned marker and
    // the actual EMU8000 target high16, not just a free C++ bool.
    occupied[6]=false;
    probes[6].driver_key_marker=0x1200;
    CHECK(!candidateOriginalWin95IdleMonoVoice(probes,occupied));
    probes[6].driver_key_marker=0x0ffe;
    probes[6].chip_voice_target_high16=0x2345;
    CHECK(!candidateOriginalWin95IdleMonoVoice(probes,occupied));
    probes[6].chip_voice_target_high16=0;
    probes[6].driver_eligible=false;
    CHECK(!candidateOriginalWin95IdleMonoVoice(probes,occupied));
    probes[6].driver_eligible=true;
    CHECK(candidateOriginalWin95IdleMonoVoice(probes,occupied)==6);
    CHECK(!candidateOriginalWin95IdleMonoVoice(probes,occupied,0));
    CHECK(!candidateOriginalWin95IdleMonoVoice(probes,occupied,31));
    // Limiting to the old 12-voice mode cannot return an index above 11.
    CHECK(candidateOriginalWin95IdleMonoVoice(probes,occupied,12)==6);
    occupied[6]=true;
    CHECK(!candidateOriginalWin95IdleMonoVoice(probes,occupied,12));
    std::cout<<"P002_ORIGINAL_SBAWE_VXD_IDLE_MONO_3_LANE_PASS"
             <<" probed=30 physical=0_3_6_9_12"
             <<" no_busy_voice_steal_guesses=TRUE\n";
}
