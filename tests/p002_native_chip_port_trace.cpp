#include "Chip.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#define CHECK(x) do {if(!(x)){std::cerr<<"P002_PORT_TRACE_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(){
    using namespace awe64;
    const auto rom=std::filesystem::temp_directory_path()/
        "125a-p002-trace-SYNTHETIC-ROM.raw";
    {std::ofstream f(rom,std::ios::binary|std::ios::trunc);
     CHECK(f);const std::array<char,4096> zero{};
     for(unsigned i=0;i<256;++i)f.write(zero.data(),zero.size());
     CHECK(f);}
    Chip a,b;
    std::string err;
    CHECK(a.open(rom,err));CHECK(b.open(rom,err));
    CHECK(a.portWriteTrace().empty());
    CHECK(!a.portWriteTraceOverflow());
    CHECK(!a.armPortWriteTrace(0));
    CHECK(!a.armPortWriteTrace(262145));
    CHECK(a.armPortWriteTrace(4));
    CHECK(a.renderedFrames()==0);
    a.writePort(0xE22u,0x1Fu);b.writePort(0xE22u,0x1Fu);
    a.writePort(0xA20u,0x0004u);b.writePort(0xA20u,0x0004u);
    CHECK(a.portWriteTrace().size()==2u);
    CHECK(a.portWriteTrace()[0].frame==0u &&
          a.portWriteTrace()[0].port==0xE22u &&
          a.portWriteTrace()[0].value==0x1Fu);
    CHECK(a.portWriteTrace()[1].frame==0u &&
          a.portWriteTrace()[1].port==0xA20u &&
          a.portWriteTrace()[1].value==4u);
    std::array<std::int32_t,2u*128u> x{},y{};
    a.render(x.data(),31);b.render(y.data(),31);
    CHECK(a.renderedFrames()==31);
    a.writePort(0xE22u,0x03u);b.writePort(0xE22u,0x03u);
    a.writePort(0xA20u,0x1234u);b.writePort(0xA20u,0x1234u);
    CHECK(a.portWriteTrace().size()==4u);
    CHECK(a.portWriteTrace()[2].frame==31u &&
          a.portWriteTrace()[3].frame==31u);
    a.writePort(0xA22u,0x5678u);b.writePort(0xA22u,0x5678u);
    CHECK(a.portWriteTraceOverflow());
    CHECK(a.portWriteTrace().size()==4u);
    a.render(x.data()+62,97);b.render(y.data()+62,97);
    CHECK(a.renderedFrames()==128u && x==y);
    // Identical real-fork PCM both with and without tracing,
    // even when the bounded recorder overflows and fails closed.
    CHECK(a.armPortWriteTrace(1));
    CHECK(!a.portWriteTraceOverflow() && a.portWriteTrace().empty());
    a.writePort(0xE22u,0x11u);
    CHECK(a.portWriteTrace().size()==1u &&
          a.portWriteTrace()[0].frame==128u);
    a.disarmPortWriteTrace();
    a.writePort(0xA20u,0x100u);
    CHECK(a.portWriteTrace().size()==1u);
    a.close();
    CHECK(!a.armPortWriteTrace(2));
    CHECK(a.open(rom,err));
    CHECK(a.portWriteTrace().empty() && a.renderedFrames()==0u);
    // Exercise the real chip state rollback that makes the source-clock
    // native NoteOn integration possible: authentic allocator writes
    // are generated and read against hardware, then rolled back
    // without polluting the independent post-clock recorder.
    Chip direct;
    CHECK(direct.open(rom,err));
    CHECK(a.armNativeNoteOutTransaction());
    std::vector<std::byte> originalChip(sizeof(emu8k_t));
    std::memcpy(originalChip.data(),a.rawForContractTesting(),
                originalChip.size());
    CHECK(a.beginNativeNoteOutTransaction());
    CHECK(!a.beginNativeNoteOutTransaction());
    a.writePort(0xe22u,0x1fu);
    a.writePort(0xa20u,0x0004u);
    a.writePort(0xe22u,0x60u);
    (void)a.readPort(0x620u); // native voice allocator reads genuine chip
    a.writePort(0x620u,0xffffu);
    a.writePort(0x622u,0x0000u);
    CHECK(a.nativeNoteOutTransaction().size()==5u);
    CHECK(a.nativeNoteOutTransaction()[2].value==0x60u);
    CHECK(a.renderedFrames()==0u);
    CHECK(a.endNativeNoteOutTransaction());
    CHECK(!a.endNativeNoteOutTransaction());
    CHECK(!a.nativeNoteOutTransactionActive());
    CHECK(std::memcmp(originalChip.data(),a.rawForContractTesting(),
                      originalChip.size())==0);
    // The five native chip port OUTs were NOT sent to the public
    // write-only recorder; their real execution happens later.
    CHECK(a.portWriteTrace().empty());
    for(const auto& p:a.nativeNoteOutTransaction()) {
        a.writePort(p.port,p.value);
        direct.writePort(p.port,p.value);
    }
    std::array<std::int32_t,2u*64u> actual{},baseline{};
    a.render(actual.data(),64u);
    direct.render(baseline.data(),64u);
    CHECK(actual==baseline);
    // A speculative original NoteOn MUST NOT be able to write to the
    // 28-MiB sample RAM: restoring only emu8k_t would not undo it.
    // Original Creative SMALW/SMLD use selector 0x36/0x3a on DATA1.
    // Both 0x36 and 0x3a are also valid MUSIC voice 22/26 DATA3
    // selectors, so the guard must distinguish the DATA PORT.
    const auto beforeValue=a.rawForContractTesting()->ram[16];
    std::memcpy(originalChip.data(),a.rawForContractTesting(),
                originalChip.size());
    CHECK(a.beginNativeNoteOutTransaction());
    a.writePort(0xe22u,0x36u);
    a.writePort(0xe20u,0x1234u); // legal music DATA3
    a.writePort(0xe22u,0x3au);
    a.writePort(0xe20u,0x5678u); // legal music DATA3
    CHECK(a.nativeNoteOutTransaction().size()==4u);
    CHECK(a.endNativeNoteOutTransaction());
    CHECK(a.rawForContractTesting()->ram[16]==beforeValue);
    CHECK(std::memcmp(originalChip.data(),a.rawForContractTesting(),
                      originalChip.size())==0);

    for(const auto forbidden:{0x36u,0x3au}) {
        std::memcpy(originalChip.data(),a.rawForContractTesting(),
                    originalChip.size());
        CHECK(a.beginNativeNoteOutTransaction());
        a.writePort(0xe22u,static_cast<std::uint16_t>(forbidden));
        CHECK(!a.nativeNoteOutTransactionOverflow());
        const auto accepted=a.nativeNoteOutTransaction().size();
        a.writePort(0xa20u,0xffffu); // forbidden SMALW/SMLD DATA1
        CHECK(a.nativeNoteOutTransactionOverflow());
        CHECK(a.nativeNoteOutTransaction().size()==accepted);
        CHECK(!a.endNativeNoteOutTransaction());
        CHECK(std::memcmp(originalChip.data(),a.rawForContractTesting(),
                          originalChip.size())==0);
        CHECK(a.rawForContractTesting()->ram[16]==beforeValue);
    }
    CHECK(a.beginNativeNoteOutTransaction());
    a.writePort(0xffffu,0x1u); // speculative unknown chip port
    CHECK(a.nativeNoteOutTransactionOverflow());
    CHECK(a.nativeNoteOutTransaction().empty());
    CHECK(!a.endNativeNoteOutTransaction());
    direct.close();
    b.close();a.close();std::filesystem::remove(rom);
    std::cout<<"P002_NATIVE_CHIP_PORT_TRACE_SAMPLE_CLOCK_AND_NO_DSP_SIDE_EFFECT_PASS\n";
}
