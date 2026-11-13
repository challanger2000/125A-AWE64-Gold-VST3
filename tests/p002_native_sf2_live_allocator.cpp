#include "CreativeSf2VxdLiveAllocator.h"
#include "Chip.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
namespace fs=std::filesystem;
using namespace awe64::creative;
#define CHECK(x) do{if(!(x)){std::cerr<<"P002_LIVE_VXD_ALLOC_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
namespace {
void put16(std::array<std::uint8_t,kOriginalVxdMinDeviceImage>& s,
           std::size_t at,std::uint16_t x) {
    s[at]=static_cast<std::uint8_t>(x);
    s[at+1u]=static_cast<std::uint8_t>(x>>8u);
}
void put32(std::array<std::uint8_t,kOriginalVxdMinDeviceImage>& s,
           std::size_t at,std::uint32_t x) {
    for(int i=0;i<4;++i)s[at+i]=static_cast<std::uint8_t>(x>>(8*i));
}
struct SyntheticRom {
    fs::path filename;
    SyntheticRom() {
        filename=fs::temp_directory_path()/"125a-p002-native-live-alloc-1mgm.raw";
        std::ofstream f(filename,std::ios::binary|std::ios::trunc);
        if(!f)throw std::runtime_error("ROM create");
        const std::array<char,4096> zeros{};
        for(int i=0;i<256;++i)f.write(zeros.data(),zeros.size());
        if(!f)throw std::runtime_error("ROM incomplete");
    }
    ~SyntheticRom(){std::error_code ec;fs::remove(filename,ec);}
};
}
int main(){
    awe64::Chip chip{};
    std::array<std::uint8_t,kOriginalVxdMinDeviceImage> state{};
    put32(state,kOriginalVxdRomIdOffset,0x4d474d31u);
    put16(state,6u,30u);put32(state,0x18u,30u);
    for(unsigned voice=0;voice<30u;++voice)
        put16(state,kOriginalVxdVoiceRowStart+
                 voice*kOriginalVxdVoiceRowStride,0x0ffeu);
    CHECK(!candidateOriginalSf2LiveVxdMonoVoice(chip,state,true));
    CHECK(!candidateOriginalSf2LiveVxdMonoVoice(chip,state,false));
    try{
        SyntheticRom rom{};
        std::string error;
        CHECK(chip.open(rom.filename,error));
        // Pinned original EMU8000 source is used for EVERY read, not a
        // prefilled status vector. Initialize the authentic VTFT DATA0
        // voice register to zero exactly as the original idle branch
        // requires. These 30 writes are CI synthetic test setup.
        for(unsigned voice=0;voice<30u;++voice) {
            chip.writePort(0xe22u,static_cast<std::uint16_t>(
                (3u<<5u)|voice));
            chip.writePort(0x620u,0);
            chip.writePort(0x622u,0);
        }
        const auto first=candidateOriginalSf2LiveVxdMonoVoice(
            chip,state,true);
        CHECK(first && first->physical_voice==0u &&
              first->reason==OriginalWin95MonoSelectionReason::CompletelyIdle);
        // Simulate the ORIGINAL x86 NoteOn marker store; the next
        // candidate must follow the VXD's three-lane source order.
        put16(state,kOriginalVxdVoiceRowStart,0x003cu);
        chip.writePort(0xe22u,(3u<<5u)|0u);
        chip.writePort(0x620u,0);
        chip.writePort(0x622u,0x5000u);
        const auto next=candidateOriginalSf2LiveVxdMonoVoice(chip,state,true);
        CHECK(next && next->physical_voice==3u);
        put16(state,0x06u,31u);
        CHECK(!candidateOriginalSf2LiveVxdMonoVoice(chip,state,true));
        put16(state,0x06u,30u);
        state[kOriginalVxdRomIdOffset]=0u;
        CHECK(!candidateOriginalSf2LiveVxdMonoVoice(chip,state,true));
        state[kOriginalVxdRomIdOffset]=0x31u;
        put16(state,0x12u,1u);
        CHECK(!candidateOriginalSf2LiveVxdMonoVoice(chip,state,true));
        put16(state,0x12u,0u);
        std::cout<<"P002_LIVE_SOURCE_VXD_MONO_VOICE_ALLOCATOR_REAL_CHIP_PASS "
                 <<"source_30_music_voices=TRUE "
                 <<"original_3_lane_priority=TRUE "
                 <<"real_chip_VTFT_DCYSUSV_CCCA_reads=TRUE "
                 <<"missing_source_state_FAIL_CLOSED=TRUE "
                 <<"voice_steal_effects_and_Win95_timestamps=NOT_PROVEN\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"P002_LIVE_VXD_ALLOC_FAIL "<<ex.what()<<"\n";
        return 1;
    }
}
