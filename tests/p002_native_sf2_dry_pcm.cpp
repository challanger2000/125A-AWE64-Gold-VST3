// P002 actual audible ROM SF2 NoteOn through the unchanged pinned EMU8000.
// Uses SYNTHETIC 1-MiB ROM (not copyrighted original ROM) and the
// independently source-derived original GM0 61-OUT sequence. Validates
// real PCM generation and audible sensitivity to ROM wave contents.
// Not independent Win95 Creative driver PCM equivalence or production VST3.
#include "CreativeSf2VxdStateSnapshot.h"
#include "CreativeSf2VxdSourceRelease.h"
#include "Chip.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>
namespace fs=std::filesystem;
using namespace awe64::creative;
#define CHECK(x) do {if(!(x)){std::cerr<<"P002_DRY_PCM_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;}} while(0)
namespace {
void gen(CreativeSf2VoiceDescriptorBytes& b,unsigned g,int n) noexcept {
    const auto w=static_cast<std::uint16_t>(n);
    b[2u*g]=static_cast<std::uint8_t>(w);
    b[2u*g+1u]=static_cast<std::uint8_t>(w>>8u);
}
void dword(CreativeSf2VoiceDescriptorBytes& b,unsigned i,std::uint32_t v) noexcept {
    for(unsigned j=0;j<4u;++j)b[i+j]=static_cast<std::uint8_t>(v>>(j*8u));
}
OriginalSf2VxdSnapshotVoice gm0() noexcept {
    OriginalSf2VxdSnapshotVoice source{};
    auto& v=source.assigned_voice;
    source.original_midi_channel=0;
    v.source_original_voice_verified=true;
    v.played_midi_key=60;
    v.physical_voice=0;
    v.lfo={true,0,0};
    auto& b=v.verified_after_start_record;
    b[0x8cu]=1u;b[0x88u]=0xe0u;b[0x8au]=60u;
    gen(b,58,77);gen(b,56,100);gen(b,52,4);
    gen(b,8,179);gen(b,19,58);
    gen(b,34,125);gen(b,35,120);
    gen(b,26,125);gen(b,27,127);gen(b,28,127);gen(b,29,0);
    gen(b,21,static_cast<std::int16_t>(0x8000u));
    gen(b,23,static_cast<std::int16_t>(0x8000u));
    gen(b,25,static_cast<std::int16_t>(0x8000u));
    gen(b,33,static_cast<std::int16_t>(0x8000u));
    gen(b,22,128);gen(b,54,1);gen(b,15,0);gen(b,16,27);
    gen(b,17,127);gen(b,18,129);gen(b,36,3);gen(b,37,0);
    dword(b,0x76,0xfeb9u);dword(b,0x7a,0x152feu);
    dword(b,0x7e,0x152bbu);dword(b,0x82,0x152fau);
    return source;
}
struct TempRom {
    fs::path path;
    explicit TempRom(int variant) {
        static unsigned serial=0;
        const auto temp=fs::temp_directory_path();
        path=temp/("125a-p002-synthetic-rom-"+std::to_string(++serial)+".raw");
        std::ofstream out(path,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("Unable to create synthetic ROM");
        for(std::uint32_t i=0;i<524288u;++i) {
            std::int16_t sample=0;
            if(i>=0xfeb9u && i<0x15310u) {
                const auto t=i-(0xfeb9u);
                sample=variant==0
                    ?static_cast<std::int16_t>((static_cast<int>(t%96u)-48)*280)
                    :static_cast<std::int16_t>((t%96u)<48u?11000:-11000);
            }
            const auto raw=static_cast<std::uint16_t>(sample);
            out.put(static_cast<char>(raw));
            out.put(static_cast<char>(raw>>8u));
        }
        if(!out)throw std::runtime_error("Synthetic ROM write failed");
    }
    ~TempRom(){std::error_code ec;fs::remove(path,ec);}
};
struct Pcm {
    std::uint64_t fnv=0xcbf29ce484222325ull;
    std::uint32_t nonzero=0;
    std::int32_t peak=0;
};
Pcm playback(const fs::path& rom,std::uint32_t& ip,std::uint32_t& ptrx){
    awe64::Chip chip;
    std::string reason;
    if(!chip.open(rom,reason))throw std::runtime_error("Chip open: "+reason);
    // Source-original global HWCF3 stereo unmute. Neither P002 DSP
    // nor recorded original NoteOn includes the chip's boot contract.
    chip.writePort(0xe22u,(1u<<5u)|31u);
    chip.writePort(0xa20u,0x0004u);
    std::array<std::uint8_t,kOriginalVxdMinDeviceImage> state{};
    const std::uint32_t romTag=0x4d474d31u;
    for(unsigned i=0;i<4u;++i)state[kOriginalVxdRomIdOffset+i]=
        static_cast<std::uint8_t>(romTag>>(8u*i));
    OriginalSf2VerifiedVxdDeviceSnapshot view{};
    view.original_state_bytes=state;
    view.independently_verified_original_vxd_snapshot=true;
    // The normal GM0 attack is <128. No original attenuation table
    // values are used in that source branch; private data remains absent.
    view.original_lcod_31word_table_verified=true;
    const std::array<OriginalSf2VxdSnapshotVoice,1> notes{{gm0()}};
    if(!candidateOriginalSf2WriteInitialIpToVxdState(state,true,notes))
        throw std::runtime_error("Source original voice IP write rejected");
    const auto plan=candidateOriginalSf2FullNoteOnFromVxdSnapshot(notes,view);
    if(!plan || plan->count!=61u)
        throw std::runtime_error("Genuine GM0 source 61-OUT plan rejected");
    if(!applyOriginalCreativeSf2FullNoteOnFromVxdSnapshot(chip,notes,view))
        throw std::runtime_error("Genuine GM0 source 61-OUT chip execution rejected");
    const auto* voice=chip.rawForContractTesting();
    ip=voice->voice[0].ip;
    ptrx=voice->voice[0].ptrx;
    Pcm result{};
    std::array<std::int32_t,512u*2u> samples{};
    for(int i=0;i<24;++i){
        chip.render(samples.data(),512u);
        for(const auto value:samples) {
            if(value!=0)++result.nonzero;
            result.peak=std::max(result.peak,
                value<0?-value:value);
            const auto bits=static_cast<std::uint32_t>(value);
            for(unsigned byte=0;byte<4u;++byte){
                result.fnv^=(bits>>(8u*byte))&0xffu;
                result.fnv*=0x100000001b3ull;
            }
        }
    }
    return result;
}
// P002 multi-note research oracle: two separate original-source NoteOns
// at different keys and physical EMU8000 voices, then original channel
// CC64 pedal-down, two deferred NoteOffs, and pedal-up. This is actual
// digital chip PCM, NOT a second software ADSR, a native VST3
// multi-note host, or an independent Creative Win95 PCM reference.
struct PolyEvidence {
    Pcm pre_release{},post_release{};
    std::uint16_t ip0=0,ip3=0;
    std::uint16_t dcysusv0=0,dcysusv3=0;
    std::size_t released=0;
    bool both_deferred=false,both_idle=false;
};
Pcm hashFrames(awe64::Chip& chip,const unsigned frames) {
    Pcm result{};
    std::array<std::int32_t,512u*2u> samples{};
    for(unsigned offset=0;offset<frames;offset+=512u) {
        const auto size=std::min(512u,frames-offset);
        chip.render(samples.data(),size);
        for(unsigned i=0;i<size*2u;++i) {
            const auto value=samples[i];
            if(value!=0)++result.nonzero;
            result.peak=std::max(result.peak,value<0?-value:value);
            const auto bits=static_cast<std::uint32_t>(value);
            for(unsigned byte=0;byte<4u;++byte) {
                result.fnv^=(bits>>(8u*byte))&0xffu;
                result.fnv*=0x100000001b3ull;
            }
        }
    }
    return result;
}
PolyEvidence polyPlayback(
    const fs::path& rom,const bool pedal_lift,
    const bool two_voices) {
    awe64::Chip chip;
    std::string reason;
    if(!chip.open(rom,reason))
        throw std::runtime_error("P002 poly ROM open: "+reason);
    chip.writePort(0xe22u,(1u<<5u)|31u);
    chip.writePort(0xa20u,0x0004u);
    std::array<std::uint8_t,kOriginalVxdMinDeviceImage> state{};
    constexpr std::uint32_t kRom=0x4d474d31u;
    for(unsigned i=0;i<4u;++i)
        state[kOriginalVxdRomIdOffset+i]=
            static_cast<std::uint8_t>(kRom>>(8u*i));
    OriginalSf2VerifiedVxdDeviceSnapshot source{};
    source.original_state_bytes=state;
    source.independently_verified_original_vxd_snapshot=true;
    source.original_lcod_31word_table_verified=true;
    auto piano0=gm0();
    // Proven original GM0 descriptor generator30/38 encoded NOTE START
    // values. No synthetic release byte is substituted at NoteOff.
    gen(piano0.assigned_voice.verified_after_start_record,30u,127);
    gen(piano0.assigned_voice.verified_after_start_record,38u,41);
    auto piano3=piano0;
    piano3.assigned_voice.physical_voice=3u;
    piano3.assigned_voice.played_midi_key=64u;
    // Actual SF2 per-note played key, same ROM sample root and original
    // descriptor; pitch is computed by the existing PCOD source path.
    gen(piano3.assigned_voice.verified_after_start_record,46u,64);
    const std::array<OriginalSf2VxdSnapshotVoice,2> assigned{{piano0,piano3}};
    const auto first=std::span<const OriginalSf2VxdSnapshotVoice>(
        assigned.data(),1u);
    const auto second=std::span<const OriginalSf2VxdSnapshotVoice>(
        assigned.data()+1u,1u);
    const auto all=std::span<const OriginalSf2VxdSnapshotVoice>(
        assigned.data(),two_voices?2u:1u);
    const auto start=[&](const auto selected) {
        if(!candidateOriginalSf2StoreNoteOnSourceReleaseState(
               state,true,selected) ||
           !candidateOriginalSf2WriteInitialIpToVxdState(
               state,true,selected) ||
           !applyOriginalCreativeSf2FullNoteOnFromVxdSnapshot(
               chip,selected,source))
            throw std::runtime_error("Original multi-NoteOn chip start failed");
    };
    start(first);
    (void)hashFrames(chip,1024u);
    if(two_voices)start(second);
    (void)hashFrames(chip,1024u);
    const auto* core=chip.rawForContractTesting();
    PolyEvidence out{};
    out.ip0=core->voice[0].ip;
    if(two_voices)out.ip3=core->voice[3].ip;
    const auto down=applyOriginalSf2MidiCc64FromVxdState(
        chip,state,all,0u,127u,true);
    if(!down || !down->sustain_enabled ||
       down->typed_port_writes!=0u)
        throw std::runtime_error("Original poly pedal-down failed");
    const auto off0=applyOriginalSf2MidiNoteOffFromVxdState(
        chip,state,first,0u,60u,true);
    if(!off0 || !off0->deferred_by_sustain ||
       off0->typed_port_writes!=0u)
        throw std::runtime_error("Original first deferred NoteOff failed");
    if(two_voices) {
        const auto off3=applyOriginalSf2MidiNoteOffFromVxdState(
            chip,state,second,0u,64u,true);
        if(!off3 || !off3->deferred_by_sustain ||
           off3->typed_port_writes!=0u)
            throw std::runtime_error("Original second deferred NoteOff failed");
    }
    const auto voice0=kOriginalVxdVoiceRowStart;
    const auto voice3=kOriginalVxdVoiceRowStart+
                      3u*kOriginalVxdVoiceRowStride;
    out.both_deferred=state[voice0]==0xffu &&
        (!two_voices || state[voice3]==0xffu);
    out.pre_release=hashFrames(chip,4096u);
    if(pedal_lift) {
        const auto up=applyOriginalSf2MidiCc64FromVxdState(
            chip,state,all,0u,0u,true);
        if(!up || up->sustain_enabled ||
           up->released_deferred_voices!=(two_voices?2u:1u) ||
           up->typed_port_writes!=(two_voices?8u:4u))
            throw std::runtime_error("Original physical poly pedal-up failed");
        out.released=up->released_deferred_voices;
    }
    out.dcysusv0=core->voice[0].dcysusv;
    if(two_voices)out.dcysusv3=core->voice[3].dcysusv;
    out.both_idle=state[voice0]==0xfeu && state[voice0+1u]==0x0fu &&
        (!two_voices ||
         (state[voice3]==0xfeu && state[voice3+1u]==0x0fu));
    out.post_release=hashFrames(chip,32768u);
    return out;
}
} // namespace
int main(){
    try {
        TempRom saw(0),square(1);
        std::uint32_t ip0=0,ip1=0,ip2=0;
        std::uint32_t ptrx0=0,ptrx1=0,ptrx2=0;
        const auto a=playback(saw.path,ip0,ptrx0);
        const auto b=playback(saw.path,ip1,ptrx1);
        const auto different=playback(square.path,ip2,ptrx2);
        CHECK(ip0==0xc961u && ip1==ip0 && ip2==ip0);
        CHECK(ptrx0==0x1c481b81u && ptrx1==ptrx0 && ptrx2==ptrx0);
        CHECK(a.nonzero>200u && a.peak>20);
        CHECK(different.nonzero>200u && different.peak>20);
        CHECK(a.fnv==b.fnv && a.nonzero==b.nonzero);
        CHECK(a.fnv!=different.fnv);
        // Distinct original SF2 MIDI notes on physical voices 0 and 3
        // MUST both contribute real digital PCM, with the same exact
        // pre-release stream regardless of future pedal lift. Pedal-up
        // then releases BOTH existing source-verified held voices through
        // genuine VXD envelope bytes and chip ports.
        const auto dualHeld=polyPlayback(saw.path,false,true);
        const auto dualReleased=polyPlayback(saw.path,true,true);
        const auto soloHeld=polyPlayback(saw.path,false,false);
        CHECK(dualHeld.ip0==0xc961u &&
              dualHeld.ip3!=0u && dualHeld.ip3!=dualHeld.ip0);
        CHECK(dualHeld.both_deferred && dualReleased.both_deferred);
        CHECK(dualHeld.pre_release.fnv==dualReleased.pre_release.fnv);
        CHECK(dualHeld.post_release.nonzero>500u);
        CHECK(dualHeld.post_release.fnv!=soloHeld.post_release.fnv);
        CHECK(dualReleased.released==2u && dualReleased.both_idle);
        CHECK(dualReleased.dcysusv0==0x8029u &&
              dualReleased.dcysusv3==0x8029u);
        CHECK(dualReleased.post_release.fnv!=dualHeld.post_release.fnv);

        std::cout<<"P002_ORIGINAL_ROM_SF2_NOTEON_REAL_PCM_PASS"
                 <<" source_original_GM0_61_OUT=TRUE"
                 <<" pinned_EMU8000_real_digital_pcm=TRUE"
                 <<" nonzero="<<a.nonzero<<" peak="<<a.peak
                 <<" synthetic_rom_mutation_changes_pcm=TRUE"
                 <<" original_Creative_ROM=NOT_TESTED"
                 <<" original_Win95_PCM_PARITY=NOT_PROVEN"
                 <<" P002_original_dual_sequential_NoteOn_real_PCM=TRUE"
                 <<" P002_source_CC64_two_voice_pedal_lift_changes_PCM=TRUE"
                 <<" P002_original_30_voice_full_polyphonic_runtime=NOT_PROVEN"
                 <<" P002_production_VST3=NOT_CONNECTED\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"P002_DRY_PCM_FAIL "<<ex.what()<<"\n";
        return 1;
    }
}
