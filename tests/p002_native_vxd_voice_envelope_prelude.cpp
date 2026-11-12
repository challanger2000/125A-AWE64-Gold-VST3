#include "CreativeVxdVoiceEnvelopePrelude.h"
#include <iostream>
using namespace awe64::creative;
#define CHECK(v) do{if(!(v)){std::cerr<<"P002_VXD_ENVELOPE_START_FAIL line="<<__LINE__<<" "<<#v<<"\n";return 1;}}while(0)
int main(){
    OriginalVxdEncodedVoiceEnvelopePrelude s{};
    CHECK(!candidateOriginalVxdVoiceEnvelopePrelude(3,s));
    s.from_actual_original_driver_record=true;
    s.attack_hold_volume_low_byte_word=0x0080;
    s.attack_hold_volume_high_word=0x56;
    s.lfo1_delay_word=0x8000;
    s.attack_hold_mod_low_byte_word=0x0080;
    s.attack_hold_mod_high_word=0x34;
    s.decay_sustain_mod_low_byte=0x12;
    s.decay_sustain_mod_high_word=0x45;
    s.lfo2_delay_word=0x8010;
    const auto x=candidateOriginalVxdVoiceEnvelopePrelude(3,s);
    CHECK(x);
    CHECK((*x)[0].port==0xE22 && (*x)[0].value==0x63 &&
          (*x)[0].access_width_bits==8);
    CHECK((*x)[1].port==0x620 && (*x)[1].value==0xffffu &&
          (*x)[1].access_width_bits==32);
    CHECK((*x)[2].value==0x43 && (*x)[3].value==0xffffu); // CVCF
    CHECK((*x)[4].value==0x83 && (*x)[5].port==0xA22 &&
          (*x)[5].value==0x567fu && (*x)[5].access_width_bits==16);
    CHECK((*x)[6].value==0xA3 && (*x)[7].value==0x8000u); // LFO1VAL
    CHECK((*x)[8].value==0xC3 && (*x)[9].value==0x347fu); // ATKHLD
    CHECK((*x)[10].value==0xE3 && (*x)[11].port==0xA20 &&
          (*x)[11].value==0x4512u); // DCYSUS
    CHECK((*x)[12].value==0xE3 && (*x)[13].port==0xA22 &&
          (*x)[13].value==0x8010u); // LFO2VAL
    CHECK(!candidateOriginalVxdVoiceEnvelopePrelude(30,s));
    CHECK(!candidateOriginalVxdVoiceEnvelopePrelude(31,s));
    CHECK(!candidateOriginalVxdVoiceEnvelopePrelude(255,s));
    s.attack_hold_volume_low_byte_word=0x0081;
    s.attack_hold_mod_low_byte_word=0x007e;
    const auto y=candidateOriginalVxdVoiceEnvelopePrelude(29,s);
    CHECK(y && (*y)[5].value==0x5681u && (*y)[9].value==0x347eu);
    s.from_actual_original_driver_record=false;
    CHECK(!candidateOriginalVxdVoiceEnvelopePrelude(29,s));
    std::cout<<"P002_ORIGINAL_CREATIVE_VXD_VOICE_ENVELOPE_PRELUDE_PASS"
             <<" actual_vtft_cvcf=FFFF two_attacks_and_lfos=TRUE"
             <<" special_0080_to_007f=TRUE native_full_noteon_PCM_UNVERIFIED=TRUE\n";
}
