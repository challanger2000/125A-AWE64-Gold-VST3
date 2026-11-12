#include "CreativeSf2FullNoteOnPorts.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace awe64::creative;
#define CHECK(c) do{if(!(c)){std::cerr<<"P002_SF2_FULL_NOTEON_FAIL line="<<__LINE__<<" "<<#c<<"\n";return 1;}}while(0)
namespace {
void set(CreativeSf2VoiceDescriptorBytes& b,unsigned op,int v) noexcept {
    const auto word=static_cast<std::uint16_t>(v);
    b[2u*op]=static_cast<std::uint8_t>(word);
    b[2u*op+1u]=static_cast<std::uint8_t>(word>>8u);
}
void set32(CreativeSf2VoiceDescriptorBytes& b,unsigned offset,
           std::uint32_t v) noexcept {
    for(unsigned i=0;i<4u;++i)b[offset+i]=static_cast<std::uint8_t>(v>>(i*8u));
}
OriginalSf2FullNoteOnVoice voice(
    std::uint8_t physical,
    std::uint32_t start,std::uint32_t end,
    std::uint32_t loopStart,std::uint32_t loopEnd,
    int root,int fine,int cutoff,int attenuation,
    int attack,int attackHigh,int pan,int chorus,
    int volDecay,int volSustain,
    std::uint16_t originalVolumeTarget,
    std::uint16_t originalPitchTarget) noexcept {
    OriginalSf2FullNoteOnVoice v{};
    v.physical_voice=physical;
    v.played_midi_key=60u;
    v.source_original_voice_verified=true;
    v.lfo={true,0,0}; // actual original x86 per-voice lfo channel state
    v.target={true,originalVolumeTarget,originalPitchTarget};
    auto& b=v.verified_after_start_record;
    set(b,58u,root);set(b,56u,100);set(b,52u,fine);
    set(b,8u,cutoff);set(b,19u,attenuation);
    set(b,34u,attack);set(b,35u,attackHigh);
    set(b,26u,125);set(b,27u,127);
    set(b,28u,127);set(b,29u,0);
    set(b,21u,static_cast<std::int16_t>(0x8000u));
    set(b,23u,static_cast<std::int16_t>(0x8000u));
    set(b,25u,static_cast<std::int16_t>(0x8000u));
    set(b,33u,static_cast<std::int16_t>(0x8000u));
    set(b,22u,128);
    set(b,54u,1);
    set(b,15u,chorus);set(b,16u,27);set(b,17u,pan);
    set(b,18u,129);
    set(b,36u,volDecay);set(b,37u,volSustain);
    b[0x8au]=60u;b[0x8cu]=1u;
    b[0x88u]=0xe0u; // independently measured original dwPitch 44.1k
    set32(b,0x76u,start);
    set32(b,0x7au,end);
    set32(b,0x7eu,loopStart);
    set32(b,0x82u,loopEnd);
    return v;
}
}
int main(){
    const auto gm0=voice(0,0xfeb9,0x152fe,0x152bb,0x152fa,
         77,4,179,58,125,120,127,0,3,0,0,0x1c48);
    const auto single=candidateOriginalSf2FullNoteOnPorts(
        std::span<const OriginalSf2FullNoteOnVoice>(&gm0,1));
    CHECK(single && single->voices==1u && single->count==61u);
    CHECK(single->out[0].port==0xe22u && single->out[0].value==0x60u);
    CHECK(single->out[1].port==0xe22u && single->out[1].value==0xa0u);
    CHECK(single->out[2].port==0xa20u && single->out[2].value==0xffu &&
          single->out[2].access_width_bits==16u);
    CHECK(single->out[3].port==0xe22u && single->out[3].value==0x60u);
    CHECK(single->out[4].port==0x620u && single->out[4].value==0xffffu &&
          single->out[4].access_width_bits==32u);
    CHECK(single->out[5].port==0xe22u &&
          single->out[6].port==0x620u &&
          single->out[6].value==0x0000ffffu);
    CHECK(single->out[5+18].port==0xe22u &&
          single->out[5+18].value==0x40u);
    CHECK(single->out[5+34].port==0xe22u &&
          single->out[5+34].value==0xc0u);
    CHECK(single->out[5+34+1].value==0x7f0152bau);
    CHECK(single->out[59].port==0xe22u &&
          single->out[59].value==0xa0u);
    CHECK(single->out[60].port==0xa20u &&
          single->out[60].value==0x0003u);

    // Authentic GM100 first voice0 + second voice3:
    // source x86 = [5 allocator OUTs for voice0]
    //           + [5 allocator OUTs for voice3]
    //           + [56 full NoteStart OUTs for voice0]
    //           + [56 full NoteStart OUTs for voice3] => 122 exactly.
    const auto gm100a=voice(0,0x74a48,0x76dd5,0x74a6b,0x76dd1,
        85,-32,255,42,128,127,127,118,9,0,0x260c,0x1254);
    const auto gm100b=voice(3,0x34116,0x36804,0x3460e,0x36800,
        94,4,255,42,128,127,127,122,9,0,0x260c,0x0ae7);
    const std::array<OriginalSf2FullNoteOnVoice,2> pair{{gm100a,gm100b}};
    const auto two=candidateOriginalSf2FullNoteOnPorts(pair);
    CHECK(two && two->voices==2u && two->count==122u);
    CHECK(two->out[0].value==0x60u);
    CHECK(two->out[5].value==0x63u);
    CHECK(two->out[6].value==0xa3u);
    CHECK(two->out[7].value==0xffu);
    CHECK(two->out[10].value==0x60u);
    CHECK(two->out[66].value==0x63u);
    CHECK(two->out[10+34+1].value==0x7f074a6au);
    CHECK(two->out[66+34+1].value==0x7f03460du);
    CHECK(two->out[10+34+21].value==0x0009u);
    CHECK(two->out[66+34+21].value==0x0009u);
    CHECK(two->out[10+14+1].value==0xbe3bu);
    CHECK(two->out[66+14+1].value==0xb2b5u);

    // Invalid source identity/voicing must fail before ANY emitted plan;
    // two voices are not allowed to collide on the physical chip channel.
    CHECK(!candidateOriginalSf2FullNoteOnPorts({}));
    auto duplicate=pair;
    duplicate[1].physical_voice=duplicate[0].physical_voice;
    CHECK(!candidateOriginalSf2FullNoteOnPorts(duplicate));
    auto invalid=pair;
    invalid[1].source_original_voice_verified=false;
    CHECK(!candidateOriginalSf2FullNoteOnPorts(invalid));
    invalid=pair;invalid[1].lfo.source_original_state_verified=false;
    CHECK(!candidateOriginalSf2FullNoteOnPorts(invalid));
    invalid=pair;invalid[1].target.original_driver_state_verified=false;
    CHECK(!candidateOriginalSf2FullNoteOnPorts(invalid));
    invalid=pair;invalid[1].verified_after_start_record[0x8du]=0x80u;
    CHECK(!candidateOriginalSf2FullNoteOnPorts(invalid));
    invalid=pair;invalid[1].played_midi_key=128u;
    CHECK(!candidateOriginalSf2FullNoteOnPorts(invalid));
    std::array<OriginalSf2FullNoteOnVoice,7> tooMany{};
    for(unsigned i=0;i<tooMany.size();++i) {
        tooMany[i]=gm0;
        tooMany[i].physical_voice=static_cast<std::uint8_t>(i);
    }
    CHECK(!candidateOriginalSf2FullNoteOnPorts(tooMany));
    std::cout<<"P002_ORIGINAL_SF2_FULL_61_122_ORDERED_OUTS_PASS "
             <<"single_61=TRUE source_dual_122=TRUE "
             <<"allocation_prologue_all_voices_before_56_per_voice=TRUE "
             <<"explicit_original_target_operands=TRUE "
             <<"original_driver_timing_pcm=NOT_PROVEN\n";
    return 0;
}
