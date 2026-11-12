#include "CreativeSf2VoiceDescriptor.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(v) do { if(!(v)) { std::cerr<<"P002_SF2_DESCRIPTOR_FAIL line="<<__LINE__<<" "<<#v<<"\n";return 1;} } while(0)
namespace {
void set(CreativeSf2VoiceDescriptorBytes& b,unsigned op,std::int32_t x) {
    const auto w=static_cast<std::uint16_t>(x);
    b[2u*op]=static_cast<std::uint8_t>(w);
    b[2u*op+1u]=static_cast<std::uint8_t>(w>>8);
}
std::uint16_t get(const CreativeSf2VoiceDescriptorBytes& b,unsigned op) {
    return static_cast<std::uint16_t>(b[2u*op] |
        (static_cast<unsigned>(b[2u*op+1u])<<8));
}
struct OriginalReference {
    unsigned program,layer;
    std::int16_t amplitude,cutoff,attack,delay_lfo1,freq_lfo1;
    std::int16_t hold_env2,decay_env2,release_env2,chorus;
    std::uint8_t expected_cutoff,volume;
    std::uint16_t expected_delay,expected_freq;
    std::uint16_t expected_hold,expected_decay,expected_release,expected_chorus;
};
}
int main() {
    // Original private Win95 x86 executed SF2: DOS/DEMO/SYNTHGM.SF2.
    // Only independently reported numerical fields, NOT private bank,
    // ROM/driver bytes or 148-byte original descriptor payloads.
    constexpr std::array<OriginalReference,4> known{{
        {0,0,150,179,-8800,-12000,-725,-799,4386,-148,0,
         179,58,32768,128,120,3,41,0},
        {40,0,82,211,-7972,-2332,-837,-6772,3975,-667,0,
         149,50,32388,122,127,4,48,0},
        {100,0,0,313,-11958,-12000,460,-6772,2733,1699,466,
         255,42,32768,254,127,9,17,118},
        {100,1,0,344,-11958,-12000,-725,-6772,2733,1699,482,
         255,42,32768,128,127,9,17,122}
    }};
    unsigned tested=0;
    for(const auto& v:known) {
        CreativeSf2VoiceDescriptorBytes raw{};
        // Distinct made-up metadata sentinel: original address/pitch
        // structures must be preserved, except original sampleType flag.
        std::fill(raw.begin()+0x76u,raw.end(),0x5au);
        set(raw,0x8cu/2u,0x8001); // original ROM mono
        set(raw,8,v.cutoff);
        set(raw,9,0);
        set(raw,15,v.chorus);
        set(raw,16,109);  // original bank send -> 27, device flag zero
        set(raw,17,0);
        set(raw,18,0);
        set(raw,19,0);
        for(const unsigned op:{21u,23u,25u,27u,28u,30u,33u})
            set(raw,op,-12000);
        set(raw,21,v.delay_lfo1);
        set(raw,22,v.freq_lfo1);
        set(raw,24,-15600);
        set(raw,26,-8800);
        set(raw,29,0);
        set(raw,34,v.attack);
        set(raw,35,v.hold_env2);
        set(raw,36,v.decay_env2);
        set(raw,37,v.program==40?127:0);
        set(raw,38,v.release_env2);
        set(raw,48,v.amplitude);
        OriginalCreativeVolumeTerms volume{};
        volume.cc7=100;volume.cc11=127;
        volume.device_state_additive_attenuation=0;
        const OriginalSf2VoiceStartConditions ctx{
            0,60,90,64,0,0,0,volume};
        const auto after=candidateOriginalSf2EncodedVoiceDescriptor(raw,ctx);
        CHECK(after);
        CHECK(get(*after,8)==v.expected_cutoff);
        CHECK(get(*after,15)==v.expected_chorus);
        CHECK(get(*after,16)==27);
        CHECK(get(*after,17)==127 && get(*after,18)==129);
        CHECK(get(*after,19)==v.volume);
        CHECK(get(*after,21)==v.expected_delay);
        CHECK(get(*after,22)==v.expected_freq);
        CHECK(get(*after,23)==32768 && get(*after,24)==0);
        CHECK(get(*after,26)==125);
        CHECK(get(*after,34)==(v.program==0?125u:v.program==40?114u:128u));
        CHECK(get(*after,35)==v.expected_hold);
        CHECK(get(*after,36)==v.expected_decay);
        CHECK(get(*after,38)==v.expected_release);
        CHECK(get(*after,0x8cu/2u)==0x0001u); // found by full byte comparison
        // Actual sample/pitch/address metadata stays untouched.
        for(unsigned i=0x76u;i<0x94u;++i)
            if(i!=0x8cu && i!=0x8du)CHECK((*after)[i]==raw[i]);
        auto invalid=raw;
        set(invalid,0x8cu/2u,1); // RAM sample: not a ROM descriptor
        CHECK(!candidateOriginalSf2EncodedVoiceDescriptor(invalid,ctx));
        set(invalid,0x8cu/2u,0x8002); // ROM right/link: unsupported
        CHECK(!candidateOriginalSf2EncodedVoiceDescriptor(invalid,ctx));
        auto bad=ctx;
        bad.midi_velocity=0;
        CHECK(!candidateOriginalSf2EncodedVoiceDescriptor(raw,bad));
        bad=ctx;bad.midi_key=128;
        CHECK(!candidateOriginalSf2EncodedVoiceDescriptor(raw,bad));
        bad=ctx;bad.channel_pan=128;
        CHECK(!candidateOriginalSf2EncodedVoiceDescriptor(raw,bad));
        ++tested;
    }
    // The four known private 148-byte before/after vectors were also
    // replayed outside this repository without publishing original bytes:
    // all four compared byte-identically with source-derived volume terms.
    std::cout<<"P002_NATIVE_SF2_148BYTE_DESCRIPTOR_PASS cases="<<tested
             <<" original_sampleType_8001_to_0001=TRUE"
             <<" original_full_driver_pcm_parity=NOT_PROVEN\n";
}
