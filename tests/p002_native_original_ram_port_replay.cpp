#include "CreativeOriginalRamPortReplay.h"
#include <86box/sound.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using awe64::creative::OriginalRamUploadPortWrite;
using awe64::creative::candidateReplayOriginalCreativeCapturedRamUpload;
#define CHECK(x) do { if(!(x)) { \
    std::cerr<<"P002_RAM_PORT_REPLAY_FAIL line "<<__LINE__ \
             <<" condition "#x"\n";return 1; } } while(false)
namespace {
using Writes=std::vector<OriginalRamUploadPortWrite>;
void setPointer(Writes& trace,std::uint64_t frame,std::uint32_t at) {
    trace.push_back({frame,0xe22u,0x36u});
    trace.push_back({frame,0xa20u,static_cast<std::uint16_t>(at)});
    trace.push_back({frame,0xa22u,static_cast<std::uint16_t>(at>>16u)});
    trace.push_back({frame,0xe22u,0x3au});
}
void addWords(Writes& trace,std::uint64_t frame,
              std::uint32_t at,std::span<const std::uint16_t> words) {
    setPointer(trace,frame,at);
    for(const auto data:words)
        trace.push_back({frame++,0xa20u,data});
}
}
int main() {
    const auto rom=std::filesystem::temp_directory_path()/
        "125a-p002-original-RAM-port-replay-synthetic-ROM.raw";
    {
        std::ofstream file(rom,std::ios::binary|std::ios::trunc);
        CHECK(bool(file));
        const std::array<char,4096> empty{};
        for(int i=0;i<256;++i)file.write(empty.data(),empty.size());
        CHECK(bool(file));
    }
    awe64::Chip chip;
    std::string error;
    CHECK(chip.open(rom,error));
    std::error_code ec;std::filesystem::remove(rom,ec);
    auto* core=chip.rawForContractTesting();
    CHECK(core!=nullptr && core->ram!=nullptr);
    const std::array<std::uint16_t,4> data{
        {0x8001u,0x7fffu,0x00ffu,0xabcdu}};
    Writes traced{};
    addWords(traced,5u,0x200010u,data);
    const std::array<std::uint16_t,1> last{{0x55aau}};
    addWords(traced,11u,0xffffdfu,last);
    const auto before=std::array<std::uint16_t,5>{{
        static_cast<std::uint16_t>(core->ram[0x10u]),
        static_cast<std::uint16_t>(core->ram[0x11u]),
        static_cast<std::uint16_t>(core->ram[0x12u]),
        static_cast<std::uint16_t>(core->ram[0x13u]),
        static_cast<std::uint16_t>(
            core->ram[0xffffdfu-awe64::kSampleRamBase])}};
    // Synthetic public test trace is NOT independently authenticated;
    // FALSE must NEVER write or accept it as proven original Win95.
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,traced,false));
    CHECK(static_cast<std::uint16_t>(core->ram[0x10u])==before[0]);
    // Negative original trace with otherwise valid first RAM word, but
    // second segment crosses the EMU8000 FM-reserved high memory gate.
    Writes overflow=traced;
    overflow.push_back({14u,0xa20u,0xdead});
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,overflow,true));
    CHECK(static_cast<std::uint16_t>(core->ram[0x10u])==before[0]);
    CHECK(static_cast<std::uint16_t>(
        core->ram[0xffffdfu-awe64::kSampleRamBase])==before[4]);
    // Missing high address, wrong selector, wrong DATA port, duplicate
    // low, backwards original chip clock, and forged non-RAM pointer
    // are each rejected before any pin-unmodified-chip port effects.
    auto invalid=traced;
    invalid.erase(invalid.begin()+2);
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid[0].value=0x37u;
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid[4].port=0xa22u;
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid.insert(invalid.begin()+2,
                                  {5u,0xa20u,0x0010u});
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid[5].original_chip_frame=0u;
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid[1].value=0xffffu;invalid[2].value=0x001fu;
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;invalid[9].value=0xffe0u;
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true));
    invalid=traced;
    invalid.pop_back();invalid.pop_back();invalid.pop_back();
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,invalid,true)); // dangling pointer before SMLD
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,{},true));
    chip.close();
    CHECK(!candidateReplayOriginalCreativeCapturedRamUpload(
        chip,traced,true)); // no chip allocated
    CHECK(chip.open(std::filesystem::temp_directory_path()/
                    "125a-p002-original-RAM-port-replay-synthetic-ROM.raw",
                    error)==false); // already deleted original fixture
    // Re-create an original-fork chip with an explicitly SYNTHETIC ROM
    // and verify actual, typed low/high EMU8000 port data, NOT memcpy.
    {
        std::ofstream file(rom,std::ios::binary|std::ios::trunc);
        CHECK(bool(file));
        const std::array<char,4096> empty{};
        for(int i=0;i<256;++i)file.write(empty.data(),empty.size());
        CHECK(bool(file));
    }
    CHECK(chip.open(rom,error));
    std::filesystem::remove(rom,ec);
    core=chip.rawForContractTesting();
    const auto done=candidateReplayOriginalCreativeCapturedRamUpload(
        chip,traced,true);
    CHECK(done && done->exact_port_writes==traced.size() &&
          done->original_sample_words==5u &&
          done->first_word_address==0x200010u &&
          done->last_word_address==0xffffdfu &&
          done->first_chip_frame==5u &&
          done->last_chip_frame==11u);
    for(unsigned i=0;i<4u;++i)
        CHECK(static_cast<std::uint16_t>(core->ram[0x10u+i])==data[i]);
    CHECK(static_cast<std::uint16_t>(
        core->ram[0xffffdfu-awe64::kSampleRamBase])==0x55aau);
    std::cout<<"P002_SOURCE_ORIGINAL_EMU8000_RAM_PORT_REPLAY_PASS"
             <<" source_32bit_word_addresses=TRUE"
             <<" original_SMALW_SMLD_port_order=TRUE"
             <<" unmodified_chip_actual_RAM_contents=TRUE"
             <<" full_preflight_before_hardware_write=TRUE"
             <<" provenance_gate_and_original_28MiB_bounds=TRUE"
             <<" source_guest_trace_in_test=SYNTHETIC_ONLY"
             <<" original_Creative_driver_RAM_upload_verified=NOT_YET\n";
    return 0;
}
