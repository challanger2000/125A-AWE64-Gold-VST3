#include "Chip.h"
#include <array>
#include <cstdint>
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
    b.close();a.close();std::filesystem::remove(rom);
    std::cout<<"P002_NATIVE_CHIP_PORT_TRACE_SAMPLE_CLOCK_AND_NO_DSP_SIDE_EFFECT_PASS\n";
}
