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
    // Actual original VXD selection exits via 0x3FBA -> its word writer
    // at 0x3CB1, selecting hardware DATA1/voice DCYSUSV with 0x00FF.
    const auto reset=candidateOriginalWin95SelectedVoiceDcysusvWrites(
        OriginalWin95MonoSelection{3,Reason::CompletelyIdle,0});
    CHECK(reset && (*reset)[0].port==0xE22 &&
          (*reset)[0].value==0xA3 &&
          (*reset)[0].access_width_bits==8 &&
          (*reset)[1].port==0xA20 &&
          (*reset)[1].value==0x00FF &&
          (*reset)[1].access_width_bits==16);
    const auto resetBusy=candidateOriginalWin95SelectedVoiceDcysusvWrites(
        OriginalWin95MonoSelection{29,Reason::LowestEnvelopePriority,0x2200});
    CHECK(resetBusy && (*resetBusy)[0].value==0xBD &&
          (*resetBusy)[0].access_width_bits==8 &&
          (*resetBusy)[1].access_width_bits==16);
    const auto resetFinished=candidateOriginalWin95SelectedVoiceDcysusvWrites(
        OriginalWin95MonoSelection{9,Reason::SampleEndReached,0});
    CHECK(resetFinished && (*resetFinished)[0].value==0xA9);
    CHECK(!candidateOriginalWin95SelectedVoiceDcysusvWrites(
        OriginalWin95MonoSelection{30,Reason::CompletelyIdle,0}));
    CHECK(!candidateOriginalWin95SelectedVoiceDcysusvWrites(
        OriginalWin95MonoSelection{31,Reason::LowestEnvelopePriority,0}));
    CHECK(!candidateOriginalWin95SelectedVoiceDcysusvWrites(
        OriginalWin95MonoSelection{2,
           static_cast<Reason>(255),0}));
    // After the original selection exit, SBAWE.VXD +0x3DDB
    // writes a full 32-bit 0000FFFF into DATA0/VTFT, not a 16-bit A20 word.
    const auto vtft0=candidateOriginalWin95AssignedVoiceVtftWrites(0);
    const auto vtft3=candidateOriginalWin95AssignedVoiceVtftWrites(3);
    const auto vtft29=candidateOriginalWin95AssignedVoiceVtftWrites(29);
    CHECK(vtft0 && (*vtft0)[0].port==0xE22 &&
          (*vtft0)[0].value==0x60 && (*vtft0)[0].access_width_bits==8);
    CHECK((*vtft0)[1].port==0x620 && (*vtft0)[1].value==0x0000ffffu &&
          (*vtft0)[1].access_width_bits==32);
    CHECK(vtft3 && (*vtft3)[0].value==0x63);
    CHECK(vtft29 && (*vtft29)[0].value==0x7d);
    CHECK(!candidateOriginalWin95AssignedVoiceVtftWrites(30));
    CHECK(!candidateOriginalWin95AssignedVoiceVtftWrites(31));
    CHECK(!candidateOriginalWin95AssignedVoiceVtftWrites(255));
    std::cout<<"P002_REAL_VXD_MODE0_MONO_SELECTION_PRIORITY_PASS"
             <<" idle=TRUE busy_scoring=TRUE sample_end=TRUE"
             <<" original_voice_steal_IO_and_PCM_UNVERIFIED=TRUE\n";
}
