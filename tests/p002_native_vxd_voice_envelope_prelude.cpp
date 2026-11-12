#include "CreativeVxdVoiceEnvelopePrelude.h"
#include <iostream>
#include <array>
#include <cstdint>
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
    // Independent x86 Unicorn source oracle examples, as recorded in PR #4.
    CHECK(originalCreativePcodEnvelopeRateFromWord(0)==127);
    CHECK(originalCreativePcodEnvelopeRateFromWord(127)==87);
    CHECK(originalCreativePcodEnvelopeRateFromWord(1000)==39);
    CHECK(originalCreativePcodEnvelopeRateFromWord(47513)==1);
    CHECK(originalCreativePcodEnvelopeRateFromWord(65535)==0);
    CHECK(originalCreativePcodEnvelopeRateFromWord(47512)==1);
    CHECK(originalCreativePcodEnvelopeRateFromWord(47514)==0);
    CHECK(originalCreativePcodEnvelopeRateFromWord(23)==127);
    CHECK(originalCreativePcodEnvelopeRateFromWord(24)==127);
    CHECK(originalCreativePcodEnvelopeRateFromWord(25)==126);
    CHECK(originalCreativePcodEnvelopeRateFromWord(27)==124);
    // Independently calculated digest over ALL 65,536 input words.
    // Oracle is the private original VXD's literal LCOD threshold bytes.
    std::uint64_t fnv=14695981039346656037ull;
    std::array<std::uint32_t,128> hist{};
    std::uint8_t prev=127;
    for(std::uint32_t i=0;i<65536u;++i) {
        const auto rate=originalCreativePcodEnvelopeRateFromWord(
            static_cast<std::uint16_t>(i));
        CHECK(rate<=prev);
        prev=rate;
        ++hist[rate];
        fnv=(fnv^static_cast<std::uint64_t>(rate))*1099511628211ull;
    }
    CHECK(fnv==0xedaf6dc7659a5db1ull);
    CHECK(hist[0]==18022 && hist[127]==25);
    // Independently recovered ORIGINAL LE PCOD+0x37BA generator-dispatch
    // mapping: gen26 and gen34 -> LCOD+0x74F2 ATTACK lookup; gen28,
    // gen30 and gen38 -> PCOD+0x3791 RELEASE/DECAY lookup.
    CHECK(originalCreativePcodAttackRateFromWord(0)==128);
    CHECK(originalCreativePcodAttackRateFromWord(10)==115);
    CHECK(originalCreativePcodAttackRateFromWord(127)==55);
    CHECK(originalCreativePcodAttackRateFromWord(1000)==11);
    CHECK(originalCreativePcodAttackRateFromWord(65535)==1);
    CHECK(candidateOriginalCreative1MgmGeneratorEnvelopeRate(26,10)==115);
    CHECK(candidateOriginalCreative1MgmGeneratorEnvelopeRate(34,10)==115);
    CHECK(candidateOriginalCreative1MgmGeneratorEnvelopeRate(28,1000)==39);
    CHECK(candidateOriginalCreative1MgmGeneratorEnvelopeRate(30,680)==49);
    CHECK(candidateOriginalCreative1MgmGeneratorEnvelopeRate(38,918)==41);
    // These IDs use OTHER original VXD processing: do not assume the
    // PCOD threshold is a universal timecents converter.
    CHECK(!candidateOriginalCreative1MgmGeneratorEnvelopeRate(36,10));
    CHECK(!candidateOriginalCreative1MgmGeneratorEnvelopeRate(48,10));
    CHECK(!candidateOriginalCreative1MgmGeneratorEnvelopeRate(8,10));
    CHECK(!candidateOriginalCreative1MgmGeneratorEnvelopeRate(255,10));
    std::uint64_t attackDigest=14695981039346656037ull;
    std::array<std::uint32_t,129> attackHist{};
    for(std::uint32_t input=0;input<65536u;++input) {
        const auto rate=originalCreativePcodAttackRateFromWord(
            static_cast<std::uint16_t>(input));
        CHECK(rate<=128);
        ++attackHist[rate];
        attackDigest=(attackDigest^rate)*1099511628211ull;
    }
    // Independent original owner's LCOD 0x30AA private table, full
    // exhaustive 65,536-case native-vs-original functional checksum.
    CHECK(attackDigest==0xebb9ee405dab7e7full);
    CHECK(attackHist[1]==59596 && attackHist[126]==6 &&
          attackHist[128]==1);
    std::cout<<"P002_ORIGINAL_CREATIVE_VXD_VOICE_ENVELOPE_PRELUDE_PASS"
             <<" actual_vtft_cvcf=FFFF two_attacks_and_lfos=TRUE"
             <<" special_0080_to_007f=TRUE PCOD_65536=PASS"
             <<" strict_less_than=TRUE ATTACK_65536=PASS"
             <<" original_SBK_generator26_28_30_34_38=TRUE"
             <<" native_full_noteon_PCM_UNVERIFIED=TRUE\n";
}
