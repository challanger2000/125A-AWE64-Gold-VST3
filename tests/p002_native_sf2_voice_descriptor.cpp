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
    // ORIGINAL FORMAT-2 builder (PCOD +0x3985/+0x3AF7) applies the
    // source's two supplied descriptor templates, local-over-global
    // resolution and signed saturating 59-WORD merge. Here templates
    // are explicitly synthetic: these tests certify arithmetic/ownership,
    // NOT reconstructed original LCOD+0x2D88 default template bytes.
    std::array<std::int16_t,59> presetTemplate{},instrumentTemplate{};
    instrumentTemplate[8]=313; // observed GM100 template cutoff
    instrumentTemplate[34]=-12000;
    instrumentTemplate[56]=100;
    instrumentTemplate[43]=static_cast<std::int16_t>(0x7f00);
    instrumentTemplate[44]=static_cast<std::int16_t>(0x7f00);
    presetTemplate[14]=30000;instrumentTemplate[14]=30000;
    presetTemplate[50]=-30000;instrumentTemplate[50]=-30000;
    SbkLayer fullLayer{};
    fullLayer.source_format=OriginalCreativeBankFormat::Format2Sf2;
    fullLayer.bank=0u;fullLayer.program=0u;
    fullLayer.key=60u;fullLayer.velocity=90u;
    fullLayer.instrument=0u;fullLayer.rom_sample_selector=3u;
    fullLayer.preset_global.raw[48]=5;
    fullLayer.preset_local.raw[48]=7;
    fullLayer.instrument_global.raw[48]=9;
    fullLayer.instrument_local.raw[48]=11;
    fullLayer.instrument_global.raw[8]=10856;
    fullLayer.instrument_local.raw[8]=9617;
    fullLayer.instrument_global.raw[34]=static_cast<std::uint16_t>(-8800);
    fullLayer.instrument_local.raw[43]=16954;
    fullLayer.instrument_local.raw[53]=3;
    fullLayer.instrument_local.raw[52]=4;
    fullLayer.instrument_local.raw[58]=77;
    fullLayer.preset_local.raw[46]=127; // original preset keynum zeroed
    fullLayer.instrument_local.raw[47]=7; // actual NoteOn velocity wins
    const auto built=candidateOriginalSf2GeneratorsWithVerifiedTemplates(
        fullLayer,presetTemplate,instrumentTemplate);
    CHECK(built);
    CHECK(get(*built,8)==179u);
    CHECK(get(*built,34)==static_cast<std::uint16_t>(-8800));
    CHECK(get(*built,48)==18u);
    CHECK(get(*built,43)==0x7f00u);
    CHECK(get(*built,44)==0x7f00u);
    // The actual original SF2 driver selected these four different zone
    // ranges, but every final pre-start voice descriptor kept 0x7F00.
    for(const std::uint16_t rawRange:{16954u,17920u,27392u,29440u}) {
        auto zone=fullLayer;
        zone.instrument_local.raw[43]=rawRange;
        zone.instrument_local.raw[44]=static_cast<std::uint16_t>(0x423au);
        const auto afterSelection=candidateOriginalSf2GeneratorsWithVerifiedTemplates(
            zone,presetTemplate,instrumentTemplate);
        CHECK(afterSelection && get(*afterSelection,43)==0x7f00u &&
              get(*afterSelection,44)==0x7f00u);
    }
    CHECK(get(*built,52)==4u && get(*built,58)==77u);
    CHECK(get(*built,53)==3u);
    CHECK(get(*built,46)==60u && get(*built,47)==90u);
    CHECK(get(*built,14)==32767u);
    CHECK(get(*built,50)==static_cast<std::uint16_t>(-32768));
    CHECK(get(*built,56)==100u);
    CHECK(std::all_of(built->begin()+0x76u,built->end(),
        [](std::uint8_t x){return x==0u;}));
    auto badLayer=fullLayer;
    badLayer.instrument_local.raw[53]=4; // cannot silently play wrong sample
    CHECK(!candidateOriginalSf2GeneratorsWithVerifiedTemplates(
        badLayer,presetTemplate,instrumentTemplate));
    badLayer=fullLayer;badLayer.source_format=OriginalCreativeBankFormat::Format1Sbk;
    CHECK(!candidateOriginalSf2GeneratorsWithVerifiedTemplates(
        badLayer,presetTemplate,instrumentTemplate));
    badLayer=fullLayer;badLayer.velocity=0u;
    CHECK(!candidateOriginalSf2GeneratorsWithVerifiedTemplates(
        badLayer,presetTemplate,instrumentTemplate));
    // Source-backed SF2 shdr metadata -> exact 148-byte descriptor tail.
    // The original sample-rate->dwPitch conversion remains separately
    // verified by its original x86 helper; no host log2 approximation.
    CreativeSf2VoiceDescriptorBytes generatorWords{};
    set(generatorWords,8,179);
    SbkRomAddress originalRom{};
    originalRom.source_format=OriginalCreativeBankFormat::Format2Sf2;
    originalRom.sample_id=3;
    originalRom.start_word=100;
    originalRom.end_word=1000;
    originalRom.loop_start_word=200;
    originalRom.loop_end_word=900;
    originalRom.sample_rate_hz=44100;
    originalRom.original_pitch=60;
    originalRom.pitch_correction=-7;
    originalRom.sample_link=0;
    originalRom.sample_type=0x8001;
    const auto withSample=candidateOriginalSf2VoiceWithVerifiedRomSampleMetadata(
        generatorWords,originalRom,0x00e00000u,40u);
    CHECK(withSample);
    const auto readDword=[](const CreativeSf2VoiceDescriptorBytes& b,
                            unsigned off)->std::uint32_t {
        return static_cast<std::uint32_t>(b[off]) |
            (static_cast<std::uint32_t>(b[off+1])<<8) |
            (static_cast<std::uint32_t>(b[off+2])<<16) |
            (static_cast<std::uint32_t>(b[off+3])<<24);
    };
    CHECK(readDword(*withSample,0x76u)==100u);
    CHECK(readDword(*withSample,0x7au)==1000u);
    CHECK(readDword(*withSample,0x7eu)==200u);
    CHECK(readDword(*withSample,0x82u)==900u);
    CHECK(readDword(*withSample,0x86u)==0x00e00000u);
    CHECK(readDword(*withSample,0x90u)==40u);
    CHECK((*withSample)[0x8au]==60u && (*withSample)[0x8bu]==249u);
    CHECK(get(*withSample,0x8cu/2u)==0x8001u);
    CHECK(get(*withSample,0x8eu/2u)==0u);
    // All original 59 premerged generator words stay byte-identical.
    CHECK(std::equal(generatorWords.begin(),generatorWords.begin()+0x76u,
                     withSample->begin()));
    // Continue the SAME input from sample header to NOTE-START encoding.
    // The original writer changes the sampleType qualifier, but neither
    // reassigns the authentic sample addresses nor re-invents dwPitch.
    OriginalCreativeVolumeTerms explicitVolume{};
    explicitVolume.device_state_additive_attenuation=0u;
    const OriginalSf2VoiceStartConditions startContext{
        0u,60u,90u,64u,0u,0u,0u,explicitVolume};
    const auto started=candidateOriginalSf2EncodedVoiceDescriptor(
        *withSample,startContext);
    CHECK(started && get(*started,0x8cu/2u)==0x0001u);
    for(const unsigned off:{0x76u,0x7au,0x7eu,0x82u,0x86u,0x90u})
        CHECK(readDword(*started,off)==readDword(*withSample,off));
    CHECK((*started)[0x8au]==60u && (*started)[0x8bu]==249u);
    CHECK(get(*started,8u)==127u); // synthetic cutoff179 @velocity90
    CHECK(get(*started,19u)==42u); // native CC7/CC11/ROM volume, no gen48
    CHECK(!candidateOriginalSf2VoiceWithVerifiedRomSampleMetadata(
        generatorWords,originalRom,std::nullopt,40u));
    auto invalidRom=originalRom;
    invalidRom.source_format=OriginalCreativeBankFormat::Format1Sbk;
    CHECK(!candidateOriginalSf2VoiceWithVerifiedRomSampleMetadata(
        generatorWords,invalidRom,0x00e00000u,40u));
    invalidRom=originalRom;invalidRom.sample_type=0x0001;
    CHECK(!candidateOriginalSf2VoiceWithVerifiedRomSampleMetadata(
        generatorWords,invalidRom,0x00e00000u,40u));
    invalidRom=originalRom;invalidRom.sample_type=0x8002;
    CHECK(!candidateOriginalSf2VoiceWithVerifiedRomSampleMetadata(
        generatorWords,invalidRom,0x00e00000u,40u));
    invalidRom=originalRom;invalidRom.loop_end_word=1001;
    CHECK(!candidateOriginalSf2VoiceWithVerifiedRomSampleMetadata(
        generatorWords,invalidRom,0x00e00000u,40u));
    invalidRom=originalRom;invalidRom.sample_rate_hz=0;
    CHECK(!candidateOriginalSf2VoiceWithVerifiedRomSampleMetadata(
        generatorWords,invalidRom,0x00e00000u,40u));
    // The four known private 148-byte before/after vectors were also
    // replayed outside this repository without publishing original bytes:
    // all four compared byte-identically with source-derived volume terms.
    std::cout<<"P002_NATIVE_SF2_148BYTE_DESCRIPTOR_PASS cases="<<tested
             <<" original_sampleType_8001_to_0001=TRUE"
             <<" original_full_driver_pcm_parity=NOT_PROVEN\n";
}
