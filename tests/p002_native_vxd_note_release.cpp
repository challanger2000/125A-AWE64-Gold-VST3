#include "CreativeVxdNoteRelease.h"
#include <iostream>
using namespace awe64::creative;
#define CHECK(v) do{if(!(v)){std::cerr<<"P002_VXD_MIDI_RELEASE_FAIL line="<<__LINE__<<" "<<#v<<"\n";return 1;}}while(0)
int main(){
    OriginalVxdNoteReleaseEvidence e{};
    CHECK(!candidateOriginalVxdMidiNoteRelease(3,e));
    e.original_key_channel_match=true;
    CHECK(!candidateOriginalVxdMidiNoteRelease(3,e));
    e.original_voice_record_verified=true;
    e.original_sustain_held=true;
    auto a=candidateOriginalVxdMidiNoteRelease(3,e);
    CHECK(a && a->deferred_by_sustain && a->count==0 &&
          !a->original_voice_marker_idle);
    e.original_sustain_held=false;
    CHECK(!candidateOriginalVxdMidiNoteRelease(3,e)); // missing chip reads
    e.current_chip_registers_verified=true;
    e.encoded_volume_release=0x20;
    e.encoded_mod_release=0x45;
    a=candidateOriginalVxdMidiNoteRelease(3,e);
    CHECK(a && !a->deferred_by_sustain && a->count==4 &&
          a->original_voice_marker_idle);
    CHECK(a->writes[0].port==0xE22 && a->writes[0].value==0xA3 &&
          a->writes[0].access_width_bits==8);
    CHECK(a->writes[1].port==0xA20 && a->writes[1].value==0x8020 &&
          a->writes[1].access_width_bits==16);
    CHECK(a->writes[2].port==0xE22 && a->writes[2].value==0xE3);
    CHECK(a->writes[3].port==0xA20 && a->writes[3].value==0x8045);
    // Original VXD takes the prior chip register high bytes, not 
    // an invented pan/chorus zero. Exact voice/data register order.
    e.stored_end_word=0x152feu;
    e.observed_chip_csl=0x53000005u;
    e.observed_chip_psst=0x47000012u;
    a=candidateOriginalVxdMidiNoteRelease(3,e);
    CHECK(a && a->count==8);
    CHECK(a->writes[4].port==0xE22 && a->writes[4].value==0xE3);
    CHECK(a->writes[5].port==0x620 && a->writes[5].value==0x53015302u &&
          a->writes[5].access_width_bits==32);
    CHECK(a->writes[6].port==0xE22 && a->writes[6].value==0xC3);
    CHECK(a->writes[7].port==0x620 && a->writes[7].value==0x470152FEu &&
          a->writes[7].access_width_bits==32);
    e.stored_end_word=0xfffffbu;
    CHECK(candidateOriginalVxdMidiNoteRelease(29,e)->writes[5].value==
          (0x53000000u|0xffffffu));
    e.stored_end_word=0xfffffcu;
    CHECK(!candidateOriginalVxdMidiNoteRelease(29,e));
    e.stored_end_word=0;
    CHECK(!candidateOriginalVxdMidiNoteRelease(30,e));
    CHECK(!candidateOriginalVxdMidiNoteRelease(31,e));
    e.original_key_channel_match=false;
    CHECK(!candidateOriginalVxdMidiNoteRelease(3,e));
    e.original_key_channel_match=true;
    e.current_chip_registers_verified=false;
    e.original_sustain_held=true;
    // Pedal-held NoteOff requires note-record identity but NO HW read.
    CHECK(candidateOriginalVxdMidiNoteRelease(3,e)->count==0);
    std::cout<<"P002_ORIGINAL_WIN95_MIDI_NOTEOFF_RELEASE_PASS"
             <<" sustain_deferral=TRUE real_DCYSUSV=8020 real_DCYSUS=8045"
             <<" exact_CSL_PSST_current_high_bytes=TRUE"
             <<" no_force_choke_or_guessed_ADSR=TRUE\n";
}
