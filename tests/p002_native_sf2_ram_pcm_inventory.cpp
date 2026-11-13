#include "CreativeSf2RamSamplePcm.h"
#include "CreativeSbkLayers.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>
using namespace awe64::creative;
using Bytes=std::vector<std::uint8_t>;
#define CHECK(x) do { if(!(x)) { \
    std::cerr<<"P002_RAM_SF2_PCM_FAIL line="<<__LINE__ \
             <<" "#x<<"\n"; return 1; } } while(false)
namespace {
void w16(Bytes& b,std::uint32_t x) {
    b.push_back(static_cast<std::uint8_t>(x));
    b.push_back(static_cast<std::uint8_t>(x>>8u));
}
void w32(Bytes& b,std::uint32_t x) {w16(b,x);w16(b,x>>16u);}
void name(Bytes& b,std::string_view s) {
    b.insert(b.end(),s.begin(),s.end());
    b.insert(b.end(),20u-s.size(),0u);
}
void chunk(Bytes& b,std::string_view tag,const Bytes& data) {
    b.insert(b.end(),tag.begin(),tag.end());
    w32(b,static_cast<std::uint32_t>(data.size()));
    b.insert(b.end(),data.begin(),data.end());
    if(data.size()&1u)b.push_back(0u);
}
Bytes make(
    std::uint16_t sampleType=1u,
    std::uint32_t start=8u,std::uint32_t end=40u,
    std::uint32_t loopStart=12u,std::uint32_t loopEnd=36u,
    bool withSmpl=true,bool withSm24=false,bool duplicateSmpl=false,
    bool duplicateShdr=false,std::uint32_t rate=44100u,
    std::uint16_t sampleLink=0u,bool withMidiPreset=false,
    bool genericSf2Metadata=false) {
    Bytes info,sdta,pdta,shdr,smpl;
    Bytes version{};w16(version,2u);w16(version,1u);
    chunk(info,"ifil",version);
    if(withMidiPreset && !genericSf2Metadata) {
        chunk(info,"isng",Bytes{'E','M','U','8','0','0','0',0});
        chunk(info,"IPRD",Bytes{'S','B','A','W','E','3','2',0});
        chunk(info,"irom",Bytes{'1','M','G','M',0});
    }
    for(unsigned i=0;i<64u;++i)
        w16(smpl,static_cast<std::uint16_t>(
            i==8u?0x8001u:i==9u?0x7fffu:i*109u));
    if(withSmpl)chunk(sdta,"smpl",smpl);
    if(duplicateSmpl)chunk(sdta,"smpl",smpl);
    if(withSm24)chunk(sdta,"sm24",Bytes(64u,0u));
    if(withMidiPreset) {
        Bytes phdr,pbag,pgen,inst,ibag,igen;
        name(phdr,"RAM-PRESET");
        w16(phdr,7u);w16(phdr,0u);w16(phdr,0u);
        w32(phdr,0u);w32(phdr,0u);w32(phdr,0u);
        name(phdr,"EOP");
        w16(phdr,0u);w16(phdr,0u);w16(phdr,1u);
        w32(phdr,0u);w32(phdr,0u);w32(phdr,0u);
        w16(pbag,0u);w16(pbag,0u);
        w16(pbag,1u);w16(pbag,0u);
        w16(pgen,41u);w16(pgen,0u);
        name(inst,"RAM-INST");w16(inst,0u);
        name(inst,"EOI");w16(inst,1u);
        w16(ibag,0u);w16(ibag,0u);
        w16(ibag,1u);w16(ibag,0u);
        w16(igen,53u);w16(igen,0u);
        chunk(pdta,"phdr",phdr);
        chunk(pdta,"pbag",pbag);
        chunk(pdta,"pgen",pgen);
        chunk(pdta,"inst",inst);
        chunk(pdta,"ibag",ibag);
        chunk(pdta,"igen",igen);
    }
    name(shdr,"RAM-MONO");
    w32(shdr,start);w32(shdr,end);
    w32(shdr,loopStart);w32(shdr,loopEnd);
    w32(shdr,rate);
    shdr.push_back(60u);shdr.push_back(249u); // signed correction -7
    w16(shdr,sampleLink);w16(shdr,sampleType);
    name(shdr,"EOS");
    for(int k=0;k<5;++k)w32(shdr,0u);
    shdr.push_back(0u);shdr.push_back(0u);w16(shdr,0u);w16(shdr,0u);
    chunk(pdta,"shdr",shdr);
    if(duplicateShdr)chunk(pdta,"shdr",shdr);
    Bytes root{};
    Bytes listInfo{'I','N','F','O'};
    listInfo.insert(listInfo.end(),info.begin(),info.end());
    Bytes listSdta{'s','d','t','a'};
    listSdta.insert(listSdta.end(),sdta.begin(),sdta.end());
    Bytes listPdta{'p','d','t','a'};
    listPdta.insert(listPdta.end(),pdta.begin(),pdta.end());
    chunk(root,"LIST",listInfo);
    chunk(root,"LIST",listSdta);
    chunk(root,"LIST",listPdta);
    Bytes file{'R','I','F','F'};
    w32(file,static_cast<std::uint32_t>(root.size()+4u));
    file.insert(file.end(),{'s','f','b','k'});
    file.insert(file.end(),root.begin(),root.end());
    return file;
}
}
int main() {
    constexpr std::size_t capacityWords=0xE00000u; // 28 MiB, original EMU8000
    auto sf2=make();
    const auto valid=candidateOriginalSf2ReadRamMonoSamplePcm(
        sf2,0u,capacityWords);
    CHECK(valid && valid->sample_id==0u &&
          valid->start_word==8u && valid->end_word==40u &&
          valid->loop_start_word==12u &&
          valid->loop_end_word==36u &&
          valid->sample_rate_hz==44100u &&
          valid->original_pitch==60u &&
          valid->pitch_correction==-7 &&
          valid->total_pool_words==64u);
    CHECK(valid->sample_pcm_le16.size()==64u);
    CHECK(valid->sample_pcm_le16.data()!=nullptr);
    CHECK(valid->sample_pcm_le16[0]==1u &&
          valid->sample_pcm_le16[1]==0x80u &&
          valid->sample_pcm_le16[2]==0xffu &&
          valid->sample_pcm_le16[3]==0x7fu);
    CHECK(valid->sample_pcm_le16.data()>=sf2.data() &&
          valid->sample_pcm_le16.data()<sf2.data()+sf2.size());
    const auto midiBank=make(1u,8u,40u,12u,36u,true,false,
                             false,false,44100u,0u,true);
    const auto sourceNote=candidateOriginalSf2RamMonoNotePcmPlan(
        midiBank,0u,7u,60u,100u,capacityWords);
    CHECK(sourceNote && sourceNote->count==1u &&
          sourceNote->bank==0u && sourceNote->program==7u &&
          sourceNote->key==60u && sourceNote->velocity==100u &&
          sourceNote->layers[0].sample_id==0u &&
          sourceNote->layers[0].sample_pcm_le16.size()==64u &&
          sourceNote->layers[0].sample_pcm_le16[0]==1u);
    // A NORMAL format-2 RAM SoundFont should not need the proprietary
    // original Creative 1MGM/EMU8000/SBAWE32 INFO fields. Preserve
    // the strict original ROM parser gate separately from RAM SF2.
    const auto genericBank=make(
        1u,8u,40u,12u,36u,true,false,false,false,
        44100u,0u,true,true);
    CHECK(!resolveCreativeSbkLayers(genericBank,0u,7u,60u,100u));
    const auto generic=candidateStandardSf2RamLayers(
        genericBank,0u,7u,60u,100u);
    CHECK(generic && generic->count==1u &&
          generic->layers[0].rom_sample_selector==0u &&
          generic->layers[0].source_format==
              OriginalCreativeBankFormat::Format2Sf2);
    const auto genericPcm=candidateOriginalSf2RamMonoNotePcmPlan(
        genericBank,0u,7u,60u,100u,capacityWords);
    CHECK(genericPcm && genericPcm->count==1u &&
          genericPcm->layers[0].sample_id==0u &&
          genericPcm->layers[0].sample_pcm_le16.size()==64u &&
          genericPcm->layers[0].sample_pcm_le16[0]==1u);
    CHECK(!candidateOriginalSf2RamMonoNotePcmPlan(
        genericBank,0u,7u,60u,100u,31u));
    const auto unselected=candidateOriginalSf2RamMonoNotePcmPlan(
        midiBank,0u,6u,60u,100u,capacityWords);
    CHECK(unselected && unselected->count==0u);
    CHECK(!candidateOriginalSf2RamMonoNotePcmPlan(
        midiBank,0u,7u,60u,100u,31u));
    const auto invalidRamZone=make(0x8001u,8u,40u,12u,36u,
        true,false,false,false,44100u,0u,true);
    CHECK(!candidateOriginalSf2RamMonoNotePcmPlan(
        invalidRamZone,0u,7u,60u,100u,capacityWords));
    CHECK(!candidateOriginalSf2RamMonoNotePcmPlan(
        sf2,0u,7u,60u,100u,capacityWords));
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(sf2,1u,capacityWords));
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(sf2,0u,0u));
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(sf2,0u,31u));
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        sf2,0u,capacityWords+1u));
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(0x8001u),0u,capacityWords)); // no ROM mistaken as RAM
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(2u),0u,capacityWords)); // stereo/linked not proven
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(1u,8u,40u,12u,36u,true,false,false,false,44100u,1u),
        0u,capacityWords)); // spurious link
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(1u,8u,65u),0u,capacityWords)); // over smpl pool
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(1u,40u,8u),0u,capacityWords)); // inverted end
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(1u,8u,40u,11u,41u),0u,capacityWords));
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(1u,8u,40u,36u,12u),0u,capacityWords));
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(1u,8u,40u,12u,36u,false),0u,capacityWords));
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(1u,8u,40u,12u,36u,true,true),0u,capacityWords));
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(1u,8u,40u,12u,36u,true,false,true),0u,capacityWords));
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(1u,8u,40u,12u,36u,true,false,false,true),
        0u,capacityWords));
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        make(1u,8u,40u,12u,36u,true,false,false,false,0u),
        0u,capacityWords));
    auto wrongRiff=sf2;wrongRiff[4u]^=1u;
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        wrongRiff,0u,capacityWords));
    auto trunc=sf2;trunc.pop_back();
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        trunc,0u,capacityWords));
    auto noForm=sf2;noForm[8u]^=1u;
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        noForm,0u,capacityWords));
    auto wrongMajor=sf2;
    // INFO ifil: first nested chunk begins at byte 24, data at 32.
    // Actual position is fixed by THIS test's minimal RIFF builder.
    CHECK(wrongMajor[32u]==2u);
    wrongMajor[32u]=1u;
    CHECK(!candidateOriginalSf2ReadRamMonoSamplePcm(
        wrongMajor,0u,capacityWords));
    std::cout<<"P002_RAM_SF2_PCM_INVENTORY_SOURCE_DATA_PASS"
             <<" real_SF2_sdta_smpl_16bit_PCM_extract=TRUE"
             <<" source_original_MIDI_program_zone_to_real_RAM_PCM=TRUE"
             <<" generic_non_1MGM_format2_RAM_SF2_preset_note_sample=TRUE"
             <<" original_ROM_only_Creative_metadata_gate_unchanged=TRUE"
             <<" standard_46byte_RAM_shdr_boundaries=TRUE"
             <<" original_28MiB_EMU8000_capacity_enforced=TRUE"
             <<" ROM_stereo_sm24_EOS_and_truncation_rejected=TRUE"
             <<" original_Win95_RAM_allocation_and_upload=NOT_PROVEN\n";
    return 0;
}
