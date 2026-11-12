#include "CreativeVxdVoiceEnvelopePrelude.h"
#include "CreativeSbkLayers.h"
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
    // Original actual x86 SF2 bank handoff values, not the SF1 rate table:
    CHECK(originalCreativeSf2VoiceAttackRate(-8800)==125);  // SF2 GM0
    CHECK(originalCreativeSf2VoiceAttackRate(-7972)==114);  // SF2 GM40
    CHECK(originalCreativeSf2VoiceAttackRate(-11958)==128); // two GM100 layers
    CHECK(originalCreativeSf2VoiceAttackRate(-32768)==128);
    CHECK(originalCreativeSf2VoiceAttackRate(-600)==16);
    CHECK(originalCreativeSf2VoiceAttackRate(-599)==15);
    CHECK(originalCreativeSf2VoiceAttackRate(0)==12);
    CHECK(originalCreativeSf2VoiceAttackRate(4300)==1);
    CHECK(originalCreativeSf2VoiceAttackRate(32767)==1);
    // Format2 generator34 SOURCE words -> within-level override ->
    // signed merge -> original PCOD voice attack encoder. No arbitrary
    // default from a synthetic Windows device_state is fabricated here.
    SbkLayer sf2Attack{};
    sf2Attack.source_format=OriginalCreativeBankFormat::Format2Sf2;
    sf2Attack.instrument_local.raw[34]=static_cast<std::uint16_t>(-11958);
    CHECK(!candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2Attack,34));
    sf2Attack.preset_global.raw[34]=0;
    const auto mergedAttack=candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
        sf2Attack,34);
    CHECK(mergedAttack && *mergedAttack==-11958);
    const auto descriptorAttack=originalCreativeSf2VoiceAttackRate(*mergedAttack);
    CHECK(descriptorAttack==128); // original GM100 SF2 special descriptor
    // The separate original VXD note-start contract above checks that an
    // actual descriptor value 128 is emitted as ATKHLDV low byte 127.
    sf2Attack.instrument_local.raw[34]=static_cast<std::uint16_t>(-8800);
    CHECK(originalCreativeSf2VoiceAttackRate(
        *candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
            sf2Attack,34))==125); // original GM0 signed attack case
    sf2Attack.instrument_local.raw[34]=static_cast<std::uint16_t>(-7972);
    CHECK(originalCreativeSf2VoiceAttackRate(
        *candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
            sf2Attack,34))==114); // original GM40 signed attack case
    // Real Creative SF2 PCOD +0x3BD4 source-derived integer boundary
    // vectors. These deliberately include measured original GM0/40 cases.
    struct Sf2PcodCase{std::uint16_t op;std::int16_t input;std::uint16_t output;};
    constexpr Sf2PcodCase sf2PcodCases[]{
        {21,-12000,32768},{21,-11999,32767},{21,5483,14},{21,5484,0},
        {23,0,31304},{25,-2332,32388},{33,-32768,32768},
        {22,-16000,0},{22,-8800,1},{22,0,205},{22,1800,255},
        {24,-2332,53},
        {26,-11958,128},{34,-8800,125},{34,-7972,114},
        {27,-5369,127},{27,-799,120},{27,0,116},{35,-2332,125},
        {28,-32768,127},{28,1800,16},{28,1801,15},{28,4386,3},
        {30,3975,4},{36,3975,4},{38,-667,48},
        {29,-200,0},{29,50,50},{37,255,127}
    };
    for(const auto& v:sf2PcodCases){
        CHECK(originalCreativeSf2VoiceEnvelopeOrLfoWord(v.op,v.input)==v.output);
    }
    for(const std::uint16_t op:{0u,8u,20u,31u,40u,41u,255u})
        CHECK(!originalCreativeSf2VoiceEnvelopeOrLfoWord(op,0));
    // Original SF2 melodic key scaling occurs before rate encoding.
    // Independently observed original format1/format2 branch structure;
    // key72 with original scale100 produces 4000+(60-72)*100=2800.
    CHECK(originalCreativeSf2KeyScaledEnvelopeWord(36,4000,100,72,0)==2800);
    CHECK(originalCreativeSf2KeyScaledEnvelopeWord(36,4000,100,72,128)==4000);
    CHECK(originalCreativeSf2KeyScaledEnvelopeWord(28,4000,100,60,0)==4000);
    CHECK(originalCreativeSf2KeyScaledEnvelopeWord(27,-799,100,72,0)==-1999);
    CHECK(originalCreativeSf2KeyScaledEnvelopeWord(35,32000,1000,0,0)==32767);
    CHECK(originalCreativeSf2KeyScaledEnvelopeWord(36,-32000,1000,127,0)==-32768);
    CHECK(!originalCreativeSf2KeyScaledEnvelopeWord(29,0,0,60,0));
    CHECK(!originalCreativeSf2KeyScaledEnvelopeWord(36,0,0,128,0));
    CHECK(originalCreativeSf2VoiceEnvelopeOrLfoWord(38,-148)==41);
    CHECK(originalCreativeSf2VoiceEnvelopeOrLfoWord(35,-799)==120);
    CHECK(originalCreativeSf2VoiceEnvelopeOrLfoWord(36,4386)==3);
    CHECK(originalCreativeSf2VoiceEnvelopeOrLfoWord(38,-667)==48);
    std::cout<<"P002_ORIGINAL_CREATIVE_VXD_VOICE_ENVELOPE_PRELUDE_PASS"
             <<" actual_vtft_cvcf=FFFF two_attacks_and_lfos=TRUE"
             <<" special_0080_to_007f=TRUE PCOD_65536=PASS"
             <<" strict_less_than=TRUE ATTACK_65536=PASS"
             <<" original_SBK_generator26_28_30_34_38=TRUE"
             <<" original_SF2_attack=-8800:125_-7972:114_-11958:128"
             <<" native_full_noteon_PCM_UNVERIFIED=TRUE\n";
}
