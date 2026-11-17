#include "OriginalCreativeSf2ResourceBuilder.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace awe64::original_vxd;
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"SF2_READER_FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using B=std::vector<std::uint8_t>;
static void u16(B& b,std::uint16_t v){b.push_back(static_cast<std::uint8_t>(v));b.push_back(static_cast<std::uint8_t>(v>>8));}
static void u32(B& b,std::uint32_t v){u16(b,static_cast<std::uint16_t>(v));u16(b,static_cast<std::uint16_t>(v>>16));}
static void chunk(B& b,const char (&name)[5],B const& d){for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(name[i]));u32(b,static_cast<std::uint32_t>(d.size()));b.insert(b.end(),d.begin(),d.end());if(d.size()&1u)b.push_back(0u);}
static void list(B& b,const char (&name)[5],B const& d){B x;for(unsigned i=0;i<4;++i)x.push_back(static_cast<std::uint8_t>(name[i]));x.insert(x.end(),d.begin(),d.end());chunk(b,"LIST",x);}
static void phdr(B& b,std::uint16_t program,std::uint16_t bank,std::uint16_t bag){b.insert(b.end(),20u,0u);u16(b,program);u16(b,bank);u16(b,bag);b.insert(b.end(),12u,0u);}
static void inst(B& b,std::uint16_t bag){b.insert(b.end(),20u,0u);u16(b,bag);}
static void bag(B& b,std::uint16_t gen,std::uint16_t mod=0u){u16(b,gen);u16(b,mod);}
static void gen(B& b,std::uint16_t op,std::uint16_t amount){u16(b,op);u16(b,amount);}
static void sample(B& b,std::uint32_t start,std::uint32_t end,std::uint32_t ls,std::uint32_t le,std::uint16_t link,std::uint16_t kind){b.insert(b.end(),20u,0u);u32(b,start);u32(b,end);u32(b,ls);u32(b,le);u32(b,44100u);b.push_back(60u);b.push_back(0u);u16(b,link);u16(b,kind);}
struct Fixture {B file{};std::size_t pbag_offset=0u;std::size_t igen_offset=0u;std::size_t smpl_offset=0u;};
static Fixture make_fixture(std::uint16_t sampleTypeA=2u,std::uint16_t sampleTypeB=4u,bool modulated=false,bool padded_loop_end=false){
    B info,ifil;u16(ifil,2);u16(ifil,1);chunk(info,"ifil",ifil);
    B pd,ph,pb,pg,in,ib,ig,im,sh,sampleData(2400u,0u);
    phdr(ph,7u,1u,0u);phdr(ph,0u,0u,2u);
    bag(pb,0u);bag(pb,1u);bag(pb,3u);
    gen(pg,8u,100u);gen(pg,43u,static_cast<std::uint16_t>((65u<<8u)|60u));gen(pg,41u,0u);
    inst(in,0u);inst(in,3u);
    bag(ib,0u,0u);bag(ib,1u,0u);bag(ib,4u,modulated?1u:0u);bag(ib,7u,modulated?1u:0u);
    gen(ig,8u,88u);
    gen(ig,43u,static_cast<std::uint16_t>((64u<<8u)|60u));
    gen(ig,44u,static_cast<std::uint16_t>((127u<<8u)|1u));
    gen(ig,53u,0u);
    gen(ig,43u,static_cast<std::uint16_t>((64u<<8u)|60u));
    gen(ig,44u,static_cast<std::uint16_t>((127u<<8u)|1u));
    gen(ig,53u,1u);
    sample(sh,100u,300u,150u,padded_loop_end?302u:250u,1u,sampleTypeA);
    sample(sh,400u,600u,450u,550u,0u,sampleTypeB);
    sample(sh,1200u,1200u,1200u,1200u,0u,1u);
    chunk(pd,"phdr",ph);chunk(pd,"pbag",pb);chunk(pd,"pgen",pg);
    if(modulated) {
        // One 10-byte instrument modulator for the first zone, followed
        // by an ordinary terminal imod record.
        u16(im,2u);u16(im,8u);u16(im,50u);u16(im,0u);u16(im,0u);
    }
    im.insert(im.end(),10u,0u);
    chunk(pd,"inst",in);chunk(pd,"ibag",ib);chunk(pd,"igen",ig);
    chunk(pd,"imod",im);
    chunk(pd,"shdr",sh);
    B sd;chunk(sd,"smpl",sampleData);
    B body;for(char c:"sfbk"){if(c)body.push_back(static_cast<std::uint8_t>(c));}
    list(body,"INFO",info);list(body,"sdta",sd);list(body,"pdta",pd);
    Fixture fixture;for(char c:"RIFF"){if(c)fixture.file.push_back(static_cast<std::uint8_t>(c));}
    u32(fixture.file,static_cast<std::uint32_t>(body.size()));
    fixture.file.insert(fixture.file.end(),body.begin(),body.end());return fixture;
}

struct PortLog {
    struct Event {std::uint16_t port;std::uint32_t value;std::uint8_t selector;char width;};
    std::array<Event,1024> events{};
    std::size_t count=0;
    std::uint8_t selected=0;
    std::array<std::uint16_t,32> voice_release{};
    std::array<std::uint16_t,32> ifatn{};
    bool put(std::uint16_t port,std::uint32_t value,char width) noexcept {
        if(count==events.size())return false;
        events[count++]={port,value,selected,width};return true;
    }
    static bool b(void* p,std::uint16_t port,std::uint8_t value) noexcept {
        auto& x=*static_cast<PortLog*>(p);
        if(port!=0xe22u)return false;x.selected=value;
        return x.put(port,value,'b');
    }
    static bool w(void* p,std::uint16_t port,std::uint16_t value) noexcept {
        auto& x=*static_cast<PortLog*>(p);
        if(port!=0xa20u && port!=0xe20u && port!=0xa22u)return false;
        if(port==0xa20u && (x.selected&0xe0u)==0xa0u)
            x.voice_release[x.selected&31u]=value;
        if(port==0xe20u && (x.selected&0xe0u)==0x20u)
            x.ifatn[x.selected&31u]=value;
        return x.put(port,value,'w');
    }
    static bool d(void* p,std::uint16_t port,std::uint32_t value) noexcept {
        auto& x=*static_cast<PortLog*>(p);
        if(port!=0x620u && port!=0xa20u)return false;
        return x.put(port,value,'d');
    }
    static bool rw(void* p,std::uint16_t port,std::uint16_t& value) noexcept {
        auto& x=*static_cast<PortLog*>(p);
        if(port==0xe20u && (x.selected&0xe0u)==0x20u) {
            value=x.ifatn[x.selected&31u];return x.put(port,value,'r');
        }
        if(port!=0xa20u)return false;
        value=x.voice_release[x.selected&31u];return x.put(port,value,'r');
    }
    static bool rd(void* p,std::uint16_t port,std::uint32_t& value) noexcept {
        auto& x=*static_cast<PortLog*>(p);
        if(port!=0x620u && port!=0xa20u)return false;
        // Empty virtual EMU8000 voice during original allocation.
        value=0u;return x.put(port,value,'R');
    }
    PortIo io() noexcept{return {this,b,w,rw,rd,d};}
};
int main(){
    const auto file=make_fixture();
    const auto read=select_sf2_raw_layers(file.file,1u,7u,60u,100u);
    REQUIRE(read.status==Sf2ReadStatus::Ok && read.count==2u);
    OriginalSf2SourceTables table{};
    std::array<OriginalSf2SourceChannelState,2> inputs{};
    REQUIRE(prepare_original_sf2_zones(read,table,inputs).status==Sf2BuildStatus::Unverified);
    table.original_generator_templates_verified=true;
    table.original_pitch_tables_verified=true;
    table.original_sample_ram_placement_verified=true;
    table.original_stereo_descriptor_conversion_verified=true;
    table.ram_base_word=0x200000u;
    table.instrument_template[54u]=1;
    table.instrument_template[56u]=100;
    table.instrument_template[58u]=-1;
    table.instrument_template[34u]=100;
    table.instrument_template[8u]=200;
    table.instrument_template[9u]=7;
    table.instrument_template[19u]=20;
    table.instrument_template[36u]=63;
    table.instrument_template[37u]=5;
    for(auto& c:inputs){
        c.original_note_context_verified=true;
        c.original_attenuation_verified=true;
        c.original_encoded_attenuation=20u;
        c.original_voice_start.original_descriptor_verified=true;
        c.original_voice_start.original_device_state_verified=true;
        c.original_voice_start.original_sample_placement_verified=true;
        c.original_voice_start.original_ram_sample=true;
        c.original_voice_start.original_pitch_target=0x8000u;
        c.original_voice_pitch_base_verified=true;
        c.original_voice_pitch_base=0xe000u;
        c.original_program_identity_verified=true;
        c.original_program_bank_slot=0u;
        c.original_program_preset_row=0u;
    }
    REQUIRE(prepare_original_sf2_zones(read,table,inputs).status==Sf2BuildStatus::Ok);
    auto prepared=prepare_original_sf2_zones(read,table,inputs);
    REQUIRE(prepared.count==2u);
    const auto& a=prepared.zones[0];const auto& b=prepared.zones[1];
    REQUIRE(a.voice.descriptor[0x8cu]==2u && b.voice.descriptor[0x8cu]==4u);
    REQUIRE(a.voice.note_start_descriptor[0x8cu]==1u && b.voice.note_start_descriptor[0x8cu]==1u);
    REQUIRE(a.voice.descriptor[0x8eu]==1u && b.voice.descriptor[0x8eu]==0u);
    auto r32=[](Descriptor const& d,unsigned at) noexcept {
        return static_cast<std::uint32_t>(d[at])|
          (static_cast<std::uint32_t>(d[at+1u])<<8u)|
          (static_cast<std::uint32_t>(d[at+2u])<<16u)|
          (static_cast<std::uint32_t>(d[at+3u])<<24u);
    };
    REQUIRE(r32(a.voice.descriptor,0x76u)==0x200064u);
    REQUIRE(r32(b.voice.descriptor,0x76u)==0x200190u);
    REQUIRE(r32(a.voice.descriptor,0x86u)==0xe00000u);
    // Genuine owner SF2 GUITAR15/LEDHEAV/others use loopEnd=end+2.
    // Test the complete descriptor->chip plan, not just raw parsing:
    // original looped LCOD+0x5B90/+0x5BA0 writes loopEnd-1.
    const auto guard_source=make_fixture(2u,4u,false,true);
    const auto guard_layers=select_sf2_raw_layers(
        guard_source.file,1u,7u,60u,100u);
    REQUIRE(guard_layers.status==Sf2ReadStatus::Ok &&
            guard_layers.layers[0].sample.loop_end==302u);
    const auto guard_prepared=prepare_original_sf2_zones(
        guard_layers,table,inputs);
    REQUIRE(guard_prepared.status==Sf2BuildStatus::Ok &&
            guard_prepared.count==2u);
    REQUIRE(r32(guard_prepared.zones[0].voice.descriptor,0x82u)==0x20012eu);
    OriginalVoiceProgram guard_program{};
    REQUIRE(compile_original_sf2_main(guard_prepared.zones[0].voice,
        30u,60u,guard_prepared.zones[0].original_state,guard_program));
    REQUIRE(guard_program.data[18]==0x20012du);
    REQUIRE(a.voice.note_start_descriptor[38u]==20u);
    auto noPitch=table;noPitch.original_pitch_tables_verified=false;
    REQUIRE(prepare_original_sf2_zones(read,noPitch,inputs).status==Sf2BuildStatus::Unverified);
    auto noPlacement=table;noPlacement.original_sample_ram_placement_verified=false;
    REQUIRE(prepare_original_sf2_zones(read,noPlacement,inputs).status==Sf2BuildStatus::Unsupported);
    auto noStereo=table;noStereo.original_stereo_descriptor_conversion_verified=false;
    REQUIRE(prepare_original_sf2_zones(read,noStereo,inputs).status==Sf2BuildStatus::Unsupported);
    const auto with_mod=make_fixture(2u,4u,true);
    const auto mod_layers=select_sf2_raw_layers(with_mod.file,1u,7u,60u,100u);
    REQUIRE(mod_layers.status==Sf2ReadStatus::Ok);
    REQUIRE(prepare_original_sf2_zones(mod_layers,table,inputs).status==Sf2BuildStatus::Unsupported);
    auto wrong=inputs;wrong[1].original_program_preset_row=5u;
    REQUIRE(prepare_original_sf2_zones(read,table,wrong).status==Sf2BuildStatus::Unverified);
    // Real MIDI -> prebuilt ORIGINAL-encoded sample zones -> live port
    // operations. This synthetic fixture establishes integration only.
    VerifiedSf2Bank bank{std::span<const VerifiedSf2Zone>(prepared.zones.data(),prepared.count)};
    const auto src=original_verified_sf2_bank_source(bank);
    MidiEngine midi{};PortLog io{};
    REQUIRE(dispatch_midi(midi,0xb0u,0u,1u,io.io(),src)==MidiResult::Applied);
    REQUIRE(dispatch_midi(midi,0xc0u,7u,0u,io.io(),src)==MidiResult::Applied);
    const auto note=dispatch_midi(midi,0x90u,60u,100u,io.io(),src);
    REQUIRE(note==MidiResult::Applied);
    // Original NoteOn volume is now generated from raw generator48,
    // velocity and source CC tables, not a pre-attested value per note.
    // Synthetic source tables; this is NOT original-driver PCM parity.
    {
        std::array<std::uint8_t,128> velocity{},cc7{},cc11{};
        velocity.fill(10u);velocity[127]=0u;cc7[100]=64u;
        OriginalNoteVolumeResources volume{};
        volume.original_velocity_table_verified=true;
        volume.velocity_table=velocity;
        volume.controllers={true,cc7.data(),cc11.data(),10u,true,true,true};
        auto dynamicTable=table;dynamicTable.instrument_template[48u]=24;
        auto dynamicInputs=inputs;
        for(auto& c:dynamicInputs) {
            c.original_attenuation_verified=false;
            c.original_note_volume_source=&volume;
        }
        auto dynamic=prepare_original_sf2_zones(read,dynamicTable,dynamicInputs);
        REQUIRE(dynamic.status==Sf2BuildStatus::Ok && dynamic.count==2u);
        for(const auto& z:std::span(dynamic.zones.data(),dynamic.count)) {
            REQUIRE(z.voice.note_start_descriptor[38u]==147u);
            REQUIRE(z.voice.volume_terms_verified);
            REQUIRE(z.voice.original_amplitude_word==24);
            REQUIRE(z.voice.original_velocity_volume_level==10u);
        }
        // Derive pitch metadata/targets from the original descriptor and
        // live channel, with NO externally supplied per-note pitch base.
        auto nativeInputs=dynamicInputs;
        for(auto& c:nativeInputs) {
            c.original_voice_pitch_base_verified=false;
            c.original_voice_start.original_derive_pitch_target_from_channel=true;
            c.original_voice_start.original_derive_volume_target_from_attenuation=true;
        }
        auto native=prepare_original_sf2_zones(read,dynamicTable,nativeInputs);
        REQUIRE(native.status==Sf2BuildStatus::Ok && native.count==2u);
        REQUIRE(native.zones[0].voice.original_pitch_base==0xe000u);
        VerifiedSf2Bank nativeBank{std::span<const VerifiedSf2Zone>(native.zones.data(),native.count)};
        auto nativeSource=original_verified_sf2_bank_source(nativeBank);
        MidiChannel nativeChannel{};nativeChannel.bank_msb=1u;nativeChannel.program=7u;
        nativeChannel.original_bend_delta=4096;
        std::array<SourceVoice,max_source_note_layers> nativeVoices{};
        std::uint8_t nativeCount=0u;
        REQUIRE(nativeSource.resolve(nativeSource.opaque,nativeChannel,60u,100u,nativeVoices,nativeCount));
        REQUIRE(nativeCount==2u);
        OriginalVoiceProgram nativeProgram{};
        REQUIRE(nativeSource.compile(nativeSource.opaque,nativeVoices[0],0u,60u,nativeProgram));
        REQUIRE(nativeProgram.data[7]==0xf000u && nativeProgram.data[26]==0xa0000000u);
        nativeChannel.original_fine_tune_verified=true;
        nativeChannel.original_fine_tune_cents=100;
        REQUIRE(nativeSource.resolve(nativeSource.opaque,nativeChannel,60u,100u,nativeVoices,nativeCount));
        REQUIRE(nativeVoices[0].original_pitch_base==static_cast<std::uint16_t>((0xe00000u+87400u)>>8u));
        REQUIRE(nativeSource.compile(nativeSource.opaque,nativeVoices[0],0u,60u,nativeProgram));
        ++nativeVoices[0].original_pitch_base;
        REQUIRE(!nativeSource.compile(nativeSource.opaque,nativeVoices[0],0u,60u,nativeProgram));
        // Every playable velocity can use the same small source context.
        for(unsigned v=1u;v<128u;++v) {
            const auto selected=select_sf2_raw_layers(file.file,1u,7u,60u,
                static_cast<std::uint8_t>(v));
            const auto built=prepare_original_sf2_zones(selected,dynamicTable,dynamicInputs);
            REQUIRE(built.status==Sf2BuildStatus::Ok && built.count==2u);
            REQUIRE(built.zones[0].voice.original_velocity_volume_level==velocity[v]);
        }
        VerifiedSf2Bank dynamicBank{
            std::span<const VerifiedSf2Zone>(dynamic.zones.data(),dynamic.count)};
        const auto dynamicSource=original_verified_sf2_bank_source(dynamicBank);
        MidiEngine live{};PortLog bus{};
        live.volume_source=volume.controllers;
        REQUIRE(dispatch_midi(live,0xb0u,0u,1u,bus.io(),dynamicSource)==MidiResult::Applied);
        REQUIRE(dispatch_midi(live,0xc0u,7u,0u,bus.io(),dynamicSource)==MidiResult::Applied);
        REQUIRE(dispatch_midi(live,0x90u,60u,100u,bus.io(),dynamicSource)==MidiResult::Applied);
        REQUIRE(dispatch_midi(live,0xb0u,7u,127u,bus.io(),dynamicSource)==MidiResult::Applied);
        unsigned active=0u;
        for(unsigned i=0u;i<32u;++i) {
            if(live.device.voices[i].key_marker!=60u)continue;
            ++active;
            REQUIRE((bus.ifatn[i]&255u)==39u);
            REQUIRE((bus.ifatn[i]>>8u)==dynamic.zones[0].voice.note_start_descriptor[16u]);
        }
        REQUIRE(active==2u);
        REQUIRE(dispatch_midi(live,0xb0u,11u,10u,bus.io(),dynamicSource)==MidiResult::Applied);
        for(unsigned i=0u;i<32u;++i)
            if(live.device.voices[i].key_marker==60u)
                REQUIRE((bus.ifatn[i]&255u)==255u);
        // Missing source and contradictory supplied values fail closed.
        volume.original_velocity_table_verified=false;
        REQUIRE(prepare_original_sf2_zones(read,dynamicTable,dynamicInputs).status!=Sf2BuildStatus::Ok);
        volume.original_velocity_table_verified=true;
        dynamicInputs[1].original_attenuation_verified=true;
        dynamicInputs[1].original_encoded_attenuation=20u;
        REQUIRE(prepare_original_sf2_zones(read,dynamicTable,dynamicInputs).status!=Sf2BuildStatus::Ok);
    }
    const auto& started=prepared.zones[0].voice.note_start_descriptor;
    const auto expected_trigger=static_cast<std::uint32_t>(started[74u])*256u+started[72u];
    unsigned ntrigger=0u;
    for(std::size_t i=0;i<io.count;++i){
        const auto& e=io.events[i];
        if(e.port==0xa20u && e.width=='w' &&
           (e.selector&0xe0u)==0xa0u && e.value==expected_trigger)++ntrigger;
    }
    REQUIRE(ntrigger==2u);
    // No manually guessed RAM base: use the newly ported exact original
    // Creative filter/bank-allocator return to bind the descriptor tables.
    OriginalCreativeRamBankCursor cursor{true,false,0x200000u};
    const auto physical_sink=[](void*,std::uint32_t,
        std::span<const std::int16_t>) noexcept -> bool {return true;};
    const auto uploaded=upload_original_creative_sf2_ram_bank(
        file.file,cursor,{nullptr,physical_sink});
    REQUIRE(uploaded.status==OriginalSf2UploadStatus::Ok &&
        uploaded.placement.first_sample_word==0x20000eu);
    auto bound_table=table;
    REQUIRE(bind_original_creative_uploaded_ram_bank(bound_table,uploaded));
    REQUIRE(bound_table.original_sample_ram_placement_verified &&
            bound_table.ram_base_word==0x20000eu);
    const auto bound=prepare_original_sf2_zones(read,bound_table,inputs);
    REQUIRE(bound.status==Sf2BuildStatus::Ok);
    REQUIRE(r32(bound.zones[0].voice.descriptor,0x76u)==0x200072u);
    REQUIRE(r32(bound.zones[1].voice.descriptor,0x76u)==0x20019eu);
    // A partial upload must cancel the authenticated base, never reuse it.
    OriginalCreativeRamUploadResult failed=uploaded;
    failed.status=OriginalSf2UploadStatus::UploadFailed;
    REQUIRE(!bind_original_creative_uploaded_ram_bank(bound_table,failed));
    REQUIRE(!bound_table.original_sample_ram_placement_verified &&
            bound_table.ram_base_word==0u);
    REQUIRE(prepare_original_sf2_zones(read,bound_table,inputs).status==
            Sf2BuildStatus::Unsupported);
    std::puts("CREATIVE_SF2_RESOURCE_BUILDER_END_TO_END_PASS");
}
