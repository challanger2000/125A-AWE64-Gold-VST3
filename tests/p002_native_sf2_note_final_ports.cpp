#include "CreativeSf2NoteFinalPorts.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(c) do{if(!(c)){std::cerr<<"P002_SF2_FINAL22_FAIL line="<<__LINE__<<" "<<#c<<"\n";return 1;}}while(0)
namespace {
void word(CreativeSf2VoiceDescriptorBytes& b,unsigned op,int val){
    const auto w=static_cast<std::uint16_t>(val);
    b[2u*op]=static_cast<std::uint8_t>(w);
    b[2u*op+1u]=static_cast<std::uint8_t>(w>>8u);
}
void dword(CreativeSf2VoiceDescriptorBytes& b,unsigned offset,std::uint32_t v){
    for(unsigned i=0;i<4u;++i)b[offset+i]=static_cast<std::uint8_t>(v>>(8u*i));
}
}
int main(){
    // Source-observed four ORIGINAL x86 ROM-mono GM0/GM40/GM100×2 cases:
    // no ROM or VXD data is distributed. Two original stack local operands
    // (volume envelope and pitch target) are independently VERIFIED caller
    // inputs, not fabricated SoundFont generator formulas.
    struct Original {
        unsigned program,voice;
        std::uint32_t start,end,loopStart,loopEnd;
        unsigned chorus,pan,reverb,aux,decay,sustain,mode,cutoff;
        unsigned attack,volDelay,volumeTarget,pitchTarget;
        std::uint32_t psst,csl,ccca,filterVol,ptrx,cpf,trigger;
    };
    constexpr std::array<Original,4> cases{{
        {0,0,0x0feb9,0x152fe,0x152bb,0x152fa,
         0,127,27,129,3,0,1,179,
         125,0x8000,0,0x1c48,
         0x7f0152ba,0x000152f9,0x0000feb5,0x0000b300,
         0x1c481b81,0x1c480000,0x0003},
        {40,0,0x3765b,0x37d1e,0x37cf6,0x37d1a,
         0,127,27,129,4,127,1,149,
         114,0x8000,0,0x10cf,
         0x7f037cf5,0x00037d19,0x00037657,0x00009500,
         0x10cf1b81,0x10cf0000,0x7f04},
        {100,0,0x74a48,0x76dd5,0x74a6b,0x76dd1,
         118,127,27,129,9,0,1,255,
         128,0x8000,0x260c,0x1254,
         0x7f074a6a,0x76076dd0,0x00074a44,0x260cff00,
         0x12541b81,0x12540000,0x0009},
        {100,3,0x34116,0x36804,0x3460e,0x36800,
         122,127,27,129,9,0,1,255,
         128,0x8000,0x260c,0x0ae7,
         0x7f03460d,0x7a0367ff,0x00034112,0x260cff00,
         0x0ae71b81,0x0ae70000,0x0009}
    }};
    constexpr std::array<std::uint16_t,11> selector{{
        0xc0,0xe0,0x00,0xa0,0x80,0x00,0x60,0x40,0x20,0x00,0xa0
    }};
    constexpr std::array<std::uint16_t,11> ports{{
        0x620,0x620,0xa20,0x620,0x620,0xa20,0x620,
        0x620,0x620,0x620,0xa20
    }};
    constexpr std::array<std::uint8_t,11> widths{{
        32,32,32,32,32,32,32,32,32,32,16
    }};
    for(const auto& r:cases){
        CreativeSf2VoiceDescriptorBytes note{};
        note[0x8cu]=1u; // actual AFTER original note-start ROM mono
        dword(note,0x76u,r.start);
        dword(note,0x7au,r.end);
        dword(note,0x7eu,r.loopStart);
        dword(note,0x82u,r.loopEnd);
        word(note,54u,r.mode);word(note,9u,0);
        word(note,15u,r.chorus);word(note,16u,r.reverb);
        word(note,17u,r.pan);word(note,18u,r.aux);
        word(note,34u,r.attack);word(note,33u,r.volDelay);
        word(note,36u,r.decay);word(note,37u,r.sustain);
        word(note,8u,r.cutoff);
        const OriginalSf2VerifiedFinalTargetState state{
            true,static_cast<std::uint16_t>(r.volumeTarget),
            static_cast<std::uint16_t>(r.pitchTarget)};
        const auto got=candidateOriginalSf2FinalNoteStartPorts(
            note,static_cast<std::uint8_t>(r.voice),state);
        CHECK(got && got->size()==22u);
        const std::array<std::uint32_t,11> values{{
            r.psst,r.csl,r.ccca,0u,0u,r.ccca,
            r.filterVol,r.filterVol,r.ptrx,r.cpf,r.trigger
        }};
        for(unsigned n=0;n<11u;++n){
            const auto& p=(*got)[n*2u];
            const auto& d=(*got)[n*2u+1u];
            CHECK(p.port==0xe22u && p.access_width_bits==8u &&
                  p.value==(selector[n]|r.voice));
            CHECK(d.port==ports[n] && d.access_width_bits==widths[n] &&
                  d.value==values[n]);
        }
    }

    // No model for unknown VXD targets: explicit verified source flag.
    CreativeSf2VoiceDescriptorBytes b{};
    b[0x8cu]=1u;
    dword(b,0x76u,0x100u);dword(b,0x7au,0x200u);
    dword(b,0x7eu,0x110u);dword(b,0x82u,0x1f0u);
    word(b,54u,1);word(b,8u,179);word(b,17u,127);
    word(b,16u,27);word(b,18u,129);
    const OriginalSf2VerifiedFinalTargetState valid{true,0,0x1c48};
    CHECK(!candidateOriginalSf2FinalNoteStartPorts(b,0,{false,0,0x1c48}));
    CHECK(!candidateOriginalSf2FinalNoteStartPorts(b,30,valid));
    CHECK(!candidateOriginalSf2FinalNoteStartPorts(b,31,valid));
    b[0x8du]=0x80u; // raw pre-start ROM qualifier forbidden
    CHECK(!candidateOriginalSf2FinalNoteStartPorts(b,0,valid));
    b[0x8du]=0u;word(b,54u,4);
    CHECK(!candidateOriginalSf2FinalNoteStartPorts(b,0,valid));
    word(b,54u,1);word(b,9u,16);
    CHECK(!candidateOriginalSf2FinalNoteStartPorts(b,0,valid));
    word(b,9u,0);word(b,8u,256);
    CHECK(!candidateOriginalSf2FinalNoteStartPorts(b,0,valid));
    word(b,8u,179);word(b,17u,256);
    CHECK(!candidateOriginalSf2FinalNoteStartPorts(b,0,valid));
    word(b,17u,127);word(b,34u,125);
    CHECK(!candidateOriginalSf2FinalNoteStartPorts(b,0,{true,1,0x1c48}));
    word(b,34u,128);word(b,33u,static_cast<std::int16_t>(0x8000u));
    const auto special=candidateOriginalSf2FinalNoteStartPorts(
        b,0,{true,0x260c,0x1c48});
    CHECK(special && (*special)[13].value==0x260cb300u);
    word(b,54u,0); // non-loop, end+4 / end+8
    const auto nonloop=candidateOriginalSf2FinalNoteStartPorts(
        b,0,{true,0x260c,0x1c48});
    CHECK(nonloop && (*nonloop)[1].value==0x204u &&
          (*nonloop)[3].value==0x208u);
    std::cout<<"P002_ORIGINAL_SF2_FINAL_22_TYPED_NOTEON_OUTS_PASS "
             <<"original_GM0_GM40_GM100_4x22=88 "
             <<"source_stacks_pitch_volume_explicit=TRUE "
             <<"actual_full_timing_PCM=NOT_PROVEN\n";
    return 0;
}
