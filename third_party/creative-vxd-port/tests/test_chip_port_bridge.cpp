#include "OriginalCreativeChipBridge.hpp"
#include "OriginalCreativeMidiEngine.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
using namespace awe64::original_vxd;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"BRIDGE_FAIL line=%d %s\n",__LINE__,#x);std::exit(1);}} while(false)
struct FakeChip {
    struct Event {char dir; std::uint16_t port, data;};
    std::array<Event,256> events{};
    std::size_t n=0;
    bool active=true;
    [[nodiscard]] bool ready() const noexcept {return active;}
    void writePort(std::uint16_t port,std::uint16_t data) noexcept {
        if(n<events.size())events[n++]={'w',port,data};
    }
    [[nodiscard]] std::uint16_t readPort(std::uint16_t port) noexcept {
        if(n<events.size())events[n++]={'r',port,0u};
        if(port==0x620u)return 0x1234u;
        if(port==0x622u)return 0xabcd;
        if(port==0xa20u)return 0x8765u;
        if(port==0xa22u)return 0x4321u;
        return 0u;
    }
};
int main(){
    FakeChip chip{};
    const auto io=ChipPortBridge<FakeChip>::bind(chip);
    CHECK(io.out8(io.opaque,0xe22u,0x7eu));
    CHECK(io.out32(io.opaque,0x620u,0xaabbccddu));
    CHECK(chip.n==3u);
    CHECK(chip.events[0].port==0xe22u && chip.events[0].data==0x7eu);
    CHECK(chip.events[1].port==0x620u && chip.events[1].data==0xccddu);
    CHECK(chip.events[2].port==0x622u && chip.events[2].data==0xaabbu);
    std::uint32_t value=0u;
    CHECK(io.in32(io.opaque,0x620u,value));
    CHECK(value==0xabcd1234u);
    CHECK(chip.events[3].dir=='r' && chip.events[3].port==0x620u);
    CHECK(chip.events[4].dir=='r' && chip.events[4].port==0x622u);
    CHECK(io.in32(io.opaque,0xa20u,value));
    CHECK(value==0x43218765u);
    CHECK(chip.events[5].port==0xa20u && chip.events[6].port==0xa22u);

    // Real note program selects E22=(reg<<5)|physical voice, then writes
    // low/high 16-bit chip port callbacks. Voice 30 MUST be supported.
    chip.n=0;
    OriginalVoiceProgram p{};
    p.source_verified=true;
    p.data[0]=0xdec0abcdu;
    p.data[1]=0x0000ffffu;
    p.data[7]=0xe000u;
    p.data[27]=0x053fu;
    CHECK(write_original_note_main(30u,p,io));
    CHECK(chip.n>56u);
    CHECK(chip.events[0].port==0xe22u && chip.events[0].data==0x7eu);
    CHECK(chip.events[1].port==0x620u && chip.events[1].data==0xabcdu);
    CHECK(chip.events[2].port==0x622u && chip.events[2].data==0xdec0u);
    CHECK(chip.events[3].port==0xe22u && chip.events[3].data==0x5eu);
    CHECK(chip.events[chip.n-2u].port==0xe22u &&
          chip.events[chip.n-2u].data==0xbeu);
    CHECK(chip.events[chip.n-1u].port==0xa20u &&
          chip.events[chip.n-1u].data==0x053fu);
    chip.active=false;
    const auto before=chip.n;
    CHECK(!io.out8(io.opaque,0xe22u,0x60u));
    CHECK(!write_original_note_main(0u,p,io));
    CHECK(chip.n==before);
    std::puts("P003_PORTABLE_ORIGINAL_CHIP_BRIDGE_16_32_AND_VOICE30_PASS");
}
