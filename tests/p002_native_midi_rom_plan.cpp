#include "CreativeMidiRomPlan.h"
#include "CreativeSf2VoiceDescriptor.h"
#include "CreativeSf2RomBankNoteCompiler.h"
#include "Chip.h"
#include <algorithm>
#include <vector>
#include <string_view>
#include <iostream>
#include <array>
#include <filesystem>
#include <fstream>
#include <system_error>
using namespace awe64::creative;
using Bytes=std::vector<std::uint8_t>;
#define CHECK(c) do{if(!(c)){std::cerr<<"P002_ROM_PLAN_FAIL line="<<__LINE__<<" "<<#c<<"\n";return 1;}}while(0)
void w16(Bytes&v,unsigned n){v.push_back(n&255);v.push_back((n>>8)&255);}
void w32(Bytes&v,unsigned n){w16(v,n);w16(v,n>>16);}
void name(Bytes&v,std::string_view s){v.insert(v.end(),s.begin(),s.end());v.insert(v.end(),20-s.size(),0);}
void c(Bytes&v,std::string_view k,const Bytes&b){v.insert(v.end(),k.begin(),k.end());w32(v,b.size());v.insert(v.end(),b.begin(),b.end());if(b.size()&1)v.push_back(0);}
Bytes fixture(const bool format2=false,const unsigned matchingZones=1u,
              const bool invalidLastZoneSample=false){
    Bytes phdr,pbag,pgen,inst,ibag,igen,shdr,info,pdta,root;
    name(phdr,"SYNTHETIC");w16(phdr,7);w16(phdr,0);w16(phdr,0);w32(phdr,0);w32(phdr,0);w32(phdr,0);
    name(phdr,"SYNTHETICDRUM");w16(phdr,0);w16(phdr,128);w16(phdr,1);w32(phdr,0);w32(phdr,0);w32(phdr,0);
    name(phdr,"EOP");w16(phdr,0);w16(phdr,0);w16(phdr,2);w32(phdr,0);w32(phdr,0);w32(phdr,0);
    for(unsigned n:{0u,1u,2u}){w16(pbag,n);w16(pbag,0);}
    for(unsigned n=0;n<2;++n){w16(pgen,41);w16(pgen,0);}
    name(inst,"PIANO");w16(inst,0);name(inst,"EOI");w16(inst,matchingZones);
    for(unsigned n=0;n<=matchingZones;++n){w16(ibag,2u*n);w16(ibag,0);}
    for(unsigned n=0;n<matchingZones;++n){
        w16(igen,8);w16(igen,format2?9617u:89u);
        w16(igen,53);
        // EOS sampleID1 is intentionally invalid as ROM audio. A
        // seventh zone referencing EOS is NOT among the six original
        // started voices; the sixth zone's invalid sample must fail.
        w16(igen,(invalidLastZoneSample&&n+1u==matchingZones)?1u:0u);
    }
    if(format2){
        // Format2: two standard 46-byte headers, ROM mono and EOS.
        // Exercises actual MIDI->layer->ROM planning, not just reader tests.
        name(shdr,"ROM-MONO");w32(shdr,5);w32(shdr,25);
        w32(shdr,10);w32(shdr,20);w32(shdr,44100);
        shdr.push_back(60);shdr.push_back(0);w16(shdr,0);w16(shdr,0x8001);
        name(shdr,"EOS");w32(shdr,0);w32(shdr,0);
        w32(shdr,0);w32(shdr,0);w32(shdr,44100);
        shdr.push_back(60);shdr.push_back(0);w16(shdr,0);w16(shdr,0);
        Bytes ifil;w16(ifil,2);w16(ifil,0);c(info,"ifil",ifil);
    }else{
        // Format1: three 16-byte ROM entries per 48-byte row.
        shdr.assign(96,0);
        for(unsigned i=0;i<4;++i){
            const unsigned value=(i==0?5:i==1?25:i==2?10:20);
            for(unsigned j=0;j<4;++j)shdr[i*4+j]=(value>>(8*j))&255;
        }
    }
    c(info,"isng",Bytes{'E','M','U','8','0','0','0',0});c(info,"IPRD",Bytes{'S','B','A','W','E','3','2',0});c(info,"irom",Bytes{'1','M','G','M',0});
    Bytes i{'I','N','F','O'};i.insert(i.end(),info.begin(),info.end());
    Bytes p{'p','d','t','a'};c(p,"phdr",phdr);c(p,"pbag",pbag);c(p,"pgen",pgen);c(p,"inst",inst);c(p,"ibag",ibag);c(p,"igen",igen);c(p,"shdr",shdr);
    c(root,"LIST",i);c(root,"LIST",p);
    Bytes b{'R','I','F','F'};w32(b,root.size()+4);b.insert(b.end(),{'s','f','b','k'});b.insert(b.end(),root.begin(),root.end());return b;
}
int main(){
    const auto sbk=fixture();MidiState midi;
    auto pg=decodeWinMmShort(0x000007c2u);CHECK(pg && pg->kind==MidiKind::Program);midi.apply(*pg);
    auto on=decodeWinMmShort(0x00643c92u);CHECK(on && on->channel==2);
    auto plan=prepareOriginalCreativeRomNote(*on,midi,sbk,0,524288);
    CHECK(plan && plan->count==1 && plan->program==7 && plan->channel==2);
    CHECK(plan->layers[0].address.start_word==5 && plan->layers[0].address.loop_end_word==20);
    CHECK(plan->layers[0].metadata.instrumentRawCutoff()==89);
    CHECK(plan->layers[0].address.source_format==
          OriginalCreativeBankFormat::Format1Sbk);
    CHECK(plan->layers[0].address.sample_rate_hz==0u);
    auto wrongBank=prepareOriginalCreativeRomNote(*on,midi,sbk,128,524288);
    CHECK(wrongBank && wrongBank->count==0);
    auto noProgram=decodeWinMmShort(0x00643c93u);CHECK(noProgram);
    auto unselected=prepareOriginalCreativeRomNote(*noProgram,midi,sbk,0,524288);
    CHECK(unselected && unselected->count==0);
    auto kick=decodeWinMmShort(0x00642499u);CHECK(kick);
    auto drum=prepareOriginalCreativeRomNote(*kick,midi,sbk,128,524288);
    CHECK(drum && drum->count==1 && drum->original_sbk_bank==128);
    CHECK(!prepareOriginalCreativeRomNote(*kick,midi,sbk,128,24));
    auto off=decodeWinMmShort(0x00003c92u);CHECK(off && off->kind==MidiKind::NoteOff);
    CHECK(!prepareOriginalCreativeRomNote(*off,midi,sbk,0,524288));
    auto zero=decodeWinMmShort(0x00003c92u);CHECK(zero);
    CHECK(!prepareOriginalCreativeRomNote(*zero,midi,sbk,0,524288));
    // Switching programs via channel state is live; no GM-ID special-case.
    auto pg0=decodeWinMmShort(0x000000c2u);CHECK(pg0);midi.apply(*pg0);
    auto switched=prepareOriginalCreativeRomNote(*on,midi,sbk,0,524288);
    CHECK(switched && switched->count==0);
    // Format2 must work through the existing MIDI-to-ROM note planner.
    midi.apply(*pg);
    const auto sf2=fixture(true);
    const auto plannedSf2=prepareOriginalCreativeRomNote(*on,midi,sf2,0,524288);
    CHECK(plannedSf2 && plannedSf2->count==1 &&
          plannedSf2->program==7 && plannedSf2->channel==2);
    CHECK(plannedSf2->layers[0].metadata.source_format==
          OriginalCreativeBankFormat::Format2Sf2);
    CHECK(plannedSf2->layers[0].address.start_word==5 &&
          plannedSf2->layers[0].address.end_word==25 &&
          plannedSf2->layers[0].address.loop_end_word==20);
    CHECK(plannedSf2->layers[0].address.sample_rate_hz==44100u);
    CHECK(plannedSf2->layers[0].address.original_pitch==60u);
    CHECK(plannedSf2->layers[0].address.pitch_correction==0);
    CHECK(plannedSf2->layers[0].address.sample_type==0x8001u);
    // Synthetic PHDR index 0 carries MIDI program 7: group is zero,
    // NOT the MIDI ProgramChange number 7.
    CHECK(plannedSf2->layers[0].metadata.original_preset_row_index==0u);
    // Continuous, bounded native P002 path: WinMM channel/program/key/vel
    // -> FORMAT-2 original bank layer -> source cutoff PCOD conversion ->
    // verified 46-byte ROM sample header -> exact 148-byte NOTE START.
    // Unknown original template fields and sample-rate->dwPitch are not
    // guessed: they are caller-owned explicit source-backed inputs.
    const auto sf2Cutoff=candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
        plannedSf2->layers[0].metadata,8u,0);
    CHECK(sf2Cutoff && *sf2Cutoff==179);
    // Synthetic EXPLICIT template words exercise the original 59-WORD
    // generator builder; no unknown Windows original template is inferred.
    std::array<std::int16_t,59> originalPresetTemplate{};
    std::array<std::int16_t,59> originalInstrumentTemplate{};
    originalInstrumentTemplate[56u]=100; // explicit SYNTHETIC scale
    originalInstrumentTemplate[58u]=-1;  // explicit root-from-shdr
    const auto verifiedGeneratorWords=candidateOriginalSf2GeneratorsWithVerifiedTemplates(
        plannedSf2->layers[0].metadata,
        originalPresetTemplate,originalInstrumentTemplate);
    CHECK(verifiedGeneratorWords);
    const auto genWord=[&](const unsigned op)->std::uint16_t {
        return static_cast<std::uint16_t>(
            (*verifiedGeneratorWords)[op*2u] |
            (static_cast<unsigned>((*verifiedGeneratorWords)[op*2u+1u])<<8));
    };
    CHECK(genWord(8u)==static_cast<std::uint16_t>(*sf2Cutoff));
    CHECK(genWord(46u)==60u && genWord(47u)==100u);
    CHECK(genWord(53u)==plannedSf2->layers[0].metadata.rom_sample_selector);
    // Public data contains no original VXD tables. Synthetic all-zero
    // tables reproduce the SOURCE-VERIFIED 44.1kHz index-zero anchor only.
    // The actual private original tables are checked separately.
    const OriginalCreativeSf2PitchTables syntheticPitchTables{};
    const auto beforeStart=candidateOriginalSf2VoiceWithOriginalRomPitchTables(
        *verifiedGeneratorWords,plannedSf2->layers[0].address,
        syntheticPitchTables,
        plannedSf2->layers[0].metadata.original_preset_row_index);
    CHECK(beforeStart && (*beforeStart)[0x8cu]==1u &&
          (*beforeStart)[0x8du]==128u);
    CHECK((*beforeStart)[0x90u]==0u &&
          (*beforeStart)[0x91u]==0u); // PHDR 0, not program 7
    CHECK((*beforeStart)[0x86u]==0u &&
          (*beforeStart)[0x87u]==0u &&
          (*beforeStart)[0x88u]==0xe0u &&
          (*beforeStart)[0x89u]==0u); // source exact 44.1k dwPitch
    OriginalCreativeVolumeTerms externalVolume{};
    externalVolume.device_state_additive_attenuation=0u;
    const OriginalSf2VoiceStartConditions conditions{
        0u,60u,100u,64u,0u,0u,0u,externalVolume};
    const auto started=candidateOriginalSf2EncodedVoiceDescriptor(
        *beforeStart,conditions);
    CHECK(started && (*started)[0x8cu]==1u &&
          (*started)[0x8du]==0u); // original 8001 -> 0001
    CHECK(std::equal(beforeStart->begin()+0x76u,
                     beforeStart->begin()+0x8cu,
                     started->begin()+0x76u)); // ROM addresses/dwPitch intact
    CHECK(std::equal(beforeStart->begin()+0x8eu,
                     beforeStart->end(),
                     started->begin()+0x8eu)); // sample link/context intact
    CHECK((*started)[0x8au]==60u && (*started)[0x8bu]==0u);
    // Complete original pitch STAGE after independently supplied dwPitch:
    // shdr orig-key=60, generator root=-1, scale=100, key60 => E000.
    const auto firstIp=candidateOriginalSf2InitialIpFromDescriptor(
        *started,60u,0,0);
    CHECK(firstIp && *firstIp==0xe000u);
    const auto sf2IpPorts=candidateOriginalSf2InitialIpWritesFromDescriptor(
        *started,60u,3u,0,0);
    CHECK(sf2IpPorts && (*sf2IpPorts)[0].port==0xe22u &&
          (*sf2IpPorts)[0].value==3u &&
          (*sf2IpPorts)[1].port==0xe20u &&
          (*sf2IpPorts)[1].value==0xe000u);
    // ACTUAL unified runtime-format2 ORIGINAL bank pipeline: the
    // existing SF2 parser, ORIGINAL generator template/pitch table
    // interpreter, live channel state, original VXD voice release
    // init and the source-ordered full 61-OUT register plan. No hand-
    // authored 148-byte after-NoteOn descriptor in this test.
    OriginalCreativeSf2DriverResources verifiedSyntheticResources{};
    verifiedSyntheticResources.preset_template=originalPresetTemplate;
    verifiedSyntheticResources.instrument_template=originalInstrumentTemplate;
    verifiedSyntheticResources.pitch_tables=syntheticPitchTables;
    std::array<std::uint8_t,kOriginalVxdMinDeviceImage> originalState{};
    originalState[kOriginalVxdRomIdOffset]=0x31u;
    originalState[kOriginalVxdRomIdOffset+1u]=0x4du;
    originalState[kOriginalVxdRomIdOffset+2u]=0x47u;
    originalState[kOriginalVxdRomIdOffset+3u]=0x4du;
    OriginalSf2VerifiedVxdDeviceSnapshot verifiedSyntheticVxd{};
    verifiedSyntheticVxd.original_state_bytes=originalState;
    verifiedSyntheticVxd.independently_verified_original_vxd_snapshot=true;
    verifiedSyntheticVxd.original_lcod_31word_table_verified=true;
    const std::array<std::uint8_t,1> allocated{{3u}};
    const auto compiled=candidateOriginalSf2CompileRomBankNote(
        sf2,*on,midi,0u,524288u,verifiedSyntheticResources,conditions,
        verifiedSyntheticVxd,allocated,true);
    CHECK(compiled && compiled->voice_count==1u &&
          compiled->typed_original_outs.count==61u);
    CHECK(compiled->assignments[0].assigned_voice.physical_voice==3u);
    CHECK(compiled->assignments[0].assigned_voice.verified_after_start_record==
          *started);
    CHECK(compiled->typed_original_outs.out[0].port==0xe22u);
    const auto row=kOriginalVxdVoiceRowStart+3u*kOriginalVxdVoiceRowStride;
    CHECK(compiled->vxd_state_after_noteon[row]==60u &&
          compiled->vxd_state_after_noteon[row+1u]==2u);
    CHECK(compiled->vxd_state_after_noteon[row+0x15u]==(*started)[0x3cu]);
    CHECK(compiled->vxd_state_after_noteon[row+0x16u]==(*started)[0x4cu]);
    CHECK(originalState[row]==0u && originalState[row+0x15u]==0u);
    // The physical original assignment is NOT guessed from MIDI or
    // from the preset number. A duplicate/busy source is refused.
    const std::array<std::uint8_t,1> invalidPhysical{{30u}};
    CHECK(!candidateOriginalSf2CompileRomBankNote(
        sf2,*on,midi,0u,524288u,verifiedSyntheticResources,conditions,
        verifiedSyntheticVxd,invalidPhysical,true));
    CHECK(!candidateOriginalSf2CompileRomBankNote(
        sf2,*on,midi,0u,524288u,verifiedSyntheticResources,conditions,
        verifiedSyntheticVxd,allocated,false));
    auto wrongConditions=conditions;
    wrongConditions.channel_pan=67;
    CHECK(!candidateOriginalSf2CompileRomBankNote(
        sf2,*on,midi,0u,524288u,verifiedSyntheticResources,wrongConditions,
        verifiedSyntheticVxd,allocated,true));
    wrongConditions=conditions;
    wrongConditions.volume.device_state_additive_attenuation=11;
    CHECK(!candidateOriginalSf2CompileRomBankNote(
        sf2,*on,midi,0u,524288u,verifiedSyntheticResources,wrongConditions,
        verifiedSyntheticVxd,allocated,true));
    CHECK(!candidateOriginalSf2CompileRomBankNote(
        sbk,*on,midi,0u,524288u,verifiedSyntheticResources,conditions,
        verifiedSyntheticVxd,allocated,true)); // format1 ≠ format2
    // P002: native SOURCE-PCOD RAM-SF2 compiler independently
    // generates 61 physical-voice OUTs from MIDI and parsed SF2.
    // Public test uses synthetic resources, never private Creative data.
    Bytes sourceRam=fixture(true);
    const Bytes romTag{'R','O','M','-','M','O','N','O'};
    const auto sampleAt=std::search(sourceRam.begin(),sourceRam.end(),
                                    romTag.begin(),romTag.end());
    CHECK(sampleAt!=sourceRam.end());
    const auto sampleTypeOffset=static_cast<std::size_t>(
        sampleAt-sourceRam.begin())+44u;
    CHECK(sampleTypeOffset+1u<sourceRam.size());
    sourceRam[sampleTypeOffset]=0x01u;
    sourceRam[sampleTypeOffset+1u]=0u;
    Bytes pool(64u*2u,0u);
    for(unsigned i=0;i<64u;++i) {
        pool[2u*i]=static_cast<std::uint8_t>(i);
        pool[2u*i+1u]=0u;
    }
    Bytes sdta{'s','d','t','a'};
    c(sdta,"smpl",pool);
    Bytes sdtaList;
    c(sdtaList,"LIST",sdta);
    sourceRam.insert(sourceRam.begin()+12u,sdtaList.begin(),sdtaList.end());
    const auto riffLength=static_cast<std::uint32_t>(sourceRam.size()-8u);
    for(unsigned i=0;i<4u;++i)
        sourceRam[4u+i]=static_cast<std::uint8_t>(riffLength>>(8u*i));
    const std::uint32_t sourceRamPhysicalBase=0x200010u;
    auto verifiedRamResources=verifiedSyntheticResources;
    verifiedRamResources.instrument_template[54u]=1; // synthetic LOOP mode
    const auto ramCompiled=candidateOriginalSf2CompileRamBankNote(
        sourceRam,*on,midi,0u,sourceRamPhysicalBase,64u,
        verifiedRamResources,conditions,verifiedSyntheticVxd,
        allocated,true);
    CHECK(ramCompiled && ramCompiled->voice_count==1u &&
          ramCompiled->typed_original_outs.count==61u);
    const auto readDw=[](const CreativeSf2VoiceDescriptorBytes& d,
                         const unsigned offset) noexcept {
        return static_cast<std::uint32_t>(d[offset])|
               (static_cast<std::uint32_t>(d[offset+1u])<<8u)|
               (static_cast<std::uint32_t>(d[offset+2u])<<16u)|
               (static_cast<std::uint32_t>(d[offset+3u])<<24u);
    };
    CHECK(readDw(ramCompiled->before_note_start[0],0x76u)==
          sourceRamPhysicalBase+5u);
    CHECK(readDw(ramCompiled->before_note_start[0],0x7eu)==
          sourceRamPhysicalBase+10u);
    CHECK(readDw(ramCompiled->before_note_start[0],0x82u)==
          sourceRamPhysicalBase+20u);
    CHECK(ramCompiled->before_note_start[0][0x8cu]==1u &&
          ramCompiled->before_note_start[0][0x8du]==0u);
    CHECK(ramCompiled->assignments[0].assigned_voice.physical_voice==3u);
    CHECK(ramCompiled->assignments[0].assigned_voice.played_midi_key==60u);
    CHECK(ramCompiled->assignments[0].assigned_voice.original_ram_sample_verified);
    CHECK(ramCompiled->typed_original_outs.out[0].port==0xe22u);
    // Hardware source order: 5 stop +18 prelude +16 middle +22 final.
    // Low 24-bit RAM addresses are generated from actual shdr words.
    const auto& op=ramCompiled->typed_original_outs.out;
    CHECK(op[39].port==0xe22u && op[39].value==0xc3u);
    CHECK(op[40].port==0x620u &&
          (op[40].value&0xffffffu)==sourceRamPhysicalBase+9u);
    CHECK(op[41].port==0xe22u && op[41].value==0xe3u);
    CHECK(op[42].port==0x620u &&
          (op[42].value&0xffffffu)==sourceRamPhysicalBase+19u);
    CHECK(op[43].port==0xe22u && op[43].value==3u);
    CHECK(op[44].port==0xa20u && op[44].value==sourceRamPhysicalBase+1u);
    CHECK(op[49].port==0xe22u && op[49].value==3u);
    CHECK(op[50].port==0xa20u &&
          (op[50].value&0xffffffu)==sourceRamPhysicalBase+1u);
    CHECK(!candidateOriginalSf2CompileRamBankNote(
        sourceRam,*on,midi,0u,sourceRamPhysicalBase,64u,
        verifiedSyntheticResources,conditions,verifiedSyntheticVxd,
        allocated,false));
    CHECK(!candidateOriginalSf2CompileRamBankNote(
        sourceRam,*on,midi,0u,sourceRamPhysicalBase,63u,
        verifiedSyntheticResources,conditions,verifiedSyntheticVxd,
        allocated,true));
    CHECK(!candidateOriginalSf2CompileRamBankNote(
        sourceRam,*on,midi,0u,0x1000u,64u,
        verifiedSyntheticResources,conditions,verifiedSyntheticVxd,
        allocated,true));
    CHECK(!candidateOriginalSf2CompileRamBankNote(
        sf2,*on,midi,0u,sourceRamPhysicalBase,64u,
        verifiedSyntheticResources,conditions,verifiedSyntheticVxd,
        allocated,true)); // ROM cannot be mistaken for RAM
    CHECK(!candidateOriginalSf2CompileRamBankNote(
        sourceRam,*on,midi,0u,sourceRamPhysicalBase,64u,
        verifiedSyntheticResources,conditions,verifiedSyntheticVxd,
        invalidPhysical,true));
    CHECK(!candidateOriginalSf2CompileRamBankNote(
        sourceRam,*on,midi,0u,sourceRamPhysicalBase,64u,
        verifiedSyntheticResources,wrongConditions,verifiedSyntheticVxd,
        allocated,true)); // stale MIDI channel/device state
    // The actual original Creative C4 used TWO physical voices.
    // Both source-selected layers must be compiled in one 122-OUT
    // transaction, without reusing or inventing a physical lane.
    auto sourceTwo=fixture(true,2u);
    const auto twoSampleAt=std::search(sourceTwo.begin(),sourceTwo.end(),
                                      romTag.begin(),romTag.end());
    CHECK(twoSampleAt!=sourceTwo.end());
    const auto twoSampleTypeOffset=static_cast<std::size_t>(
        twoSampleAt-sourceTwo.begin())+44u;
    sourceTwo[twoSampleTypeOffset]=1u;
    sourceTwo[twoSampleTypeOffset+1u]=0u;
    sourceTwo.insert(sourceTwo.begin()+12u,sdtaList.begin(),sdtaList.end());
    const auto twoRiff=static_cast<std::uint32_t>(sourceTwo.size()-8u);
    for(unsigned i=0;i<4u;++i)
        sourceTwo[4u+i]=static_cast<std::uint8_t>(twoRiff>>(8u*i));
    const std::array<std::uint8_t,2> sourceAllocated{{0u,3u}};
    const auto twoRam=candidateOriginalSf2CompileRamBankNote(
        sourceTwo,*on,midi,0u,sourceRamPhysicalBase,64u,
        verifiedRamResources,conditions,verifiedSyntheticVxd,
        sourceAllocated,true);
    CHECK(twoRam && twoRam->voice_count==2u &&
          twoRam->typed_original_outs.count==122u);
    CHECK(twoRam->assignments[0].assigned_voice.physical_voice==0u &&
          twoRam->assignments[1].assigned_voice.physical_voice==3u);
    CHECK(twoRam->assignments[0].assigned_voice.original_ram_sample_verified &&
          twoRam->assignments[1].assigned_voice.original_ram_sample_verified);
    CHECK(!candidateOriginalSf2CompileRamBankNote(
        sourceTwo,*on,midi,0u,sourceRamPhysicalBase,64u,
        verifiedRamResources,conditions,verifiedSyntheticVxd,
        std::array<std::uint8_t,2>{{3u,3u}},true));
    // First COMPLETE native ROM SF2 bank → SF2 descriptor → live
    // original HW-based physical allocator → source-ordered 61 OUTs →
    // genuine unchanged EMU8000. The chip sample ROM is synthetic and
    // discarded immediately; no Creative bytes are uploaded to GitHub.
    namespace fs=std::filesystem;
    const fs::path syntheticRom=fs::temp_directory_path()/
        "125a-p002-source-bank-live-mono-test.raw";
    {
        std::ofstream f(syntheticRom,std::ios::binary|std::ios::trunc);
        CHECK(f);
        for(std::uint32_t i=0;i<524288u;++i) {
            const std::int16_t sample=i>=5u && i<25u
                ?static_cast<std::int16_t>((static_cast<int>(i)-15)*1100)
                :0;
            const auto v=static_cast<std::uint16_t>(sample);
            f.put(static_cast<char>(v));
            f.put(static_cast<char>(v>>8u));
        }
        CHECK(f);
    }
    awe64::Chip liveChip{};
    awe64::Chip layeredChip{};
    awe64::Chip ramChip{};
    std::string chipError;
    CHECK(liveChip.open(syntheticRom,chipError));
    CHECK(layeredChip.open(syntheticRom,chipError));
    CHECK(ramChip.open(syntheticRom,chipError));
    std::error_code cleanup;
    fs::remove(syntheticRom,cleanup);
    originalState[6]=30u; // original VXD 30 music voices
    originalState[0x18u]=30u; // validated original allocation mode count
    for(unsigned i=0;i<30u;++i) {
        const auto at=kOriginalVxdVoiceRowStart+
                      i*kOriginalVxdVoiceRowStride;
        originalState[at]=0xfeu;originalState[at+1u]=0x0fu;
        liveChip.writePort(0xe22u,static_cast<std::uint16_t>(
            (3u<<5u)|i)); // original VTFT DATA0
        liveChip.writePort(0x620u,0u);
        liveChip.writePort(0x622u,0u);
        layeredChip.writePort(0xe22u,static_cast<std::uint16_t>(
            (3u<<5u)|i));
        layeredChip.writePort(0x620u,0u);
        layeredChip.writePort(0x622u,0u);
        ramChip.writePort(0xe22u,static_cast<std::uint16_t>(
            (3u<<5u)|i));
        ramChip.writePort(0x620u,0u);
        ramChip.writePort(0x622u,0u);
    }
    // REAL original VXD 30-slot selector integrated with newly
    // independent RAM SoundFont MIDI -> source PCOD -> full VXD chip
    // NOTE ON. This public case uses synthetic PCM (no user SF2),
    // never claims audible / identical original Creative Win95 PCM.
    auto realRamState=originalState;
    auto realRamContext=verifiedSyntheticVxd;
    realRamContext.original_state_bytes=realRamState;
    const auto ramStateBefore=realRamState;
    CHECK(!candidateOriginalSf2StartNextIdleLayeredRamBankNoteOnLiveChip(
        ramChip,realRamState,sourceTwo,*on,midi,0u,
        sourceRamPhysicalBase,64u,verifiedRamResources,
        conditions,realRamContext,false));
    CHECK(realRamState==ramStateBefore);
    const auto realRamStarted=
        candidateOriginalSf2StartNextIdleLayeredRamBankNoteOnLiveChip(
            ramChip,realRamState,sourceTwo,*on,midi,0u,
            sourceRamPhysicalBase,64u,verifiedRamResources,
            conditions,realRamContext,true);
    CHECK(realRamStarted &&
          realRamStarted->original_compilation.voice_count==2u &&
          realRamStarted->original_compilation.typed_original_outs.count==122u &&
          realRamStarted->selections[0].physical_voice==0u &&
          realRamStarted->selections[1].physical_voice==3u);
    const auto* originalRamChip=ramChip.rawForContractTesting();
    CHECK(originalRamChip && originalRamChip->voice[0].ip==0xe000u &&
          originalRamChip->voice[3].ip==0xe000u);
    OriginalSf2RomBankVoiceOwnership ramVoiceLedger{};
    CHECK(ramVoiceLedger.recordExecutedNoteOn(*realRamStarted,realRamState));
    CHECK(ramVoiceLedger.trackedPhysicalVoices()==2u);
    CHECK(ramVoiceLedger.controller64(ramChip,realRamState,2u,127u));
    const auto ramOff=ramVoiceLedger.noteOff(
        ramChip,realRamState,2u,60u);
    CHECK(ramOff && ramOff->deferred_by_sustain);
    const auto ramUp=ramVoiceLedger.controller64(
        ramChip,realRamState,2u,0u);
    CHECK(ramUp && ramUp->released_deferred_voices==2u &&
          ramVoiceLedger.trackedPhysicalVoices()==0u);
    const auto live=candidateOriginalSf2StartSingleRomBankLayerOnLiveChip(
        liveChip,sf2,*on,midi,0u,524288u,verifiedSyntheticResources,
        conditions,verifiedSyntheticVxd,true);
    CHECK(live && live->original_compilation.voice_count==1u &&
          live->original_compilation.typed_original_outs.count==61u &&
          live->original_selection.physical_voice==0u);
    const auto* originalChipState=liveChip.rawForContractTesting();
    CHECK(originalChipState && originalChipState->voice[0].ip==0xe000u);
    // Replaying the same initialized source state without a completed
    // NoteOff would select another *idle* physical voice; the controller
    // owning the VXD state must commit after successful source NoteOn.
    // This test checks the result's AFTER state, never inventing a
    // replacement state for a busy original voice.
    const auto afterRow=kOriginalVxdVoiceRowStart;
    CHECK(live->original_compilation.vxd_state_after_noteon[afterRow]==60u);
    CHECK(live->original_compilation.vxd_state_after_noteon[afterRow+1u]==2u);
    // Three consecutive MIDI NoteOns now carry the same actual ROM
    // SF2 bank, same original VXD resource tables, and the real pinned
    // chip through the original LIVE idle selector. The already-played
    // voice0 is retained in the source VXD state, not overwritten.
    auto sequentialState=live->original_compilation.vxd_state_after_noteon;
    OriginalSf2RomBankVoiceOwnership bankVoiceLedger{};
    CHECK(bankVoiceLedger.recordExecutedNoteOn(*live,sequentialState));
    CHECK(bankVoiceLedger.trackedPhysicalVoices()==1u);
    CHECK(!bankVoiceLedger.recordExecutedNoteOn(*live,sequentialState));
    auto sourceContext=verifiedSyntheticVxd;
    sourceContext.original_state_bytes=sequentialState;
    auto secondMidi=decodeWinMmShort(0x00644092u); // ch2, key64, vel100
    CHECK(secondMidi && secondMidi->channel==2u &&
          secondMidi->data1==64u);
    auto secondConditions=conditions;
    secondConditions.midi_key=64u;
    const auto unchangedBefore=sequentialState;
    CHECK(!candidateOriginalSf2StartNextIdleRomBankNoteOnLiveChip(
        liveChip,sequentialState,sf2,*secondMidi,midi,0u,524288u,
        verifiedSyntheticResources,secondConditions,sourceContext,false));
    CHECK(sequentialState==unchangedBefore);
    const auto secondLive=candidateOriginalSf2StartNextIdleRomBankNoteOnLiveChip(
        liveChip,sequentialState,sf2,*secondMidi,midi,0u,524288u,
        verifiedSyntheticResources,secondConditions,sourceContext,true);
    CHECK(secondLive &&
          secondLive->original_selection.physical_voice==3u &&
          secondLive->original_selection.reason==
              OriginalWin95MonoSelectionReason::CompletelyIdle &&
          secondLive->original_compilation.voice_count==1u);
    CHECK(bankVoiceLedger.recordExecutedNoteOn(*secondLive,sequentialState));
    CHECK(bankVoiceLedger.trackedPhysicalVoices()==2u);
    const auto secondRow=kOriginalVxdVoiceRowStart+
                         3u*kOriginalVxdVoiceRowStride;
    CHECK(sequentialState[afterRow]==60u &&
          sequentialState[afterRow+1u]==2u &&
          sequentialState[secondRow]==64u &&
          sequentialState[secondRow+1u]==2u);
    CHECK(originalChipState->voice[0].ip==0xe000u &&
          originalChipState->voice[3].ip!=0u &&
          originalChipState->voice[3].ip!=originalChipState->voice[0].ip);
    // The original second NoteOn committed state must feed the NEXT
    // allocation; no application-chosen increment or voice stealing.
    auto thirdMidi=decodeWinMmShort(0x00644392u); // ch2, key67, vel100
    CHECK(thirdMidi && thirdMidi->data1==67u);
    auto thirdConditions=conditions;
    thirdConditions.midi_key=67u;
    sourceContext.original_state_bytes=sequentialState;
    const auto thirdLive=candidateOriginalSf2StartNextIdleRomBankNoteOnLiveChip(
        liveChip,sequentialState,sf2,*thirdMidi,midi,0u,524288u,
        verifiedSyntheticResources,thirdConditions,sourceContext,true);
    CHECK(thirdLive && thirdLive->original_selection.physical_voice==6u);
    CHECK(bankVoiceLedger.recordExecutedNoteOn(*thirdLive,sequentialState));
    CHECK(bankVoiceLedger.trackedPhysicalVoices()==3u);
    const auto thirdRow=kOriginalVxdVoiceRowStart+
                        6u*kOriginalVxdVoiceRowStride;
    CHECK(sequentialState[afterRow]==60u &&
          sequentialState[secondRow]==64u &&
          sequentialState[thirdRow]==67u &&
          sequentialState[thirdRow+1u]==2u &&
          originalChipState->voice[6].ip!=0u);
    // Reject stale source context independently of the bank contents.
    auto staleSourceContext=verifiedSyntheticVxd;
    const auto allThreeState=sequentialState;
    CHECK(!candidateOriginalSf2StartNextIdleRomBankNoteOnLiveChip(
        liveChip,sequentialState,sf2,*thirdMidi,midi,0u,524288u,
        verifiedSyntheticResources,thirdConditions,staleSourceContext,true));
    CHECK(sequentialState==allThreeState);
    // CRITICAL BRIDGE: these three played keys are genuine source-SF2
    // bank-compiled voices, not synthetic private 148-byte host fixtures.
    // Reuse the original LCOD+0x6072 NoteOff and LCOD+0x656D CC64.
    const auto pedalDown=bankVoiceLedger.controller64(
        liveChip,sequentialState,2u,127u);
    CHECK(pedalDown && pedalDown->sustain_enabled &&
          pedalDown->typed_port_writes==0u &&
          bankVoiceLedger.trackedPhysicalVoices()==3u);
    const auto off60=bankVoiceLedger.noteOff(
        liveChip,sequentialState,2u,60u);
    const auto off64=bankVoiceLedger.noteOff(
        liveChip,sequentialState,2u,64u);
    const auto off67=bankVoiceLedger.noteOff(
        liveChip,sequentialState,2u,67u);
    CHECK(off60 && off64 && off67 &&
          off60->deferred_by_sustain &&
          off64->deferred_by_sustain &&
          off67->deferred_by_sustain &&
          off60->typed_port_writes==0u &&
          off64->typed_port_writes==0u &&
          off67->typed_port_writes==0u &&
          sequentialState[afterRow]==0xffu &&
          sequentialState[secondRow]==0xffu &&
          sequentialState[thirdRow]==0xffu &&
          bankVoiceLedger.trackedPhysicalVoices()==3u);
    CHECK(!bankVoiceLedger.noteOff(liveChip,sequentialState,2u,60u));
    const auto unaffectedBefore=sequentialState;
    // Untracked suspended physical voice must reject the ENTIRE
    // original pedal-up transaction before any source state mutation.
    const auto untrackedRow=kOriginalVxdVoiceRowStart+
        9u*kOriginalVxdVoiceRowStride;
    sequentialState[untrackedRow]=0xffu;
    sequentialState[untrackedRow+1u]=2u;
    CHECK(!bankVoiceLedger.controller64(liveChip,sequentialState,2u,0u));
    CHECK(sequentialState[afterRow]==0xffu &&
          sequentialState[secondRow]==0xffu &&
          sequentialState[thirdRow]==0xffu);
    sequentialState=unaffectedBefore;
    const auto pedalUp=bankVoiceLedger.controller64(
        liveChip,sequentialState,2u,0u);
    CHECK(pedalUp && !pedalUp->sustain_enabled &&
          pedalUp->released_deferred_voices==3u &&
          pedalUp->typed_port_writes==12u &&
          bankVoiceLedger.trackedPhysicalVoices()==0u);
    CHECK(sequentialState[afterRow]==0xfeu &&
          sequentialState[afterRow+1u]==0x0fu &&
          sequentialState[secondRow]==0xfeu &&
          sequentialState[secondRow+1u]==0x0fu &&
          sequentialState[thirdRow]==0xfeu &&
          sequentialState[thirdRow+1u]==0x0fu);
    CHECK(!bankVoiceLedger.noteOff(liveChip,sequentialState,2u,67u));
    // A single original SF2 MIDI NoteOn may match SIX instrument
    // zones. This regression tests three layers, with ONE original
    // hardware-selected completely-idle physical voice per layer,
    // one verified 183-OUT complete chip plan and one NoteOff/CC64
    // release covering all three original source-backed voices.
    auto layeredState=originalState; // original 30-slot all-idle image
    auto layeredContext=verifiedSyntheticVxd;
    layeredContext.original_state_bytes=layeredState;
    const auto threeZones=fixture(true,3u);
    const auto layeredBefore=layeredState;
    CHECK(!candidateOriginalSf2StartNextIdleLayeredRomBankNoteOnLiveChip(
        layeredChip,layeredState,threeZones,*on,midi,0u,524288u,
        verifiedSyntheticResources,conditions,layeredContext,false));
    CHECK(layeredState==layeredBefore);
    const auto wrongZone=fixture(true,3u,true);
    CHECK(!candidateOriginalSf2StartNextIdleLayeredRomBankNoteOnLiveChip(
        layeredChip,layeredState,wrongZone,*on,midi,0u,524288u,
        verifiedSyntheticResources,conditions,layeredContext,true));
    CHECK(layeredState==layeredBefore);
    const auto layered=
        candidateOriginalSf2StartNextIdleLayeredRomBankNoteOnLiveChip(
            layeredChip,layeredState,threeZones,*on,midi,0u,524288u,
            verifiedSyntheticResources,conditions,layeredContext,true);
    CHECK(layered && layered->original_compilation.voice_count==3u &&
          layered->original_compilation.typed_original_outs.count==183u);
    CHECK(layered->selections[0].physical_voice==0u &&
          layered->selections[1].physical_voice==3u &&
          layered->selections[2].physical_voice==6u);
    const auto layerRow0=kOriginalVxdVoiceRowStart;
    const auto layerRow3=kOriginalVxdVoiceRowStart+
                         3u*kOriginalVxdVoiceRowStride;
    const auto layerRow6=kOriginalVxdVoiceRowStart+
                         6u*kOriginalVxdVoiceRowStride;
    CHECK(layeredState[layerRow0]==60u &&
          layeredState[layerRow3]==60u &&
          layeredState[layerRow6]==60u &&
          layeredState[layerRow0+1u]==2u &&
          layeredState[layerRow3+1u]==2u &&
          layeredState[layerRow6+1u]==2u);
    const auto* layeredRegisters=layeredChip.rawForContractTesting();
    CHECK(layeredRegisters &&
          layeredRegisters->voice[0].ip==0xe000u &&
          layeredRegisters->voice[3].ip==0xe000u &&
          layeredRegisters->voice[6].ip==0xe000u);
    OriginalSf2RomBankVoiceOwnership layeredLedger{};
    CHECK(layeredLedger.recordExecutedNoteOn(*layered,layeredState));
    CHECK(layeredLedger.trackedPhysicalVoices()==3u);
    CHECK(!layeredLedger.recordExecutedNoteOn(*layered,layeredState));
    const auto downLayers=layeredLedger.controller64(
        layeredChip,layeredState,2u,127u);
    CHECK(downLayers && downLayers->sustain_enabled);
    const auto offLayers=layeredLedger.noteOff(
        layeredChip,layeredState,2u,60u);
    CHECK(offLayers && offLayers->deferred_by_sustain &&
          offLayers->typed_port_writes==0u &&
          layeredLedger.trackedPhysicalVoices()==3u);
    const auto upLayers=layeredLedger.controller64(
        layeredChip,layeredState,2u,0u);
    CHECK(upLayers && upLayers->released_deferred_voices==3u &&
          upLayers->typed_port_writes==12u &&
          layeredLedger.trackedPhysicalVoices()==0u);
    CHECK(layeredState[layerRow0]==0xfeu &&
          layeredState[layerRow3]==0xfeu &&
          layeredState[layerRow6]==0xfeu);
    // One-zone path stays governed by the previous single-note API.
    CHECK(!candidateOriginalSf2StartNextIdleLayeredRomBankNoteOnLiveChip(
        layeredChip,layeredState,sf2,*on,midi,0u,524288u,
        verifiedSyntheticResources,conditions,layeredContext,true));
    const auto wrongLive=candidateOriginalSf2StartSingleRomBankLayerOnLiveChip(
        liveChip,sbk,*on,midi,0u,524288u,verifiedSyntheticResources,
        conditions,verifiedSyntheticVxd,true);
    CHECK(!wrongLive); // original SF1 and ROM SF2 are distinct paths
    const auto transposedIp=candidateOriginalSf2InitialIpFromDescriptor(
        *started,61u,0,0);
    CHECK(transposedIp && *transposedIp==0xe155u);
    CHECK(!prepareOriginalCreativeRomNote(*on,midi,sf2,0,24));
    // Real original SF2 x86 builder returns exactly six 148-byte
    // descriptors from a synthetic seven-matching-zone source bank.
    // Our separate raw inventory still preserves all seven matches.
    const auto sevenZones=fixture(true,7u);
    const auto inventory7=resolveCreativeSbkLayers(sevenZones,0,7,60,100);
    CHECK(inventory7 && inventory7->count==7u);
    const auto planned7=prepareOriginalCreativeRomNote(
        *on,midi,sevenZones,0,524288u);
    CHECK(planned7 && planned7->count==kOriginalCreativeMaxNoteStartDescriptors);
    for(std::size_t i=0;i<planned7->count;++i){
        CHECK(planned7->layers[i].metadata.instrument_bag==i);
        CHECK(planned7->layers[i].address.start_word==5u);
    }
    // Driver selection cap applies regardless of the inventory ceiling.
    const auto sixteenZones=fixture(true,16u);
    const auto inventory16=resolveCreativeSbkLayers(sixteenZones,0,7,60,100);
    CHECK(inventory16 && inventory16->count==16u);
    const auto planned16=prepareOriginalCreativeRomNote(
        *on,midi,sixteenZones,0,524288u);
    CHECK(planned16 && planned16->count==6u);
    // Original builder stops after six. A dormant seventh matched zone
    // is inventory data only and must not invalidate a legal six-voice
    // NoteOn, even when its sampleID references the invalid EOS marker.
    const auto sevenWithBadTail=fixture(true,7u,true);
    const auto inventoryBadTail=resolveCreativeSbkLayers(
        sevenWithBadTail,0,7,60,100);
    CHECK(inventoryBadTail && inventoryBadTail->count==7u);
    CHECK(inventoryBadTail->layers[6].rom_sample_selector==1u);
    const auto sixPlay=prepareOriginalCreativeRomNote(
        *on,midi,sevenWithBadTail,0,524288u);
    CHECK(sixPlay && sixPlay->count==6u);
    for(std::size_t i=0;i<sixPlay->count;++i)
        CHECK(sixPlay->layers[i].address.sample_id==0u);
    // But a bad sixth voice, which WOULD be started, must fail closed.
    const auto sixWithBadLast=fixture(true,6u,true);
    CHECK(!prepareOriginalCreativeRomNote(
        *on,midi,sixWithBadLast,0,524288u));
    // Zero matched layers still means a valid bank, not a fake voice.
    const auto noSeven=prepareOriginalCreativeRomNote(
        *on,midi,sevenZones,128,524288u);
    CHECK(noSeven && noSeven->count==0u);
    // Valid RAM-mono SF2 must never enter the ROM-only playback plan.
    auto ramSf2=sf2;
    const std::string_view sampleName="ROM-MONO";
    const auto sampleIt=std::search(ramSf2.begin(),ramSf2.end(),
        sampleName.begin(),sampleName.end());
    CHECK(sampleIt!=ramSf2.end());
    const auto base=static_cast<std::size_t>(sampleIt-ramSf2.begin());
    CHECK(base+45u<ramSf2.size());
    CHECK(ramSf2[base+45u]==128u);
    ramSf2[base+45u]=0u; // type ROM mono 0x8001 -> RAM mono 0x0001
    CHECK(!prepareOriginalCreativeRomNote(*on,midi,ramSf2,0,524288));
    std::cout<<"P002_SYNTHETIC_16CH_MIDI_PROGRAM_TO_ROM_ADDRESS_PASS"
             <<" private_bytes=NONE"
             <<" original_live_idle_three_distinct_bank_noteons=TRUE"
             <<" source_vxd_state_committed_after_each_actual_chip_note=TRUE"
             <<" no_unproven_busy_voice_steal=TRUE"
             <<" bank_derived_three_voices_original_NoteOff_CC64=TRUE"
             <<" untracked_original_CC64_deferred_voice_rejected=TRUE"
             <<" original_three_layer_ONE_MIDI_note_183_port_writes=TRUE"
             <<" original_three_layer_single_NoteOff_CC64_releases_all=TRUE"
             <<" wrong_last_layer_fails_before_chip_writes=TRUE\n";
}
