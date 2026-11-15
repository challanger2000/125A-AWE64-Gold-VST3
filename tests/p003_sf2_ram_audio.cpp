// P003 end-to-end synthetic SoundFont -> original EMU8000 RAM ->
// MIDI source-note sample mapping -> original 61-OUT chip voice -> PCM.
// Research integration only: does not claim independent Creative Win95
// driver note/PCM equivalence or switch production VST3 playback.
#include "CreativeSf2RamBankUpload.h"
#include "CreativeSf2FullNoteOnChip.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
using namespace awe64::creative;
using B=std::vector<std::uint8_t>;
#define CHECK(x) do {if(!(x)){std::cerr<<"P003_RAM_AUDIO_FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
namespace {
void w16(B& b,std::uint32_t x){b.push_back(std::uint8_t(x));b.push_back(std::uint8_t(x>>8u));}
void w32(B& b,std::uint32_t x){w16(b,x);w16(b,x>>16u);}
void chunk(B& b,std::string_view id,const B& data){
    b.insert(b.end(),id.begin(),id.end());w32(b,std::uint32_t(data.size()));
    b.insert(b.end(),data.begin(),data.end());if(data.size()&1u)b.push_back(0u);
}
void name(B& b,std::string_view s){b.insert(b.end(),s.begin(),s.end());b.insert(b.end(),20u-s.size(),0u);}
B makeSf2(bool inverse){
    constexpr std::uint32_t n=16384u;
    B info{'I','N','F','O'};chunk(info,"ifil",{2,0,1,0});
    B pcm;
    for(std::uint32_t i=0;i<n;++i) {
        const int wave=((i%128u)<64u?1:-1)*(inverse?-1:1)*12500;
        w16(pcm,std::uint16_t(static_cast<std::int16_t>(wave)));
    }
    B sdta{'s','d','t','a'};chunk(sdta,"smpl",pcm);
    B pdta{'p','d','t','a'},phdr,pbag,pgen,inst,ibag,igen,shdr;
    name(phdr,"P003-RAM");w16(phdr,7);w16(phdr,0);w16(phdr,0);
    w32(phdr,0);w32(phdr,0);w32(phdr,0);
    name(phdr,"EOP");w16(phdr,0);w16(phdr,0);w16(phdr,1);
    w32(phdr,0);w32(phdr,0);w32(phdr,0);
    w16(pbag,0);w16(pbag,0);w16(pbag,1);w16(pbag,0);
    w16(pgen,41);w16(pgen,0);
    name(inst,"P003-INST");w16(inst,0);
    name(inst,"EOI");w16(inst,1);
    w16(ibag,0);w16(ibag,0);w16(ibag,1);w16(ibag,0);
    w16(igen,53);w16(igen,0);
    name(shdr,"P003-SAMPLE");w32(shdr,0);w32(shdr,n);
    w32(shdr,64);w32(shdr,n-64);w32(shdr,44100);
    shdr.push_back(60);shdr.push_back(0);w16(shdr,0);w16(shdr,1);
    name(shdr,"EOS");for(int i=0;i<5;++i)w32(shdr,0);
    shdr.push_back(0);shdr.push_back(0);w16(shdr,0);w16(shdr,0);
    chunk(pdta,"phdr",phdr);chunk(pdta,"pbag",pbag);
    chunk(pdta,"pgen",pgen);chunk(pdta,"inst",inst);
    chunk(pdta,"ibag",ibag);chunk(pdta,"igen",igen);
    chunk(pdta,"shdr",shdr);
    B root{'s','f','b','k'};
    chunk(root,"LIST",info);chunk(root,"LIST",sdta);chunk(root,"LIST",pdta);
    B riff{'R','I','F','F'};w32(riff,std::uint32_t(root.size()));
    riff.insert(riff.end(),root.begin(),root.end());return riff;
}
void gen(CreativeSf2VoiceDescriptorBytes& b,unsigned op,int value){
    const auto w=std::uint16_t(value);b[2u*op]=std::uint8_t(w);
    b[2u*op+1u]=std::uint8_t(w>>8u);
}
void dword(CreativeSf2VoiceDescriptorBytes& b,unsigned off,std::uint32_t x){
    for(unsigned j=0;j<4u;++j)b[off+j]=std::uint8_t(x>>(j*8u));
}
struct Result{std::uint32_t nonzero=0;std::uint64_t hash=0xcbf29ce484222325ull;};
Result render(const std::filesystem::path& rom,const B& sf2){
    awe64::Chip chip;std::string error;
    if(!chip.open(rom,error))throw std::runtime_error(error);
    constexpr std::uint32_t addr=awe64::kSampleRamBase+16u;
    P003RamUploadResult upload{};
    if(!p003UploadValidatedMonoSf2PoolToOriginalChip(chip,sf2,addr,upload,error))
        throw std::runtime_error(error);
    const auto note=p003CandidatePhysicalNoteFromVerifiedRawPool(
        chip,sf2,upload,0u,7u,60u,100u);
    if(!note || note->count!=1u)throw std::runtime_error("MIDI RAM address resolution");
    const auto& physical=note->layers[0];
    OriginalSf2FullNoteOnVoice voice{};
    voice.played_midi_key=60;voice.physical_voice=0;
    voice.source_original_voice_verified=true;
    voice.original_ram_sample_verified=true;
    voice.lfo={true,0,0};voice.target={true,0,0x1c48};
    auto& b=voice.verified_after_start_record;
    gen(b,58,60);gen(b,56,100);gen(b,52,0);
    gen(b,8,179);gen(b,19,58);gen(b,34,125);gen(b,35,120);
    gen(b,26,125);gen(b,27,127);gen(b,28,127);gen(b,29,0);
    for(unsigned g:{21u,23u,25u,33u})gen(b,g,0x8000);
    gen(b,22,128);gen(b,54,1);gen(b,15,0);gen(b,16,0);
    gen(b,17,127);gen(b,18,129);gen(b,36,3);gen(b,37,0);
    b[0x8au]=60;b[0x8cu]=1;b[0x88u]=0xe0;
    dword(b,0x76,physical.physical_sample_start);
    dword(b,0x7a,physical.physical_sample_end);
    dword(b,0x7e,physical.physical_loop_start);
    dword(b,0x82,physical.physical_loop_end);
    // Original chip hardware stereo unmute (not a second DSP).
    chip.writePort(0xe22u,(1u<<5u)|31u);chip.writePort(0xa20u,4u);
    if(!applyOriginalCreativeSf2FullNoteOnPorts(chip,
       std::span<const OriginalSf2FullNoteOnVoice>(&voice,1u)))
        throw std::runtime_error("61-port RAM NoteOn rejected");
    std::array<std::int32_t,1024> frames{};
    Result out{};
    for(unsigned block=0;block<24u;++block){
        chip.render(frames.data(),512u);
        for(const auto v:frames){
            if(v)++out.nonzero;
            const auto u=std::uint32_t(v);
            for(unsigned byte=0;byte<4u;++byte) {
                out.hash^=(u>>(byte*8u))&255u;
                out.hash*=0x100000001b3ull;
            }
        }
    }
    return out;
}
}
int main(){
    try {
        const auto rom=std::filesystem::temp_directory_path()/
            "125a-p003-synthetic-ram-playback-rom.raw";
        {
            std::ofstream file(rom,std::ios::binary|std::ios::trunc);
            const std::array<char,4096> zero{};
            for(unsigned i=0;i<256u;++i)file.write(zero.data(),zero.size());
            CHECK(file.good());
        }
        const auto a=render(rom,makeSf2(false));
        const auto repeated=render(rom,makeSf2(false));
        const auto inverted=render(rom,makeSf2(true));
        std::error_code ec;std::filesystem::remove(rom,ec);
        CHECK(a.nonzero>100u);
        CHECK(a.hash==repeated.hash && a.nonzero==repeated.nonzero);
        CHECK(a.hash!=inverted.hash);
        std::cout<<"P003_REAL_EMU8000_RAM_SF2_NOTE_PCM_PASS"
            <<" selected_MIDI_program=7 selected_sample_id=0"
            <<" original_61_OUT_chip=TRUE"
            <<" deterministic_pcm=TRUE source_mutation_changes_pcm=TRUE"
            <<" original_Win95_pcm_parity=NOT_PROVEN"
            <<" production_VST3=NOT_CONNECTED\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"P003_RAM_AUDIO_FAIL "<<e.what()<<"\n";return 1;
    }
}
