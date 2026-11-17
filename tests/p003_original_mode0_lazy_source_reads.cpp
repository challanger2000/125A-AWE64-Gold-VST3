// P003 independent original-SBAWE.VXD mode-0 source-read regression.
// Original +3EA1,+3EEF,+3F00,+3F0E,+3F5F,+3F71,+3F7E,+3FD8
// Mode0 scans lanes 0,3,6... then 1,4,7... then 2,5,8...
// It never preflights an unvisited voice and may return before its
// VTFT, DCYSUSV or CCCA read. Original driver RAM inputs are verified.
#include "CreativeVxdMonoVoiceRanking.h"
#include <array>
#include <cstdint>
#include <cstdio>

using awe64::creative::OriginalWin95MonoVoiceSnapshot;
using awe64::creative::OriginalWin95MonoSelectionReason;
using awe64::creative::candidateOriginalWin95Mode0MonoVoice;
using awe64::creative::kOriginalCreativeMusicVoices;

template<class T>
void prove_vtft(T& v) {
    if constexpr (requires(T x){ x.vtft_read_verified; })
        v.vtft_read_verified=true;
}
template<class T>
void prove_ccca(T& v) {
    if constexpr (requires(T x){ x.ccca_read_verified; })
        v.ccca_read_verified=true;
}

int main() {
    std::array<OriginalWin95MonoVoiceSnapshot,kOriginalCreativeMusicVoices> voices{};
    // The original source returns before it reaches the other slots.
    voices[0].complete=true;
    voices[0].internal_key_marker=0x0ffe;
    voices[0].volume_target_hi16=0;
    auto result=candidateOriginalWin95Mode0MonoVoice(voices,3,32);
    if(!result || result->physical_voice!=0 ||
       result->reason!=OriginalWin95MonoSelectionReason::CompletelyIdle) {
        std::puts("P003_MODE0_UNVISITED_COMPLETE_PREFLIGHT_BUG_REPRODUCED");
        return 42;
    }

    // Ineligible marker and cached end 0: no chip access; lane 0
    // voice0 source skip, then voice1 in next lane is immediately idle.
    voices[0].complete=false;
    voices[0].internal_key_marker=0x2000;
    voices[0].stored_sample_end=0;
    voices[1].complete=true;
    voices[1].internal_key_marker=0x0ffe;
    voices[1].volume_target_hi16=0;
    result=candidateOriginalWin95Mode0MonoVoice(voices,3,32);
    if(!result || result->physical_voice!=1) {
        std::puts("P003_MODE0_HIGH_MARKER_SOURCE_SKIP_REJECTED");
        return 42;
    }

    // Original +3EEF blocked gate is checked before any chip read.
    voices[0].blocked_when_few_voices=true;
    voices[0].internal_key_marker=0x0ffe;
    result=candidateOriginalWin95Mode0MonoVoice(voices,3,2);
    if(!result || result->physical_voice!=1) {
        std::puts("P003_MODE0_BLOCKED_ROW_SKIPPED_LATE");
        return 42;
    }
    voices[0].blocked_when_few_voices=false;

    // Original +3F5F exits immediately after a proven zero VTFT
    // without asking for DCYSUSV or CCCA.
    voices[0].volume_target_hi16=0;
    voices[0].complete=false;
    prove_vtft(voices[0]);
    voices[0].stored_sample_end=100;
    result=candidateOriginalWin95Mode0MonoVoice(voices,3,32);
    if(!result || result->physical_voice!=0 ||
       result->reason!=OriginalWin95MonoSelectionReason::CompletelyIdle) {
        std::puts("P003_MODE0_VTFT_ONLY_IDLE_NOT_RECOGNIZED");
        return 42;
    }

    // For a high marker and cached sample-end, only CCCA is read.
    voices[0].internal_key_marker=0x2000;
    voices[0].stored_sample_end=100;
    voices[0].chip_current_address=100;
    prove_ccca(voices[0]);
    result=candidateOriginalWin95Mode0MonoVoice(voices,3,32);
    if(!result || result->physical_voice!=0 ||
       result->reason!=OriginalWin95MonoSelectionReason::SampleEndReached) {
        std::puts("P003_MODE0_CCCA_ONLY_SAMPLE_END_NOT_RECOGNIZED");
        return 42;
    }
    std::puts("P003_MODE0_ORIGINAL_LAZY_CHIP_READ_REGRESSION_PASS");
    return 0;
}
