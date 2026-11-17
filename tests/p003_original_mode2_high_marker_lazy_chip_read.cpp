// P003 independent Original Win95 SBAWE.VXD mode-2 negative regression.
// Source-specific claim, NOT a production driver port or PCM parity claim.
//
// At LCOD+0x41D9 the original source does "cmp marker,0x2000; jae +424F".
// At +424F it checks the CACHED sample_end dword from the voice row.
// If that value is 0, it jumps +4269 (next voice). There is NO VTFT,
// DCYSUSV or CCCA read for this ineligible row.
// Hence such a voice need NOT have a complete live chip snapshot.
//
// A full-snapshot preflight before checking the marker+sample_end gates
// incorrectly returns NO-RESULT and masks a later genuine original voice.
//
#include "CreativeVxdAdditionalAllocatorModes.h"
#include <array>
#include <cstdio>
#include <cstdint>

using awe64::creative::OriginalWin95AllocatorVoiceSnapshot;
using awe64::creative::kOriginalWin95PhysicalVoiceCapacity;
using awe64::creative::candidateOriginalWin95Mode2Range;
using awe64::creative::OriginalWin95MonoSelectionReason;

int main() {
    std::array<OriginalWin95AllocatorVoiceSnapshot,
               kOriginalWin95PhysicalVoiceCapacity> voices{};
    // Voice 0 is SOURCE-SKIPPABLE: no original chip read is executed.
    voices[0].complete = false;
    voices[0].blocked_when_few_voices = false;
    voices[0].internal_key_marker = 0x2000u;
    voices[0].stored_sample_end = 0u; // original EBX:[0] == 0
    // Voice 1 is the first genuinely read/idle original candidate.
    voices[1].complete = true;
    voices[1].blocked_when_few_voices = false;
    voices[1].internal_key_marker = 0x0ffeu;
    voices[1].volume_target_hi16 = 0u;
    voices[1].stored_sample_end = 0u;
    const auto selection=candidateOriginalWin95Mode2Range(
        voices,(2u<<8u)|0u,2u,32u);
    if(!selection) {
        std::puts("P003_MODE2_INELIGIBLE_MARKER_CAUSES_FALSE_NO_RESULT");
        return 42;
    }
    std::printf("P003_MODE2_SELECTED_VOICE=%u priority=%u\n",
        static_cast<unsigned>(selection->physical_voice),
        selection->priority);
    if(selection->physical_voice == 1u &&
       selection->reason == OriginalWin95MonoSelectionReason::CompletelyIdle &&
       selection->priority == 0u) {
        std::puts("P003_MODE2_MARKER_SKIP_SOURCE_CONTRACT_PASS");
        return 0;
    }
    std::puts("P003_MODE2_UNEXPECTED_RESULT");
    return 3;
}
