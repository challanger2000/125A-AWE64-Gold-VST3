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
    std::string chipError;
    CHECK(liveChip.open(syntheticRom,chipError));
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
    }
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
    std::cout<<"P002_SYNTHETIC_16CH_MIDI_PROGRAM_TO_ROM_ADDRESS_PASS private_bytes=NONE\n";
}
