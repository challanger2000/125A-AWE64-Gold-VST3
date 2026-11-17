#include "OriginalCreativeModulationController.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(v) do{if(!(v)){std::fprintf(stderr,"MOD_FAIL %d %s\n",__LINE__,#v);std::exit(1);}}while(false)
struct Bus {
    std::uint8_t sel=0u;
    std::array<std::uint16_t,32> fmmod{};
    unsigned write_count=0, read_count=0;
    bool fail=false;
    static bool out8(void* p,std::uint16_t port,std::uint8_t v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(b.fail || port!=0xe22u)return false;
        b.sel=v;return true;
    }
    static bool out16(void* p,std::uint16_t port,std::uint16_t v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(b.fail || port!=0xe20u || (b.sel&0xe0u)!=0x60u)return false;
        b.fmmod[b.sel&31u]=v;++b.write_count;return true;
    }
    static bool in16(void* p,std::uint16_t port,std::uint16_t& v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(b.fail || port!=0xe20u || (b.sel&0xe0u)!=0x60u)return false;
        v=b.fmmod[b.sel&31u];++b.read_count;return true;
    }
    PortIo io() noexcept{return {this,out8,out16,in16,nullptr,nullptr};}
};
int main() {
    MidiEngine e{};Bus b{};
    auto& row=e.device.voices[30];
    row.key_marker=0x023cu;
    row.original_modulation_word_verified=true;
    row.original_modulation_word_signed=-4;
    auto& c=e.channel[2];
    c.original_modulation_byte_44d=7u;
    c.original_modulation_byte_44d_verified=true;
    b.fmmod[30]=0xb412u;
    CHECK(dispatch_midi(e,0xb2u,1u,90u,b.io(),{})==MidiResult::Applied);
    CHECK(c.modulation==90u && c.original_modulation_bucket==3u);
    CHECK(b.fmmod[30]==0x0612u && b.read_count==1u && b.write_count==1u);
    // Original CC1 uses value/30 bucket, not a linear 0..127 scale.
    CHECK(dispatch_midi(e,0xb2u,1u,119u,b.io(),{})==MidiResult::Applied);
    CHECK(c.original_modulation_bucket==3u && b.fmmod[30]==0x0612u);
    c.original_modulation_byte_44d=127u;
    CHECK(dispatch_midi(e,0xb2u,1u,127u,b.io(),{})==MidiResult::Applied);
    CHECK(b.fmmod[30]==0x7f12u); // cap high at 127, low byte preserved
    c.original_modulation_byte_44d=0u;
    row.original_modulation_word_signed=-10;
    CHECK(dispatch_midi(e,0xb2u,1u,0u,b.io(),{})==MidiResult::Applied);
    CHECK(b.fmmod[30]==0xf612u); // negative signed SHL wrap, not 0
    const auto writes=b.write_count;
    c.original_modulation_byte_44d_verified=false;
    CHECK(dispatch_midi(e,0xb2u,1u,61u,b.io(),{})==MidiResult::MissingSource);
    CHECK(b.write_count==writes && c.modulation==0u);
    c.original_modulation_byte_44d_verified=true;
    b.fail=true;
    CHECK(dispatch_midi(e,0xb2u,1u,61u,b.io(),{})==MidiResult::PortError);
    CHECK(e.fault && dispatch_midi(e,0xb2u,1u,62u,b.io(),{})==MidiResult::FaultLatched);
    std::puts("P003_ORIGINAL_CC1_FMMOD_LIVE_HIBYTE_PRESERVE_TEST_PASS");
}
