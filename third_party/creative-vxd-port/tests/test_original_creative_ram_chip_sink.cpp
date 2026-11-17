#include "OriginalCreativeRamChipSink.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"RAM_PORT_SINK_FAIL:%d %s\n",__LINE__,#c);std::exit(1);}}while(false)
namespace {
struct Event {std::uint16_t port{},value{};};
struct FakeChip {
    bool active=true;
    std::array<Event,2048u> events{};
    std::size_t count=0;
    [[nodiscard]] bool ready() const noexcept {return active;}
    void writePort(std::uint16_t port,std::uint16_t word) noexcept {
        if(count>=events.size())std::abort();
        events[count++]={port,word};
    }
};
}
int main(){
    FakeChip chip;
    auto sink=OriginalCreativeRamChipSink<FakeChip>::bind(chip);
    constexpr std::array<std::int16_t,5> data{0,12,-1,-32768,32767};
    CHECK(sink.upload_words(sink.opaque,0x23fb57u,data));
    CHECK(chip.count==3u+2u*data.size());
    constexpr std::array<Event,13> expected{{
        {0xe22u,0x36u}, {0xa20u,0xfb57u},{0xa22u,0x0023u},
        {0xe22u,0x3au},{0xa20u,0x0000u},
        {0xe22u,0x3au},{0xa20u,0x000cu},
        {0xe22u,0x3au},{0xa20u,0xffffu},
        {0xe22u,0x3au},{0xa20u,0x8000u},
        {0xe22u,0x3au},{0xa20u,0x7fffu}}};
    for(std::size_t i=0;i<expected.size();++i)
        CHECK(chip.events[i].port==expected[i].port &&
              chip.events[i].value==expected[i].value);
    chip.count=0; chip.active=false;
    CHECK(!sink.upload_words(sink.opaque,0x23fb57u,data));
    CHECK(chip.count==0u);
    chip.active=true;
    CHECK(!sink.upload_words(sink.opaque,0x1fffffu,data));
    CHECK(!sink.upload_words(sink.opaque,0x1000000u,data));
    CHECK(!sink.upload_words(sink.opaque,0xfffffeu,data));
    CHECK(!sink.upload_words(sink.opaque,0x200000u,
                             std::span<const std::int16_t>{}));
    CHECK(chip.count==0u);
    // Block boundaries: SMALW selector/address and SMLD selector must
    // reappear on every original transfer; nothing is cached between.
    constexpr std::array<std::int16_t,2> last{0,0};
    CHECK(sink.upload_words(sink.opaque,0x200000u,last));
    CHECK(sink.upload_words(sink.opaque,0x20000eu,last));
    CHECK(chip.count==14u);
    CHECK(chip.events[0].value==0x36u);
    CHECK(chip.events[7].value==0x36u);
    CHECK(chip.events[1].value==0x0000u);
    CHECK(chip.events[8].value==0x000eu);
    CHECK(chip.events[2].value==0x0020u);
    CHECK(chip.events[9].value==0x0020u);
    std::puts("Original Creative SMALW/SMLD native-port bridge: PASS");
}
