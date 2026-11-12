#include "CreativeVxdMonoVoiceRanking.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(x) do{if(!(x)){std::cerr<<"P002_VXD_MONO_PRIORITY_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
int main() {
    using Reason=OriginalWin95MonoSelectionReason;
    std::array<OriginalWin95MonoVoiceSnapshot,30> slots{};
    CHECK(!candidateOriginalWin95Mode0MonoVoice(slots,30,1));
    for(auto& v:slots)v.complete=true;
    auto chosen=candidateOriginalWin95Mode0MonoVoice(slots,30,1);
    CHECK(chosen && chosen->physical_voice==0 && chosen->reason==Reason::CompletelyIdle);
    slots[0].internal_key_marker=0x1100;slots[0].volume_target_hi16=0x4000;
    chosen=candidateOriginalWin95Mode0MonoVoice(slots,30,1);
    CHECK(chosen && chosen->physical_voice==3 && chosen->reason==Reason::CompletelyIdle);
    // All busy and equal-score: the VXD uses <= and selects the last
    // slot of its 0,3,... then 1,4,... then 2,5,... three-pass scan.
    for(auto& v:slots){
        v.internal_key_marker=0x1100;
        v.volume_target_hi16=0x1000;
        v.envelope_status_reg=0x8000;
        v.stored_sample_end=0;
        v.chip_current_address=0;
    }
    chosen=candidateOriginalWin95Mode0MonoVoice(slots,30,1);
    CHECK(chosen && chosen->physical_voice==29);
    CHECK(chosen->reason==Reason::LowestEnvelopePriority);
    CHECK(chosen->priority==0x2200);
    // A voice with the low 0x1000 marker bit clear incurs only 0x200,
    // not 0x1200, so it wins absent a completed sample.
    slots[9].internal_key_marker=0x0100;
    chosen=candidateOriginalWin95Mode0MonoVoice(slots,30,1);
    CHECK(chosen && chosen->physical_voice==9 && chosen->priority==0x1200);
    // An un-released envelope status adds 0x1300 (tested via high byte).
    slots[9].envelope_status_reg=0;
    chosen=candidateOriginalWin95Mode0MonoVoice(slots,30,1);
    CHECK(chosen && chosen->physical_voice==29 && chosen->priority==0x2200);
    // But sample pointer reached its original driver's end marker:
    // this is an immediate selection branch regardless of priority score.
    slots[9].stored_sample_end=100;slots[9].chip_current_address=130;
    chosen=candidateOriginalWin95Mode0MonoVoice(slots,30,1);
    CHECK(chosen && chosen->physical_voice==9 &&
          chosen->reason==Reason::SampleEndReached);
    // No completed samples, internal marker >=0x2000 excludes priority,
    // but an actual completed cursor may still nominate that voice.
    for(auto& v:slots){
        v.internal_key_marker=0x2000;
        v.stored_sample_end=0;v.chip_current_address=0;
    }
    CHECK(!candidateOriginalWin95Mode0MonoVoice(slots,30,1));
    slots[3].stored_sample_end=200;slots[3].chip_current_address=201;
    chosen=candidateOriginalWin95Mode0MonoVoice(slots,30,1);
    CHECK(chosen && chosen->physical_voice==3 &&
          chosen->reason==Reason::SampleEndReached);
    slots[3].stored_sample_end=202;
    CHECK(!candidateOriginalWin95Mode0MonoVoice(slots,30,1));
    // VXD additional low-mode gate: blocked slots are skipped only
    // while its live voice-count field <=2.
    for(auto& v:slots){
        v.internal_key_marker=0x0ffe;
        v.volume_target_hi16=0;
        v.blocked_when_few_voices=false;
    }
    slots[0].blocked_when_few_voices=true;
    CHECK(candidateOriginalWin95Mode0MonoVoice(slots,30,2)->physical_voice==3);
    CHECK(candidateOriginalWin95Mode0MonoVoice(slots,30,3)->physical_voice==0);
    // Guard all inputs: never allocate musical refresh voices 30/31;
    // do not silently read a missing hardware sample.
    CHECK(!candidateOriginalWin95Mode0MonoVoice(slots,0,3));
    CHECK(!candidateOriginalWin95Mode0MonoVoice(slots,31,3));
    slots[27].complete=false;
    CHECK(!candidateOriginalWin95Mode0MonoVoice(slots,30,3));
    slots[27].complete=true;
    CHECK(candidateOriginalWin95Mode0MonoVoice(slots,30,3)->physical_voice==0);
    std::cout<<"P002_REAL_VXD_MODE0_MONO_SELECTION_PRIORITY_PASS"
             <<" idle=TRUE busy_scoring=TRUE sample_end=TRUE"
             <<" original_voice_steal_IO_and_PCM_UNVERIFIED=TRUE\n";
}
