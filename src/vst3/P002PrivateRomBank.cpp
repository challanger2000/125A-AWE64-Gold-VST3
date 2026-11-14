#include "P002PrivateRomBank.h"
#include "CreativeMidiRomPlan.h"
#include "CreativeSf2RamSamplePcm.h"
#include "P002PrivateOriginalRamCapture.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <utility>
namespace awe64::vst3 {
namespace {
using namespace awe64::creative;
// Keep the pack binary fixed and unambiguous (no native padding).
//   8 magic "125ABK01"; 32 original VXD SHA256; program u8,
//   channel u8, bank u16 LE, ROM word count u32 LE;
//   original 0x1592 VXD device state bytes;
//   original preset/instrument 59+59 signed LE WORDs;
//   original pitch base 128 LE DWORDs + 128 signed slope BYTES;
//   ORIGINAL LCOD 15-negative / 16-positive volume WORDs.
// Only a locally source-hash-verified generator may create this pack.
// This C++ reader authenticates the header/structure; it cannot on its
// own prove that an externally authored payload is the real Win95 state.
constexpr std::array<std::uint8_t,8> kMagic{
    '1','2','5','A','B','K','0','1'};
constexpr std::array<std::uint8_t,32> kVxdSha{
    0xd3,0x43,0xf2,0x42,0x8a,0xb3,0xe5,0x93,
    0xc9,0xe4,0x2a,0x66,0x8c,0xf5,0x21,0xb7,
    0x80,0x1e,0x12,0x39,0x79,0x2e,0xaf,0x7f,
    0x2b,0x11,0xb5,0xf1,0x79,0xbc,0x7c,0x9a};
constexpr std::size_t kPackLength=
    8u+32u+8u+kOriginalVxdMinDeviceImage+59u*2u*2u+
    128u*4u+128u+15u*2u+16u*2u;
constexpr std::size_t kMaxPrivateRomBankBytes=8u*1024u*1024u;
std::uint16_t u16(std::span<const std::uint8_t> b,
                  std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(
        b[offset] | (static_cast<std::uint16_t>(b[offset+1u])<<8u));
}
std::uint32_t u32(std::span<const std::uint8_t> b,
                  std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(u16(b,offset))|
           (static_cast<std::uint32_t>(u16(b,offset+2u))<<16u);
}
}
void P002PrivateRomBank::clear() noexcept {
    ready_=false;
    bank_bytes_.clear();
    state_.fill(0u);
    resources_={};
    negative_.fill(0u);
    positive_.fill(0u);
    ownership_={};
    midi_={};
    held_keys_.fill(false);
    bank_number_=0u;channel_=0u;program_=0u;
    ram_mode_=false;
    source_ram_base_=0u;
    source_ram_pool_words_=0u;
}
bool P002PrivateRomBank::load(
    const std::filesystem::path& source_pack,
    const std::filesystem::path& soundfont,
    std::string& error,
    const bool original_ram_source_already_sha_authenticated) {
    clear();
    std::ifstream pack(source_pack,std::ios::binary|std::ios::ate);
    if(!pack || pack.tellg()!=static_cast<std::streamoff>(kPackLength)) {
        error="P002 source resource pack missing or exact length invalid";
        return false;
    }
    pack.seekg(0);
    std::array<std::uint8_t,kPackLength> blob{};
    if(!pack.read(reinterpret_cast<char*>(blob.data()),blob.size())) {
        error="P002 source pack truncated";return false;
    }
    const std::span<const std::uint8_t> bytes(blob);
    if(!std::equal(kMagic.begin(),kMagic.end(),bytes.begin()) ||
       !std::equal(kVxdSha.begin(),kVxdSha.end(),bytes.begin()+8u)) {
        error="P002 original VXD source identity or pack schema mismatch";
        return false;
    }
    auto offset=8u+32u;
    program_=bytes[offset++];
    channel_=bytes[offset++];
    bank_number_=u16(bytes,offset);offset+=2u;
    const auto romWords=u32(bytes,offset);offset+=4u;
    if(program_>127u || channel_>=16u || romWords!=524288u ||
       bank_number_>128u) {
        error="P002 unsupported original bank/channel/ROM size";
        clear();return false;
    }
    std::copy_n(bytes.begin()+offset,state_.size(),state_.begin());
    offset+=state_.size();
    if(u32(state_,kOriginalVxdRomIdOffset)!=0x4d474d31u ||
       u16(state_,0x12u)!=0u ||
       u16(state_,0x06u)!=30u ||
       u32(state_,0x18u)<3u) {
        error="P002 private original dry VXD voice state unsupported";
        clear();return false;
    }
    // The source-proven 30-slot allocator cannot be initialized from
    // an unknown already-playing voice without its associated original
    // SF2 descriptors. Accept an all-idle guest capture only.
    for(std::size_t voice=0;voice<kOriginalCreativeMusicVoices;++voice) {
        const auto at=kOriginalVxdVoiceRowStart+
            voice*kOriginalVxdVoiceRowStride;
        if(u16(state_,at)!=0x0ffeu) {
            error="P002 original VXD state not fully idle";
            clear();return false;
        }
    }
    for(auto& word:resources_.preset_template) {
        word=static_cast<std::int16_t>(u16(bytes,offset));offset+=2u;
    }
    for(auto& word:resources_.instrument_template) {
        word=static_cast<std::int16_t>(u16(bytes,offset));offset+=2u;
    }
    for(auto& value:resources_.pitch_tables.base) {
        value=u32(bytes,offset);offset+=4u;
    }
    for(auto& value:resources_.pitch_tables.slope)
        value=static_cast<std::int8_t>(bytes[offset++]);
    for(auto& value:negative_) {
        value=u16(bytes,offset);offset+=2u;
    }
    for(auto& value:positive_) {
        value=u16(bytes,offset);offset+=2u;
    }
    if(offset!=blob.size()) {
        error="P002 source resource pack layout invalid";
        clear();return false;
    }
    if(original_ram_source_already_sha_authenticated) {
        // The 125ABK01 header merely ASSERTS a VXD SHA; without this
        // independent 820-byte hash any forged pitch/volume/template
        // could masquerade as the real Windows-95 Creative algorithm.
        // The real source is the independently hash-verified 77,370-
        // byte owner's SBAWE.VXD; extract its original LE-page-mapped
        // LCOD/PDAT resources on the owner side. Never embed the tables.
        constexpr std::size_t startOfOriginalTables=
            8u+32u+8u+kOriginalVxdMinDeviceImage+59u*2u;
        constexpr std::size_t originalTableBytes=
            59u*2u+128u*4u+128u+15u*2u+16u*2u;
        static_assert(startOfOriginalTables+originalTableBytes==kPackLength);
        if(!std::all_of(resources_.preset_template.begin(),
                        resources_.preset_template.end(),
                        [](std::int16_t v) noexcept {return v==0;}) ||
           !isExactOriginalCreativeVxdResources(
               bytes.subspan(startOfOriginalTables,originalTableBytes))) {
            error="P002 native original RAM bank rejects forged "
                  "Creative 1996 VXD pitch/volume/source templates";
            clear();return false;
        }
    }
    std::ifstream source(soundfont,std::ios::binary|std::ios::ate);
    if(!source || source.tellg()<static_cast<std::streamoff>(12u) ||
       source.tellg()>static_cast<std::streamoff>(kMaxPrivateRomBankBytes)) {
        error="P002 SF2 bank missing or exceeds research limit";
        clear();return false;
    }
    const auto length=static_cast<std::size_t>(source.tellg());
    source.seekg(0);
    bank_bytes_.resize(length);
    if(!source.read(reinterpret_cast<char*>(bank_bytes_.data()),length) ||
       !std::equal(bank_bytes_.begin(),bank_bytes_.begin()+4u,"RIFF") ||
       !std::equal(bank_bytes_.begin()+8u,bank_bytes_.begin()+12u,"sfbk")) {
        error="P002 not a complete RIFF sfbk SoundFont";
        clear();return false;
    }
    const std::uint32_t riffLength=u32(bank_bytes_,4u);
    if(riffLength!=length-8u) {
        error="P002 RIFF length incorrect";
        clear();return false;
    }
    // Separate two authentic sources: the existing original-ROM
    // source bank and the owner Win95 original-driver-uploaded RAM bank.
    // Without the SHA256-pinned RAM preload this path MUST reject all
    // RAM samples. A mixed ROM/RAM bank is never silently accepted.
    const auto sampleInventory=
        candidateOriginalSf2InventoryAllSampleTypes(bank_bytes_);
    if(!sampleInventory || sampleInventory->sample_headers==0u) {
        error="P002 malformed or unsupported standard SF2 sample inventory";
        clear();return false;
    }
    if(original_ram_source_already_sha_authenticated) {
        // This is the SAME bank already admitted by the separate
        // original RAM trace+PCM SHA verifier on the activation thread.
        // Generalized Creative RAM placement has NOT been established.
        if(!isExactOriginalCreativeRamOwnerSf2(bank_bytes_) ||
           sampleInventory->sample_headers!=83u ||
           sampleInventory->ram_mono!=83u ||
           sampleInventory->rom_mono!=0u ||
           sampleInventory->unsupported!=0u ||
           bank_number_!=0u || program_>127u) {
            error="P002 original Win95 RAM source bank/type not matched";
            clear();return false;
        }
        constexpr std::uint32_t originalPhysicalBase=0x200010u;
        constexpr std::size_t originalPoolWords=260921u;
        // Require a standard RAM-mono sample descriptor within the
        // exact original source pool; the preloader already verified
        // the whole SF2 SHA and final chip RAM content.
        const auto pool=candidateOriginalSf2ReadRamMonoSamplePcm(
            bank_bytes_,0u,0xe00000u);
        if(!pool || pool->total_pool_words!=originalPoolWords) {
            error="P002 source SoundFont RAM pool not authentic";
            clear();return false;
        }
        ram_mode_=true;
        source_ram_base_=originalPhysicalBase;
        source_ram_pool_words_=originalPoolWords;
    }else {
        if(sampleInventory->ram_mono>0u ||
           sampleInventory->unsupported>0u ||
           sampleInventory->rom_mono!=sampleInventory->sample_headers) {
            error="P002 ROM-only mode rejects RAM/linked SF2 samples";
            clear();return false;
        }
    }
    MidiMessage program{};
    program.kind=MidiKind::Program;
    program.channel=channel_;
    program.data1=program_;
    midi_.apply(program);
    ready_=true;error.clear();
    return true;
}
bool P002PrivateRomBank::noteOn(
    awe64::Chip& chip,const std::uint8_t channel,
    const std::uint8_t key,const std::uint8_t velocity) noexcept {
    if(!ready_ || !chip.ready() || channel!=channel_ ||
       key>127u || velocity==0u || velocity>127u ||
       held_keys_[key])return false;
    const auto& ch=midi_.channel(channel_);
    MidiMessage midi{};
    midi.kind=MidiKind::NoteOn;
    midi.channel=channel_;
    midi.data1=key;
    midi.data2=velocity;
    midi.raw_short=static_cast<std::uint32_t>(0x90u|channel_) |
        (static_cast<std::uint32_t>(key)<<8u)|
        (static_cast<std::uint32_t>(velocity)<<16u);
    OriginalCreativeVolumeTerms volume{};
    volume.cc7=ch.cc7_volume;volume.cc11=ch.cc11_expression;
    volume.device_state_additive_attenuation=u16(state_,0x10u);
    const OriginalSf2VoiceStartConditions start{
        bank_number_,key,velocity,ch.cc10_pan,0u,0u,0u,volume};
    OriginalSf2VerifiedVxdDeviceSnapshot context{};
    context.original_state_bytes=state_;
    context.independently_verified_original_vxd_snapshot=true;
    context.original_lcod_31word_table_verified=true;
    context.source_negative_15word_table=negative_;
    context.source_positive_16word_table=positive_;
    if(ram_mode_) {
        // Authentic physical source RAM already exists in the
        // unchanged EMU8000: execute the actual original VXD
        // 1..6-voice idle allocator and freshly MIDI-compiled 61
        // register OUTs per layer. Never replay a fixed C4/D4 clip.
        const auto started=
            candidateOriginalSf2StartNextIdleLayeredRamBankNoteOnLiveChip(
                chip,state_,bank_bytes_,midi,midi_,bank_number_,
                source_ram_base_,source_ram_pool_words_,
                resources_,start,context,true);
        if(!started)return false;
        if(!ownership_.recordExecutedNoteOn(*started,state_)) {
            ready_=false;
            return false;
        }
        held_keys_[key]=true;
        return true;
    }
    // Preserve original single-layer behavior. For 2..6 instrument
    // zones the NEW bounded source allocator selects one authentic
    // completely-idle physical voice per layer before issuing the full
    // original source-ordered multi-voice register plan.
    const auto plan=prepareOriginalCreativeRomNote(
        midi,midi_,bank_bytes_,bank_number_,524288u);
    if(!plan || plan->count==0u ||
       plan->count>kOriginalCreativeMaxNoteStartDescriptors)return false;
    bool ownershipRecorded=false;
    if(plan->count==1u) {
        const auto started=candidateOriginalSf2StartNextIdleRomBankNoteOnLiveChip(
            chip,state_,bank_bytes_,midi,midi_,bank_number_,524288u,
            resources_,start,context,true);
        if(!started)return false;
        ownershipRecorded=ownership_.recordExecutedNoteOn(*started,state_);
    }else {
        const auto layered=
            candidateOriginalSf2StartNextIdleLayeredRomBankNoteOnLiveChip(
                chip,state_,bank_bytes_,midi,midi_,bank_number_,524288u,
                resources_,start,context,true);
        if(!layered)return false;
        ownershipRecorded=ownership_.recordExecutedNoteOn(*layered,state_);
    }
    if(!ownershipRecorded) {
        // Original chip already changed; fail closed on ownership
        // mismatch instead of inventing NoteOff/voice stealing.
        ready_=false;return false;
    }
    held_keys_[key]=true;
    return true;
}
bool P002PrivateRomBank::programChange(
    const std::uint8_t program) noexcept {
    if(!ready_ || program>127u)return false;
    // This is the existing source-original logical ProgramChange
    // decoder/state stage. No invented chip writes: only subsequent
    // actual bank zones are selected using the new program number.
    MidiMessage change{};
    change.kind=MidiKind::Program;
    change.channel=channel_;
    change.data1=program;
    midi_.apply(change);
    program_=program;
    return true;
}
bool P002PrivateRomBank::controller(
    const std::uint8_t number,const std::uint8_t value) noexcept {
    if(!ready_ || value>127u ||
       (number!=7u && number!=10u && number!=11u))return false;
    MidiMessage change{};
    change.kind=MidiKind::Controller;
    change.channel=channel_;
    change.data1=number;
    change.data2=value;
    midi_.apply(change);
    return true;
}
bool P002PrivateRomBank::noteOff(
    awe64::Chip& chip,const std::uint8_t channel,
    const std::uint8_t key) noexcept {
    if(!ready_ || channel!=channel_ || key>127u ||
       !held_keys_[key])return false;
    const auto released=ownership_.noteOff(chip,state_,channel,key);
    if(!released)return false;
    held_keys_[key]=false;
    return true;
}
bool P002PrivateRomBank::sustain(
    awe64::Chip& chip,const std::uint8_t value) noexcept {
    if(!ready_ || value>127u)return false;
    const auto result=ownership_.controller64(chip,state_,channel_,value);
    return result.has_value();
}
} // namespace awe64::vst3
