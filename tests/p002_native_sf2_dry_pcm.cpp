// P002 actual audible ROM SF2 NoteOn through the unchanged pinned EMU8000.
// Uses SYNTHETIC 1-MiB ROM (not copyrighted original ROM) and the
// independently source-derived original GM0 61-OUT sequence. Validates
// real PCM generation and audible sensitivity to ROM wave contents.
// Not independent Win95 Creative driver PCM equivalence or production VST3.
#include "CreativeSf2VxdStateSnapshot.h"
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
        std::cout<<"P002_ORIGINAL_ROM_SF2_NOTEON_REAL_PCM_PASS"
                 <<" source_original_GM0_61_OUT=TRUE"
                 <<" pinned_EMU8000_real_digital_pcm=TRUE"
                 <<" nonzero="<<a.nonzero<<" peak="<<a.peak
                 <<" synthetic_rom_mutation_changes_pcm=TRUE"
                 <<" original_Creative_ROM=NOT_TESTED"
                 <<" original_Win95_PCM_PARITY=NOT_PROVEN"
                 <<" P002_production_VST3=NOT_CONNECTED\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"P002_DRY_PCM_FAIL "<<ex.what()<<"\n";
        return 1;
    }
}
