// P003: original SBAWE.VXD LCOD+0x4029/+0x403E/+0x4111 
// Independent negative regression for blocked upper neighbor. This file
// does NOT port the driver or alter production code. It verifies an
// ORIGINAL SOURCE control flow rule against the candidate C++ implementation.
//
// Scenario: capacity 4, mode count 2, voice 3 is blocked; voices 0/1/2
// are unblocked and complete, VTFT highwords 900/100/10, marker 0x0FFE.
// The ORIGINAL checks voice2's neighboring voice3 at [EBX+0x2D]
// BEFORE VTFT, resets previous score to 0xFFFFF, and so excludes 1+2.
// Valid selected pair is 0+1, with score 900+100+2*900=2800.
// A current developer candidate copied voice2 score=10 through the
// blocked neighbor gate, incorrectly choosing pair 1+2 at score 310.
#include "CreativeVxdAdditionalAllocatorModes.h"
#include <array>
#include <cstdint>
#include <cstdio>

using awe64::creative::OriginalWin95AllocatorVoiceSnapshot;
using awe64::creative::kOriginalWin95PhysicalVoiceCapacity;
using awe64::creative::candidateOriginalWin95Mode1Pair;

int main() {
    std::array<OriginalWin95AllocatorVoiceSnapshot,
               kOriginalWin95PhysicalVoiceCapacity> voices{};
    for (auto &v: voices) {
        v.complete=true;
        v.internal_key_marker=0x0ffeu;
        v.volume_target_hi16=500u;
        v.envelope_status_reg=0x8000u;
        v.stored_sample_end=0u;
        v.chip_current_address=0u;
        v.blocked_when_few_voices=false;
        v.pair_extra_penalty=false;
    }
    voices[0].volume_target_hi16=900u;
    voices[1].volume_target_hi16=100u;
    voices[2].volume_target_hi16=10u;
    voices[3].blocked_when_few_voices=true;
    voices[3].complete=false; // x86 skips this chip read.
    const auto selected=candidateOriginalWin95Mode1Pair(voices,4u,2u);
    if(!selected) {
        std::puts("P003_BLOCKED_NEIGHBOR_NO_SELECTION");
        return 3;
    }
    std::printf("P003_ORIGINAL_NEIGHBOR_TEST selected=%u,%u priority=%u\n",
               static_cast<unsigned>(selected->first_voice),
               static_cast<unsigned>(selected->second_voice),
               selected->priority);
    if(selected->first_voice==0u && selected->second_voice==1u &&
       selected->priority==2800u) {
        std::puts("P003_BLOCKED_NEIGHBOR_ORIGINAL_BRANCH_PASS");
        return 0;
    }
    if(selected->first_voice==1u && selected->second_voice==2u &&
       selected->priority==310u) {
        std::puts("P003_BLOCKED_NEIGHBOR_WRONG_PAIR_REPRODUCED");
        return 42;
    }
    std::puts("P003_BLOCKED_NEIGHBOR_UNEXPECTED_RESULT");
    return 4;
}
