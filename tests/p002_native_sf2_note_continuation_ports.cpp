#include "CreativeSf2NoteContinuationPorts.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(c) do{if(!(c)){std::cerr<<"P002_SF2_POST18_FAIL line="<<__LINE__<<" "<<#c<<"\n";return 1;}}while(0)
namespace {
void word(CreativeSf2VoiceDescriptorBytes& b,unsigned op,std::int16_t val){
    const auto w=static_cast<std::uint16_t>(val);
    b[op*2u]=static_cast<std::uint8_t>(w);
    b[op*2u+1u]=static_cast<std::uint8_t>(w>>8u);
}
}
int main(){
    const OriginalSf2VerifiedLfoChannelAmounts trueState{true,0,0};
    struct Ref {
        int program;
        std::uint8_t voice;
        std::int16_t lfo1Pitch,freq1,attackVol;
        std::uint16_t fmmod,tremfrq,envval,envvol;
    };
    constexpr std::array<Ref,4> refs{{
        {0,0,0,128,125,0x0000u,0x0080u,0x8000u,0x8000u},
        {40,0,2,122,114,0x0200u,0x007au,0x8000u,0x8000u},
        {100,0,7,254,128,0x0700u,0x00feu,0x8000u,0xbfffu},
        {100,3,0,128,128,0x0000u,0x0080u,0x8000u,0xbfffu}
    }};
    for(const auto& ref:refs) {
        CreativeSf2VoiceDescriptorBytes b{};
        b[0x8cu]=1u;
        word(b,5u,ref.lfo1Pitch);
        word(b,22u,ref.freq1);
        word(b,26u,125); // original mod attack
        word(b,25u,static_cast<std::int16_t>(0x8000u));
        word(b,34u,ref.attackVol);
        word(b,33u,static_cast<std::int16_t>(0x8000u));
        const auto actual=candidateOriginalSf2AfterPitchRegisters(
            b,ref.voice,trueState);
        CHECK(actual && actual->size()==16u);
        const std::array<OriginalWin95VxdPortWrite,16> original{{
            {0xe22u,static_cast<std::uint32_t>((2u<<5u)|ref.voice),8u},
            {0xe20u,0u,16u},
            {0xe22u,static_cast<std::uint32_t>((3u<<5u)|ref.voice),8u},
            {0xe20u,ref.fmmod,16u},
            {0xe22u,static_cast<std::uint32_t>((4u<<5u)|ref.voice),8u},
            {0xe20u,ref.tremfrq,16u},
            {0xe22u,static_cast<std::uint32_t>((5u<<5u)|ref.voice),8u},
            {0xe20u,0u,16u},
            {0xe22u,static_cast<std::uint32_t>((6u<<5u)|ref.voice),8u},
            {0xa20u,ref.envval,16u},
            {0xe22u,static_cast<std::uint32_t>((4u<<5u)|ref.voice),8u},
            {0xa20u,ref.envvol,16u},
            {0xe22u,static_cast<std::uint32_t>((1u<<5u)|ref.voice),8u},
            {0x620u,0u,32u},
            {0xe22u,ref.voice,8u},
            {0x620u,0u,32u}
        }};
        for(std::size_t i=0;i<original.size();++i){
            CHECK((*actual)[i].port==original[i].port);
            CHECK((*actual)[i].access_width_bits==original[i].access_width_bits);
            CHECK((*actual)[i].value==original[i].value);
        }
    }
    // Independent second ORIGINAL attack=128 branch: modulation ENVVAL
    // becomes BFFF too (original x86 LCOD+0x5D58 special case).
    CreativeSf2VoiceDescriptorBytes mod128{};
    mod128[0x8cu]=1u;
    word(mod128,26u,128);
    word(mod128,25u,static_cast<std::int16_t>(0x8000u));
    const auto special=candidateOriginalSf2AfterPitchRegisters(
        mod128,1u,trueState);
    CHECK(special && (*special)[9].value==0xbfffu);
    word(mod128,25u,0x7fffu);
    const auto noSpecial=candidateOriginalSf2AfterPitchRegisters(
        mod128,1u,trueState);
    CHECK(noSpecial && (*noSpecial)[9].value==0x7fffu);
    word(mod128,26u,125);
    word(mod128,25u,static_cast<std::int16_t>(0x8000u));
    const auto normal=candidateOriginalSf2AfterPitchRegisters(
        mod128,1u,trueState);
    CHECK(normal && (*normal)[9].value==0x8000u);
    // Original STATE +0x0a and +0x0b actually contribute to FMMOD;
    // their unknown values must not be guessed or silently omitted.
    word(mod128,5u,12);
    const OriginalSf2VerifiedLfoChannelAmounts device{true,7,9};
    const auto deviceResult=candidateOriginalSf2AfterPitchRegisters(
        mod128,1u,device);
    CHECK(deviceResult && (*deviceResult)[3].value==0x1c00u);
    word(mod128,5u,120);
    const auto saturated=candidateOriginalSf2AfterPitchRegisters(
        mod128,1u,device);
    CHECK(saturated && (*saturated)[3].value==0x7f00u);
    CHECK(!candidateOriginalSf2AfterPitchRegisters(
        mod128,1u,{false,7,9}));
    CHECK(!candidateOriginalSf2AfterPitchRegisters(
        mod128,30u,trueState));
    CHECK(!candidateOriginalSf2AfterPitchRegisters(
        mod128,31u,trueState));
    mod128[0x8du]=0x80u;
    CHECK(!candidateOriginalSf2AfterPitchRegisters(
        mod128,1u,trueState));
    mod128[0x8du]=0;
    word(mod128,34u,129);
    CHECK(!candidateOriginalSf2AfterPitchRegisters(
        mod128,1u,trueState));
    std::cout<<"P002_ORIGINAL_SF2_NEXT_16_TYPED_PORTS_PASS "
             <<"real_original_x86_GM0_GM40_GM100_4x16=64 "
             <<"separate_attack128_mod_env_and_volume_env_source_branches=TRUE "
             <<"explicit_original_lfo_channel_terms=TRUE "
             <<"not_full_NoteOn_PCM=TRUE\n";
    return 0;
}
