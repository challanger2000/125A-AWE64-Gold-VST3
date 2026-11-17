// P005: actual source-port RIFF -> resource builder -> MIDI chip port.
// Strictly synthetic SF2 bytes / source tables: NOT Win95 driver parity.
#include "P005OriginalCreativeSf2NoteStage.hpp"
#include "P005OriginalCreativeBankCatalog.hpp"
#include "P005OriginalCreativeVxdDirectoryImporter.hpp"
#include "P005OriginalCreativeHostAdapter.hpp"
#include "P005OriginalVxdGuestCaptureFile.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>
using namespace awe64::original_vxd;
using awe64::vst3::P005OriginalCreativeSf2NoteStage;
using awe64::vst3::P005OriginalCreativeHostAdapter;
using awe64::vst3::P005OriginalVerifiedNoteInput;
using awe64::vst3::P005OriginalCreativeBankCatalog;
using awe64::vst3::P005OriginalGuestMemorySegment;
using awe64::vst3::P005OriginalVxdDirectoryImportResult;
using awe64::vst3::P005OriginalVxdDirectoryImporter;
using awe64::vst3::P005OriginalVxdGuestCaptureFile;
using awe64::vst3::P005VxdGuestCaptureFileResult;
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

    // Multi-note bank: stage C4 and D4 independently with the ORIGINAL
    // SF2 reader/compiler. The catalog must own both source descriptors
    // and all 288 physical VXD slot-row records before MIDI starts.
    P005OriginalCreativeBankCatalog catalog{};
    CHECK(!catalog.importAuthenticatedOriginalDirectory(slots,false));
    CHECK(catalog.importAuthenticatedOriginalDirectory(slots,true));
    CHECK(catalog.sourceSlotCount()==288u && catalog.zoneCount()==0u);
    CHECK(catalog.appendVerifiedNoteContext(stage));
    // Same original physical preset/row/key/velocity must NOT register
    // twice: the original MIDI resolver would trigger doubled voices.
    CHECK(!catalog.appendVerifiedNoteContext(stage));
    CHECK(catalog.zoneCount()==1u);
    CHECK(stage.prepareNonRealtime(file,1u,7u,62u,100u,original,states,true)==
          Sf2BuildStatus::Ok);
    CHECK(catalog.appendVerifiedNoteContext(stage));
    CHECK(!catalog.appendVerifiedNoteContext(stage));
    CHECK(catalog.zoneCount()==2u);
    // A full 288-slot directory alone is not proof that its note
    // contexts are sealed and immutable.
    Chip presealChip{};
    P005OriginalCreativeHostAdapter<Chip> presealHost{};
    CHECK(!presealHost.activateOwnedOriginalCatalogNonRealtime(
        presealChip,catalog,true));
    CHECK(!presealHost.sourceBound());
    CHECK(catalog.sealNonRealtime());
    CHECK(catalog.ready());
    CHECK(!catalog.sealNonRealtime());
    CHECK(!catalog.appendVerifiedNoteContext(stage));
    // Deliberately invalidate the CALLER'S original row array: the
    // sealed catalog retains its own authenticated source record copy.
    slot5[0].original_program_byte=17u;
    CHECK(catalog.view().original_program_map.slots[5].rows[0]
              .original_program_byte==7u);
    // Even the stage can now be reused/reset: its descriptor buffers
    // have already been copied into the stable catalog.
    stage.clearNonRealtime();
    CHECK(catalog.zoneCount()==2u && catalog.view().zones.size()==2u);
    Chip chip3{};
    P005OriginalCreativeHostAdapter<Chip> originalFullBank{};
    // A mere "source verified" assertion must NOT permit activation
    // when the caller has not yet completed its independent SHA gate.
    CHECK(!originalFullBank.activateOwnedOriginalCatalogNonRealtime(
        chip3,catalog,false));
    CHECK(!originalFullBank.sourceBound());
    CHECK(originalFullBank.activateOwnedOriginalCatalogNonRealtime(
        chip3,catalog,true));
    CHECK(originalFullBank.sourceBound());
    CHECK(catalog.runtimeLeased());
    // The owned 288-slot rows and zone descriptors are in use by
    // the original C++23 MIDI callbacks. A premature host reset must
    // FAIL without invalidating any borrowed span.
    CHECK(!catalog.resetNonRealtime());
    CHECK(catalog.ready() && catalog.zoneCount()==2u);
    Chip forbiddenChip{};
    P005OriginalCreativeHostAdapter<Chip> forbiddenOwner{};
    CHECK(!forbiddenOwner.activateOwnedOriginalCatalogNonRealtime(
        forbiddenChip,catalog,true));
    CHECK(!forbiddenOwner.sourceBound());
    CHECK(catalog.runtimeLeased() && originalFullBank.active());
    CHECK(originalFullBank.controller(0u,0u,1u)==MidiResult::Applied);
    CHECK(originalFullBank.program(0u,7u)==MidiResult::Applied);
    CHECK(originalFullBank.noteOn(0u,60u,100u)==MidiResult::Applied);
    CHECK(originalFullBank.noteOff(0u,60u)==MidiResult::Applied);
    CHECK(originalFullBank.noteOn(0u,62u,100u)==MidiResult::Applied);
    CHECK(originalFullBank.noteOff(0u,62u)==MidiResult::Applied);
    // Host event-bus supports 16 MIDI channels. The original driver
    // port must not silently reuse channel-0 program/CC state on 15.
    CHECK(originalFullBank.controller(15u,0u,1u)==MidiResult::Applied);
    CHECK(originalFullBank.program(15u,7u)==MidiResult::Applied);
    CHECK(originalFullBank.noteOn(15u,60u,100u)==MidiResult::Applied);
    CHECK(originalFullBank.noteOff(15u,60u)==MidiResult::Applied);
    CHECK(originalFullBank.sharedRuntime()->midi.channel[15]
        .original_selected_bank_slot==5u);
    // Real Creative LCOD+0x6D05: CC0 requests another bank, but
    // the selected physical VXD program row must remain unchanged
    // until the next C0 ProgramChange (not a PHDR hot swap).
    CHECK(originalFullBank.controller(0u,0u,2u)==MidiResult::Applied);
    CHECK(originalFullBank.sharedRuntime()->midi.channel[0].bank_msb==2u);
    CHECK(originalFullBank.sharedRuntime()->midi.channel[0]
        .original_selected_bank_slot==5u);
    // This synthetic source has no real bank2 program-7 record, and
    // the source resolver is not allowed to fabricate one.
    CHECK(originalFullBank.program(0u,7u)==MidiResult::MissingSource);
    CHECK(originalFullBank.controller(0u,0u,1u)==MidiResult::Applied);
    CHECK(originalFullBank.program(0u,7u)==MidiResult::Applied);
    CHECK(originalFullBank.noteOn(0u,60u,100u)==MidiResult::Applied);
    CHECK(originalFullBank.noteOff(0u,60u)==MidiResult::Applied);
    CHECK(originalFullBank.sharedRuntime()->scratch.device==
          &originalFullBank.sharedRuntime()->midi.device);
    CHECK(chip3.writes>0u);
    originalFullBank.resetNonRealtime();
    CHECK(!catalog.runtimeLeased());
    // Non-RT reactivation against the SAME sealed source must be
    // possible after detaching the previous original MIDI runtime.
    Chip reboundChip{};
    P005OriginalCreativeHostAdapter<Chip> reboundOwner{};
    CHECK(reboundOwner.activateOwnedOriginalCatalogNonRealtime(
        reboundChip,catalog,true));
    CHECK(catalog.runtimeLeased());
    CHECK(!catalog.resetNonRealtime());
    reboundOwner.resetNonRealtime();
    CHECK(!catalog.runtimeLeased());
    {
        // Unusual host teardown: the adapter destructor must also
        // detach its runtime and release the catalog lease; the host
        // may omit the usual explicit deactivate() callback.
        Chip teardownChip{};
        P005OriginalCreativeHostAdapter<Chip> teardownOwner{};
        CHECK(teardownOwner.activateOwnedOriginalCatalogNonRealtime(
            teardownChip,catalog,true));
        CHECK(catalog.runtimeLeased());
        CHECK(!catalog.resetNonRealtime());
    }
    CHECK(!catalog.runtimeLeased());
    CHECK(catalog.resetNonRealtime());
    CHECK(!catalog.ready() && catalog.zoneCount()==0u);
    CHECK(catalog.sourceSlotCount()==0u);

    // END-TO-END ORIGINAL INTERFACE: decode a 288-slot guest pointer
    // IMAGE + exact 26-byte source records, then stage a verified SF2
    // source descriptor and send it through the ORIGINAL C0 / MIDI /
    // EMU8000 chip-port bridge. All guest bytes here are SYNTHETIC;
    // genuine private Win95 guest pages must be SHA-attested separately.
    constexpr std::uint32_t guestPointer=0x12001000u;
    std::array<std::uint8_t,0x1592u> deviceImage{};
    auto const at=std::size_t{0x6cau}+5u*4u;
    for(unsigned i=0u;i<4u;++i)
        deviceImage[at+i]=static_cast<std::uint8_t>(
            guestPointer>>(i*8u));
    std::array<std::uint8_t,52u> guestRows{};
    guestRows[0]=1u; // real row marker !=0, !=0xFFFF
    guestRows[2]=7u; // original Program Change C0=7
    guestRows[3]=1u; // original Creative CC0 bank=1
    guestRows[26]=0xffu;guestRows[27]=0xffu;
    // Two original capture pages cover the same exact virtual byte
    // interval; the first 26-byte VXD row crosses their boundary.
    // Deliberately pass the source pages in reverse order.
    const std::array<P005OriginalGuestMemorySegment,2> guestSegments{{
        {guestPointer+15u,
         std::span<const std::uint8_t>(guestRows).subspan(15u)},
        {guestPointer,
         std::span<const std::uint8_t>(guestRows).first(15u)}
    }};
    P005OriginalCreativeBankCatalog guestCatalog{};
    using ImportResult=P005OriginalVxdDirectoryImportResult;
    CHECK(P005OriginalVxdDirectoryImporter::importNonRealtime(
        guestCatalog,deviceImage,guestSegments,false)==ImportResult::Unverified);
    CHECK(P005OriginalVxdDirectoryImporter::importNonRealtime(
        guestCatalog,deviceImage,guestSegments,true)==ImportResult::Ok);
    CHECK(guestCatalog.sourceRowCountNonRealtime(5u)==2u);
    guestRows[2]=99u; // original externally owned buffer can disappear
    CHECK(guestCatalog.originalSourceRowNonRealtime(5u,0u)
          ->original_program_byte==7u);
    CHECK(stage.prepareNonRealtime(
        file,1u,7u,60u,100u,original,states,true)==Sf2BuildStatus::Ok);
    CHECK(guestCatalog.appendVerifiedNoteContext(stage));
    CHECK(guestCatalog.sealNonRealtime());
    stage.clearNonRealtime();
    Chip guestChip{};
    P005OriginalCreativeHostAdapter<Chip> guestHost{};
    // The original 32 physical rows must reach the *same* live
    // MIDI device used by the chip allocator, not a second snapshot.
    // Synthetic guest bytes: no original Win95 attestation is claimed.
    deviceImage[0x158eu]='1';deviceImage[0x158fu]='M';
    deviceImage[0x1590u]='G';deviceImage[0x1591u]='M';
    deviceImage[0x06u]=32u;
    deviceImage[0x18u]=32u; // exact synthetic mutable mode count
    for(std::size_t voice=0u;voice<32u;++voice) {
        const auto off=0x42u+voice*0x20u;
        deviceImage[off]=0xfeu;deviceImage[off+1u]=0x0fu;
    }
    constexpr auto highRow=0x42u+31u*0x20u;
    deviceImage[highRow+0x0eu]=0xcdu;
    deviceImage[highRow+0x0fu]=0x4bu;
    deviceImage[highRow+0x12u]=21u;
    deviceImage[highRow+0x18u]=42u;
    deviceImage[highRow+0x19u]=43u;
    CHECK(!guestHost.activateOwnedOriginalCatalogWithDeviceNonRealtime(
        guestChip,guestCatalog,deviceImage,false));
    CHECK(!guestHost.sourceBound() && !guestCatalog.runtimeLeased());
    auto forgedImage=deviceImage;
    forgedImage[0x158eu]=0u;
    CHECK(!guestHost.activateOwnedOriginalCatalogWithDeviceNonRealtime(
        guestChip,guestCatalog,forgedImage,true));
    CHECK(!guestHost.sourceBound() && !guestCatalog.runtimeLeased());
    CHECK(guestHost.activateOwnedOriginalCatalogWithDeviceNonRealtime(
        guestChip,guestCatalog,deviceImage,true));
    CHECK(guestHost.sourceBound() && guestCatalog.runtimeLeased());
    CHECK(guestHost.sharedRuntime()->scratch.device==
          &guestHost.sharedRuntime()->midi.device);
    const auto& live=guestHost.sharedRuntime()->midi.device;
    CHECK(live.physical_capacity==32u && live.mode_count==32u);
    CHECK(live.voices[31].key_marker==0x0ffeu);
    CHECK(live.voices[31].original_pitch_base==0x4bcdu);
    CHECK(live.voices[31].original_pitch_base_verified);
    CHECK(live.voices[31].pan_state_verified &&
          live.voices[31].original_pan_base==21u &&
          live.voices[31].pan_target==42u &&
          live.voices[31].pan_current==43u);
    CHECK(!live.voices[31].volume_terms_verified);
    // No C0 state was present in this source device image. Its empty
    // bank-zero channel row must NOT masquerade as a valid preset.
    CHECK(!guestHost.sharedRuntime()->midi.channel[0]
          .original_program_selection_valid);
    CHECK(guestHost.noteOn(0u,60u,100u)==MidiResult::MissingSource);
    CHECK(guestHost.controller(0u,0u,1u)==MidiResult::Applied);
    CHECK(guestHost.program(0u,7u)==MidiResult::Applied);
    CHECK(guestHost.sharedRuntime()->midi.channel[0]
        .original_selected_bank_slot==5u);
    CHECK(guestHost.sharedRuntime()->midi.channel[0]
        .original_selected_preset_row==0u);
    CHECK(guestHost.noteOn(0u,60u,100u)==MidiResult::Applied);
    CHECK(guestHost.noteOff(0u,60u)==MidiResult::Applied);
    CHECK(guestChip.writes>0u);
    CHECK(!guestCatalog.resetNonRealtime()); // active source lease
    guestHost.resetNonRealtime();
    CHECK(guestCatalog.resetNonRealtime());
    // End-to-end owner CAPTURE ingestion rather than a caller-invented
    // 288-slot array: 125AVXD1 -> owned source catalog -> SF2 note
    // contexts -> actual original MIDI runtime with 32 guest voice rows.
    // All contents are synthetic, so only the explicit research opt-in
    // may supply the two independent attestation gates here.
    guestRows[2u]=7u;
    // Exact original per-channel C0 selection fields, kept in the
    // captured guest DEVICE image: row0 / program7 / slot5 / bank1.
    // The native C++23 host must play the selected program on NoteOn
    // without requiring an invented host ProgramChange after restore.
    constexpr std::size_t channel0=0x442u;
    deviceImage[channel0+0x02u]=7u;
    deviceImage[channel0+0x18u]=5u;
    deviceImage[channel0+0x1bu]=1u;
    std::vector<std::uint8_t> binary{
        '1','2','5','A','V','X','D','1'};
    const auto append32=[&binary](std::uint32_t v) {
        for(unsigned i=0u;i<4u;++i)
            binary.push_back(static_cast<std::uint8_t>(v>>(8u*i)));
    };
    append32(1u);append32(0xc30b09f0u);append32(0x3000u);
    append32(0x1592u);append32(288u);append32(1u);append32(1u);
    binary.insert(binary.end(),deviceImage.begin(),deviceImage.end());
    append32(guestPointer);
    append32(static_cast<std::uint32_t>(guestRows.size()));
    binary.insert(binary.end(),guestRows.begin(),guestRows.end());
    P005OriginalVxdGuestCaptureFile parsedGuest{};
    CHECK(parsedGuest.loadNonRealtime(binary)==
          P005VxdGuestCaptureFileResult::Ok);
    binary.clear(); // parser must own its source bytes
    CHECK(stage.prepareNonRealtime(
        file,1u,7u,60u,100u,original,states,true)==Sf2BuildStatus::Ok);
    const std::array<const P005OriginalCreativeSf2NoteStage*,1> noteSources{{&stage}};
    P005OriginalCreativeBankCatalog capturedCatalog{};
    P005OriginalCreativeHostAdapter<Chip> capturedHost{};
    Chip capturedChip{};
    CHECK(!capturedHost.activateOriginalGuestCaptureBundleNonRealtime(
        capturedChip,capturedCatalog,parsedGuest,noteSources,false,true));
    CHECK(capturedCatalog.sourceSlotCount()==0u);
    CHECK(!capturedHost.activateOriginalGuestCaptureBundleNonRealtime(
        capturedChip,capturedCatalog,parsedGuest,noteSources,true,false));
    CHECK(capturedCatalog.sourceSlotCount()==0u);
    const std::array<const P005OriginalCreativeSf2NoteStage*,2> duplicateNotes{{
        &stage,&stage
    }};
    CHECK(!capturedHost.activateOriginalGuestCaptureBundleNonRealtime(
        capturedChip,capturedCatalog,parsedGuest,duplicateNotes,true,true));
    CHECK(capturedCatalog.sourceSlotCount()==0u &&
          capturedCatalog.zoneCount()==0u &&
          !capturedCatalog.runtimeLeased());
    CHECK(capturedHost.activateOriginalGuestCaptureBundleNonRealtime(
        capturedChip,capturedCatalog,parsedGuest,noteSources,true,true));
    CHECK(capturedCatalog.ready() && capturedCatalog.runtimeLeased());
    CHECK(capturedHost.sharedRuntime()->midi.device.physical_capacity==32u);
    CHECK(capturedHost.sharedRuntime()->midi.device.voices[31]
          .original_pitch_base==0x4bcdu);
    const auto& restoredProgram=capturedHost.sharedRuntime()->midi.channel[0];
    CHECK(restoredProgram.original_program_selection_valid);
    CHECK(restoredProgram.original_selected_bank_slot==5u);
    CHECK(restoredProgram.original_selected_preset_row==0u);
    CHECK(restoredProgram.original_selected_program==7u);
    CHECK(restoredProgram.original_selected_bank==1u);
    CHECK(capturedHost.noteOn(0u,60u,100u)==MidiResult::Applied);
    CHECK(capturedHost.noteOff(0u,60u)==MidiResult::Applied);
    CHECK(capturedHost.controller(0u,0u,1u)==MidiResult::Applied);
    CHECK(capturedHost.program(0u,7u)==MidiResult::Applied);
    CHECK(capturedHost.noteOn(0u,60u,100u)==MidiResult::Applied);
    CHECK(capturedHost.noteOff(0u,60u)==MidiResult::Applied);
    CHECK(capturedChip.writes>0u);
    capturedHost.resetNonRealtime();
    CHECK(capturedCatalog.resetNonRealtime());

    // Actual interface for the six-region original compact bank:
    // 26-byte VXD row +04 -> preset opcode41 instrument ->
    // instrument opcode53 sampleID. Synthetic 125AVXD1 + 125AVXR1
    // fixtures use the SAME guest VA/CR3, root and channel0 state.
    constexpr std::uint32_t nestedRoot=0xc11b8900u;
    constexpr std::array<std::uint32_t,7u> ptrs{{
        nestedRoot+76u,nestedRoot+128u,nestedRoot+136u,
        nestedRoot+139u,nestedRoot+143u,nestedRoot+151u,
        nestedRoot+154u
    }};
    auto nestedDevice=deviceImage;
    const auto pointerByte=0x6cau+5u*4u;
    for(unsigned i=0u;i<4u;++i)
        nestedDevice[pointerByte+i]=static_cast<std::uint8_t>(
            nestedRoot>>(8u*i));
    B nestedPrimary{'1','2','5','A','V','X','D','1'};
    const auto primary32=[&nestedPrimary](std::uint32_t value) {
        for(unsigned i=0u;i<4u;++i)
            nestedPrimary.push_back(static_cast<std::uint8_t>(value>>(i*8u)));
    };
    primary32(1u);primary32(0xc30b09f0u);primary32(0x3000u);
    primary32(0x1592u);primary32(288u);primary32(1u);primary32(1u);
    nestedPrimary.insert(
        nestedPrimary.end(),nestedDevice.begin(),nestedDevice.end());
    primary32(nestedRoot);primary32(76u+2u*26u);
    const auto bankStart=nestedPrimary.size();
    nestedPrimary.resize(bankStart+128u,0u);
    const auto set16=[&nestedPrimary](std::size_t at,std::uint16_t value) {
        nestedPrimary[at]=static_cast<std::uint8_t>(value);
        nestedPrimary[at+1u]=static_cast<std::uint8_t>(value>>8u);
    };
    const auto set32=[&set16](std::size_t at,std::uint32_t value) {
        set16(at,static_cast<std::uint16_t>(value));
        set16(at+2u,static_cast<std::uint16_t>(value>>16u));
    };
    for(std::size_t i=0u;i<ptrs.size();++i)
        set32(bankStart+i*4u,ptrs[i]);
    set16(bankStart+76u,1u);
    nestedPrimary[bankStart+76u+2u]=7u;
    nestedPrimary[bankStart+76u+3u]=1u;
    set16(bankStart+76u+4u,0u); // compact preset group 0
    set16(bankStart+76u+26u,0xffffu);
    set16(bankStart+76u+26u+4u,1u); // terminal group index
    B nestedSidecar{'1','2','5','A','V','X','R','1'};
    const auto side32=[&nestedSidecar](std::uint32_t value) {
        for(unsigned i=0u;i<4u;++i)
            nestedSidecar.push_back(static_cast<std::uint8_t>(value>>(i*8u)));
    };
    side32(1u);side32(0xc30b09f0u);side32(0x3000u);
    side32(nestedRoot);
    for(auto ptr:ptrs)side32(ptr);
    side32(6u);
    const std::array<B,6u> sections{{
        B{0u,0u,0u,0u,1u,0u,0u,0u},
        B{41u,0u,0u},
        B{0u,0u,1u,0u},
        B{0u,0u,0u,0u,1u,0u,0u,0u},
        B{53u,0u,0u}, // actual SF2-selected sampleID=0
        B(4096u,0u)
    }};
    for(std::size_t i=0u;i<sections.size();++i) {
        side32(ptrs[i+1u]);
        side32(static_cast<std::uint32_t>(sections[i].size()));
        side32(i==5u?0u:1u);
        nestedSidecar.insert(
            nestedSidecar.end(),sections[i].begin(),sections[i].end());
    }
    P005OriginalVxdGuestCaptureFile nestedGuest{};
    P005OriginalNestedGuestCaptureFile nestedSource{};
    CHECK(nestedGuest.loadNonRealtime(nestedPrimary)==
          P005VxdGuestCaptureFileResult::Ok);
    CHECK(nestedSource.loadNonRealtime(nestedPrimary,nestedSidecar)==
          P005NestedGuestCaptureResult::Ok);
    P005OriginalCreativeBankCatalog nestedCatalog{};
    P005OriginalCreativeHostAdapter<Chip> nestedHost{};
    Chip nestedChip{};
    CHECK(!nestedHost.activateOriginalGuestCaptureWithNestedNonRealtime(
        nestedChip,nestedCatalog,nestedGuest,nestedSource,
        noteSources,false,true));
    CHECK(nestedCatalog.sourceSlotCount()==0u);
    auto wrongSampleSidecar=nestedSidecar;
    // Region 5 (op53 triple) starts after the four preceding regions.
    // Use deterministic offset from 56-byte header and region lengths.
    const std::size_t sampleDataAt=56u+
        (12u+8u)+(12u+3u)+(12u+4u)+(12u+8u)+12u;
    CHECK(sampleDataAt+3u<wrongSampleSidecar.size());
    wrongSampleSidecar[sampleDataAt+1u]=1u;
    P005OriginalNestedGuestCaptureFile mismatchedSample{};
    CHECK(mismatchedSample.loadNonRealtime(
        nestedPrimary,wrongSampleSidecar)==P005NestedGuestCaptureResult::Ok);
    CHECK(!nestedHost.activateOriginalGuestCaptureWithNestedNonRealtime(
        nestedChip,nestedCatalog,nestedGuest,mismatchedSample,
        noteSources,true,true));
    CHECK(nestedCatalog.sourceSlotCount()==0u &&
          !nestedCatalog.runtimeLeased() && !nestedHost.sourceBound());
    CHECK(nestedHost.activateOriginalGuestCaptureWithNestedNonRealtime(
        nestedChip,nestedCatalog,nestedGuest,nestedSource,
        noteSources,true,true));
    CHECK(nestedCatalog.ready() && nestedHost.sourceBound());
    CHECK(nestedHost.sharedRuntime()->midi.channel[0]
          .original_program_selection_valid);
    CHECK(nestedHost.noteOn(0u,60u,100u)==MidiResult::Applied);
    CHECK(nestedHost.noteOff(0u,60u)==MidiResult::Applied);
    CHECK(nestedChip.writes>0u);
    nestedHost.resetNonRealtime();
    CHECK(nestedCatalog.resetNonRealtime());
    // Two distinct musical keys prepared from the same SOURCE SF2,
    // original guest bank, and individually attested source-state rows.
    // The adapter must compile/store both without a persistent array
    // of per-note stage objects or any audio callback allocations.
    std::array<P005OriginalVerifiedNoteInput,2u> contexts{};
    contexts[0].bank=1u;contexts[0].program=7u;
    contexts[0].key=60u;contexts[0].velocity=100u;
    contexts[0].layer_count=1u;contexts[0].layers[0]=states[0];
    contexts[1]=contexts[0];
    contexts[1].key=62u;
    P005OriginalCreativeBankCatalog batchCatalog{};
    P005OriginalCreativeHostAdapter<Chip> batchHost{};
    Chip batchChip{};
    CHECK(!batchHost.activateOriginalSf2ContextBatchNonRealtime(
        batchChip,batchCatalog,nestedGuest,nestedSource,
        file,original,contexts,false,true));
    CHECK(batchCatalog.sourceSlotCount()==0u);
    CHECK(batchHost.activateOriginalSf2ContextBatchNonRealtime(
        batchChip,batchCatalog,nestedGuest,nestedSource,
        file,original,contexts,true,true));
    CHECK(batchCatalog.ready() && batchCatalog.zoneCount()==2u);
    CHECK(batchHost.sharedRuntime()->midi.channel[0]
          .original_program_selection_valid);
    CHECK(batchHost.noteOn(0u,60u,100u)==MidiResult::Applied);
    CHECK(batchHost.noteOff(0u,60u)==MidiResult::Applied);
    CHECK(batchHost.noteOn(0u,62u,100u)==MidiResult::Applied);
    CHECK(batchHost.noteOff(0u,62u)==MidiResult::Applied);
    CHECK(batchChip.writes>0u);
    batchHost.resetNonRealtime();
    CHECK(batchCatalog.resetNonRealtime());
    std::puts("P005_SOURCE_BATCH_TWO_KEYS_SHARED_MIDI_PASS synthetic_only=TRUE");
    std::puts("P005_NESTED_ORIGINAL_SAMPLE_REF_TO_MIDI_PASS synthetic_only=TRUE");
    stage.clearNonRealtime();
    std::puts("P005_OWNER_CAPTURE_TO_SHARED_CREATIVE_MIDI_SOURCE_PASS synthetic_only=TRUE");
    std::puts("P005_GUEST_POINTER_TO_ORIGINAL_C0_NOTEON_NOTE_OFF_PASS synthetic_only=TRUE");

    std::puts("P005_SOURCE_288_SLOT_2_NOTE_CATALOG_PASS borrowed_rows_OWNED stable_midi_runtime=TRUE");

    std::puts("P005_REAL_SOURCE_READER_RESOURCE_BUILDER_TO_VST3_MIDI_PASS synthetic_source_only=TRUE original_win95_parity=NOT_PROVEN");
    std::puts("P005_ORIGINAL_288_SLOT_BANK_ROW_IDENTITY_PASS ephemeral_view_lifetime_safe=TRUE 77_PHDR_PRESERVED=TRUE");
    return 0;
}
