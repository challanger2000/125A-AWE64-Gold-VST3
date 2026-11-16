// P005: actual source-port RIFF -> resource builder -> MIDI chip port.
// Strictly synthetic SF2 bytes / source tables: NOT Win95 driver parity.
#include "P005OriginalCreativeSf2NoteStage.hpp"
#include "P005OriginalCreativeHostAdapter.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>
using namespace awe64::original_vxd;
using awe64::vst3::P005OriginalCreativeSf2NoteStage;
using awe64::vst3::P005OriginalCreativeHostAdapter;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"P005_NOTE_STAGE_FAIL:%d %s\n",__LINE__,#x);return 1;}}while(false)
using B=std::vector<std::uint8_t>;
struct Chip {
    bool good=true;std::uint32_t writes=0u,reads=0u;
    [[nodiscard]] bool ready() const noexcept{return good;}
    void writePort(std::uint16_t,std::uint16_t) noexcept{++writes;}
    [[nodiscard]] std::uint16_t readPort(std::uint16_t) noexcept {++reads;return 0u;}
};
static void u16(B& b,std::uint16_t v){b.push_back(static_cast<std::uint8_t>(v));b.push_back(static_cast<std::uint8_t>(v>>8u));}
static void u32(B& b,std::uint32_t v){u16(b,static_cast<std::uint16_t>(v));u16(b,static_cast<std::uint16_t>(v>>16u));}
static void chunk(B& b,const char* id,B const& data){for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(id[i]));u32(b,static_cast<std::uint32_t>(data.size()));b.insert(b.end(),data.begin(),data.end());if((data.size()&1u)!=0u)b.push_back(0u);}
static void list(B& b,const char* id,B const& data){B x;for(unsigned i=0;i<4;++i)x.push_back(static_cast<std::uint8_t>(id[i]));x.insert(x.end(),data.begin(),data.end());chunk(b,"LIST",x);}
static void phdr(B& b,std::uint16_t program,std::uint16_t bank,std::uint16_t bag){b.insert(b.end(),20u,0u);u16(b,program);u16(b,bank);u16(b,bag);b.insert(b.end(),12u,0u);}
static void inst(B& b,std::uint16_t bag){b.insert(b.end(),20u,0u);u16(b,bag);}
static void bag(B& b,std::uint16_t gen){u16(b,gen);u16(b,0u);}
static void gen(B& b,std::uint16_t op,std::uint16_t amount){u16(b,op);u16(b,amount);}
static void sample(B& b,std::uint16_t link,std::uint16_t kind){b.insert(b.end(),20u,0u);u32(b,100u);u32(b,300u);u32(b,150u);u32(b,250u);u32(b,44100u);b.push_back(60u);b.push_back(0u);u16(b,link);u16(b,kind);}
static B sf2(){
    B info,ifil;u16(ifil,2u);u16(ifil,1u);chunk(info,"ifil",ifil);
    B pd,ph,pb,pg,in,ib,ig,sh,sd;
    phdr(ph,7u,1u,0u);phdr(ph,0u,0u,1u);bag(pb,0u);bag(pb,1u);
    gen(pg,41u,0u);inst(in,0u);inst(in,1u);
    bag(ib,0u);bag(ib,1u);gen(ig,53u,0u);
    sample(sh,0u,1u);sample(sh,0u,1u); // second terminal EOS
    chunk(pd,"phdr",ph);chunk(pd,"pbag",pb);chunk(pd,"pgen",pg);
    chunk(pd,"inst",in);chunk(pd,"ibag",ib);chunk(pd,"igen",ig);
    chunk(pd,"shdr",sh);
    chunk(sd,"smpl",B(2400u,0u));
    B root;for(char c:"sfbk")if(c)root.push_back(static_cast<std::uint8_t>(c));
    list(root,"INFO",info);list(root,"sdta",sd);list(root,"pdta",pd);
    B raw;for(char c:"RIFF")if(c)raw.push_back(static_cast<std::uint8_t>(c));
    u32(raw,static_cast<std::uint32_t>(root.size()));raw.insert(raw.end(),root.begin(),root.end());
    return raw;
}
int main(){
    const auto file=sf2();
    const auto layers=select_sf2_raw_layers(file,1u,7u,60u,100u);
    CHECK(layers.status==Sf2ReadStatus::Ok && layers.count==1u);
    OriginalSf2SourceTables original{};
    original.original_generator_templates_verified=true;
    original.original_pitch_tables_verified=true;
    original.original_sample_ram_placement_verified=true;
    original.ram_base_word=0x200010u;
    original.instrument_template[54u]=1;
    original.instrument_template[56u]=100;
    original.instrument_template[58u]=-1;
    original.instrument_template[34u]=100;
    original.instrument_template[8u]=200;
    original.instrument_template[9u]=7;
    original.instrument_template[19u]=20;
    original.instrument_template[36u]=63;
    original.instrument_template[37u]=5;
    std::array<OriginalSf2SourceChannelState,1> states{};
    auto& st=states[0];
    st.original_note_context_verified=true;
    st.original_attenuation_verified=true;
    st.original_encoded_attenuation=20u;
    st.original_voice_start.original_descriptor_verified=true;
    st.original_voice_start.original_device_state_verified=true;
    st.original_voice_start.original_sample_placement_verified=true;
    st.original_voice_start.original_ram_sample=true;
    st.original_voice_start.original_pitch_target=0x8000u;
    st.original_voice_pitch_base_verified=true;
    st.original_voice_pitch_base=0xe000u;
    st.original_program_identity_verified=true;
    st.original_program_bank_slot=5u;
    st.original_program_preset_row=0u;
    P005OriginalCreativeSf2NoteStage stage{};
    CHECK(stage.prepareNonRealtime(file,1u,7u,60u,100u,original,states,false)==Sf2BuildStatus::Unverified);
    CHECK(!stage.ready());
    auto noPitch=original;noPitch.original_pitch_tables_verified=false;
    CHECK(stage.prepareNonRealtime(file,1u,7u,60u,100u,noPitch,states,true)==Sf2BuildStatus::Unverified);
    CHECK(stage.prepareNonRealtime(file,1u,7u,60u,100u,original,states,true)==Sf2BuildStatus::Ok);
    CHECK(stage.ready() && stage.count()==1u && stage.key()==60u && stage.velocity()==100u);
    const auto bank=stage.view();
    CHECK(bank.zones.size()==1u && bank.zones[0].voice.source_verified);
    CHECK(bank.zones[0].original_preset_identity_verified);
    CHECK(bank.zones[0].original_bank_slot==5u);
    CHECK(bank.zones[0].voice.note_start_descriptor_verified);
    const auto absent=stage.prepareNonRealtime(file,2u,7u,60u,100u,original,states,true);
    CHECK(absent!=Sf2BuildStatus::Ok && !stage.ready());
    CHECK(stage.prepareNonRealtime(file,1u,7u,60u,100u,original,states,true)==Sf2BuildStatus::Ok);
    Chip chip{};
    P005OriginalCreativeHostAdapter<Chip> host{};
    // Stage::view() is a PRVALUE. The original MIDI callback opaque
    // MUST point to an adapter-owned bank snapshot, NOT to a temporary.
    CHECK(host.activate(chip,stage.view(),true));
    CHECK(host.controller(0u,0u,1u)==MidiResult::Applied);
    CHECK(host.program(0u,7u)==MidiResult::Applied);
    const auto before=chip.writes;
    CHECK(host.noteOn(0u,60u,100u)==MidiResult::Applied);
    CHECK(chip.writes>before);
    CHECK(host.sharedRuntime()->scratch.device==&host.sharedRuntime()->midi.device);
    CHECK(host.noteOff(0u,60u)==MidiResult::Applied);
    host.resetNonRealtime();
    CHECK(!host.active());
    // Original Creative device directory is 288 slots, NOT 77 PHDR
    // entries padded to 128. Bind the authenticated PHDR-derived source
    // to the physical VXD slot+row identity, then use the ORIGINAL
    // ProgramChange resolver to reach that exact zone on NoteOn.
    std::array<OriginalProgramBankSlot,original_program_bank_slots> slots{};
    std::array<OriginalPresetTableRow,2> slot5{{
        {1u,7u,1u},{0xffffu,0u,0u}}};
    slots[5].rows=slot5;
    OriginalProgramBankMap originalDirectory{true,slots};
    Chip chip2{};
    P005OriginalCreativeHostAdapter<Chip> sourceMatched{};
    auto malformedDirectory=originalDirectory;
    malformedDirectory.slots=std::span<const OriginalProgramBankSlot>(
        slots.data(),10u);
    CHECK(!sourceMatched.activate(
        chip2,stage.view(malformedDirectory),true));
    CHECK(!sourceMatched.active());
    slot5[0].original_bank_byte=2u;
    CHECK(!sourceMatched.activate(
        chip2,stage.view(originalDirectory),true));
    CHECK(!sourceMatched.active());
    slot5[0].original_bank_byte=1u;
    slots[5].rows=std::span<const OriginalPresetTableRow>(
        slot5.data(),1u); // missing original EOP-style VXD sentinel
    CHECK(!sourceMatched.activate(
        chip2,stage.view(originalDirectory),true));
    CHECK(!sourceMatched.active());
    slots[5].rows=slot5;
    CHECK(sourceMatched.activate(
        chip2,stage.view(originalDirectory),true));
    CHECK(sourceMatched.controller(0u,0u,1u)==MidiResult::Applied);
    CHECK(sourceMatched.program(0u,7u)==MidiResult::Applied);
    CHECK(sourceMatched.sharedRuntime()->midi.channel[0]
        .original_selected_bank_slot==5u);
    CHECK(sourceMatched.sharedRuntime()->midi.channel[0]
        .original_selected_preset_row==0u);
    CHECK(sourceMatched.noteOn(0u,60u,100u)==MidiResult::Applied);
    CHECK(sourceMatched.noteOff(0u,60u)==MidiResult::Applied);
    CHECK(sourceMatched.program(0u,9u)==MidiResult::MissingSource);
    sourceMatched.resetNonRealtime();
    CHECK(!sourceMatched.active());
    std::puts("P005_REAL_SOURCE_READER_RESOURCE_BUILDER_TO_VST3_MIDI_PASS synthetic_source_only=TRUE original_win95_parity=NOT_PROVEN");
    std::puts("P005_ORIGINAL_288_SLOT_BANK_ROW_IDENTITY_PASS ephemeral_view_lifetime_safe=TRUE 77_PHDR_PRESERVED=TRUE");
    return 0;
}
