#include "CreativeSf2VxdLiveAllocator.h"
#include "CreativeSf2FullNoteOnChip.h"
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
    // Real Win95 Creative VXD: 32 hardware voices but only 30
    // music allocator rows (the two other physical lanes reserved).
    put16(state,6u,32u);put32(state,0x18u,32u);
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
        CHECK(chip.armPortWriteTrace(256u));
        const auto first=candidateOriginalSf2LiveVxdMonoVoice(
            chip,state,true);
        CHECK(first && first->physical_voice==0u &&
              first->reason==OriginalWin95MonoSelectionReason::CompletelyIdle);
        // REAL 2026-11-14 original Creative C4 chip trace:
        // the VXD first scans physical voice0's VTFT only,
        // stops on its idle marker, and writes E22=60 once.
        // Its DCYSUSV and NoteOn writes are emitted by the caller.
        // Any speculative reads of all 30 voices here reproduce
        // the observed +180 extra E22 selectors native bug.
        const auto firstTrace=chip.portWriteTrace();
        CHECK(firstTrace.size()==1u);
        CHECK(firstTrace[0].port==0xe22u &&
              firstTrace[0].value==0x60u &&
              firstTrace[0].frame==0u);
        // Source Win95 C4 physical voice0: interleave the FIVE
        // original 16-bit chip OUTs after allocator VTFT selection,
        // before scanning next layer. Never duplicate first E22=60.
        CHECK(applyOriginalCreativeSf2StopSelectedIdleVoice(chip,*first));
        const auto firstStop=chip.portWriteTrace();
        CHECK(firstStop.size()==6u);
        constexpr std::array<std::uint16_t,6> ports0{
            0xe22u,0xe22u,0xa20u,0xe22u,0x620u,0x622u};
        constexpr std::array<std::uint16_t,6> values0{
            0x60u,0xa0u,0x00ffu,0x60u,0xffffu,0x0000u};
        for(std::size_t i=0;i<firstStop.size();++i)
            CHECK(firstStop[i].port==ports0[i] &&
                  firstStop[i].value==values0[i]);
        // Simulate the ORIGINAL x86 NoteOn marker store; the next
        // candidate must follow the VXD's three-lane source order.
        put16(state,kOriginalVxdVoiceRowStart,0x003cu);
        chip.writePort(0xe22u,(3u<<5u)|0u);
        chip.writePort(0x620u,0);
        chip.writePort(0x622u,0x5000u);
        CHECK(chip.armPortWriteTrace(256u));
        const auto next=candidateOriginalSf2LiveVxdMonoVoice(chip,state,true);
        CHECK(next && next->physical_voice==3u);
        // After assigning original C4 physical voice0, Creative
        // observes non-idle VTFT0 then the idle VTFT3: exactly two
        // selector OUTs, no 30-voice speculative scanner.
        const auto secondTrace=chip.portWriteTrace();
        CHECK(secondTrace.size()==2u);
        CHECK(secondTrace[0].port==0xe22u &&
              secondTrace[0].value==0x60u &&
              secondTrace[1].port==0xe22u &&
              secondTrace[1].value==0x63u);
        // A second SF2 layer in the SAME source NoteOn must skip
        // already-reserved voice0, so it emits ONLY E22=63 instead
        // of reading 0 again. Trace-controlled, no fake chip cache.
        const std::array<std::uint8_t,1> already_reserved{{0u}};
        CHECK(chip.armPortWriteTrace(256u));
        const auto nextLayer=candidateOriginalSf2LiveVxdMonoVoice(
            chip,state,true,already_reserved);
        CHECK(nextLayer && nextLayer->physical_voice==3u &&
              nextLayer->reason==OriginalWin95MonoSelectionReason::CompletelyIdle);
        const auto layeredTrace=chip.portWriteTrace();
        CHECK(layeredTrace.size()==1u &&
              layeredTrace[0].port==0xe22u &&
              layeredTrace[0].value==0x63u);
        CHECK(applyOriginalCreativeSf2StopSelectedIdleVoice(chip,*nextLayer));
        const auto secondStop=chip.portWriteTrace();
        CHECK(secondStop.size()==6u);
        constexpr std::array<std::uint16_t,5> ports3{
            0xe22u,0xa20u,0xe22u,0x620u,0x622u};
        constexpr std::array<std::uint16_t,5> values3{
            0xa3u,0x00ffu,0x63u,0xffffu,0x0000u};
        for(std::size_t i=0;i<5u;++i)
            CHECK(secondStop[i+1u].port==ports3[i] &&
                  secondStop[i+1u].value==values3[i]);
        // Exercise EVERY source allocator lane and physical music voice.
        // The early-idle decision must stop after the selected VTFT;
        // a blanket 30-voice pre-read would add spurious E22 writes.
        constexpr std::array<unsigned,30> original_scan_order{{
            0,3,6,9,12,15,18,21,24,27,
            1,4,7,10,13,16,19,22,25,28,
            2,5,8,11,14,17,20,23,26,29}};
        for(std::size_t selected=0;selected<original_scan_order.size();++selected) {
            for(unsigned physical=0;physical<30u;++physical)
                put16(state,kOriginalVxdVoiceRowStart+
                      physical*kOriginalVxdVoiceRowStride,0x003cu);
            const auto target=original_scan_order[selected];
            put16(state,kOriginalVxdVoiceRowStart+
                  target*kOriginalVxdVoiceRowStride,0x0ffeu);
            CHECK(chip.armPortWriteTrace(256u));
            const auto chosen=candidateOriginalSf2LiveVxdMonoVoice(
                chip,state,true);
            CHECK(chosen && chosen->physical_voice==target &&
                  chosen->reason==OriginalWin95MonoSelectionReason::CompletelyIdle);
            const auto writes=chip.portWriteTrace();
            CHECK(writes.size()==selected+1u);
            for(std::size_t preceding=0;preceding<=selected;++preceding) {
                CHECK(writes[preceding].port==0xe22u);
                CHECK(writes[preceding].value==
                    static_cast<std::uint16_t>(0x60u|original_scan_order[preceding]));
            }
        }
        for(const auto wrong:{30u,31u,33u}) {
            put16(state,0x06u,static_cast<std::uint16_t>(wrong));
            CHECK(!candidateOriginalSf2LiveVxdMonoVoice(chip,state,true));
        }
        put16(state,0x06u,32u);
        put32(state,0x18u,30u);
        CHECK(!candidateOriginalSf2LiveVxdMonoVoice(chip,state,true));
        put32(state,0x18u,32u);
        state[kOriginalVxdRomIdOffset]=0u;
        CHECK(!candidateOriginalSf2LiveVxdMonoVoice(chip,state,true));
        state[kOriginalVxdRomIdOffset]=0x31u;
        put16(state,0x12u,1u);
        CHECK(!candidateOriginalSf2LiveVxdMonoVoice(chip,state,true));
        put16(state,0x12u,0u);
        std::cout<<"P002_LIVE_SOURCE_VXD_MONO_VOICE_ALLOCATOR_REAL_CHIP_PASS "
                 <<"original_32_physical_30_music_voices=TRUE "
                 <<"original_3_lane_priority=TRUE "
                 <<"original_real_VTFT_idle_scan_E22_outs=1_then_2 "
             <<"original_stop_after_each_selection_C4_6_then_6_OUTS=TRUE "
             <<"original_C4_2nd_layer_excludes_VTFT0=TRUE "
                 <<"missing_source_state_FAIL_CLOSED=TRUE "
                 <<"voice_steal_effects_and_Win95_timestamps=NOT_PROVEN\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"P002_LIVE_VXD_ALLOC_FAIL "<<ex.what()<<"\n";
        return 1;
    }
}
