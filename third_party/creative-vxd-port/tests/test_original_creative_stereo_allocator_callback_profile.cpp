#include "OriginalCreativeVxdLive.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}} while(false)

struct Chip {
    std::uint8_t selector=0;
    std::array<std::uint32_t,32> vtft{},ccca{};
    std::array<std::uint16_t,32> sus{};
    std::uint32_t logical_io=0;
    static bool out8(void* ctx,std::uint16_t port,std::uint8_t value) noexcept {
        auto& c=*static_cast<Chip*>(ctx);
        if(port!=0x0e22u)return false;
        c.selector=value;++c.logical_io;return true;
    }
    static bool out16(void* ctx,std::uint16_t port,std::uint16_t value) noexcept {
        auto& c=*static_cast<Chip*>(ctx);
        if(port!=0x0a20u || (c.selector&0xe0u)!=0xa0u || value!=0xffu)return false;
        ++c.logical_io;return true;
    }
    static bool in16(void* ctx,std::uint16_t port,std::uint16_t& value) noexcept {
        auto& c=*static_cast<Chip*>(ctx);
        if(port!=0x0a20u || (c.selector&0xe0u)!=0xa0u)return false;
        value=c.sus[c.selector&31u];++c.logical_io;return true;
    }
    static bool in32(void* ctx,std::uint16_t port,std::uint32_t& value) noexcept {
        auto& c=*static_cast<Chip*>(ctx);
        const auto reg=c.selector&0xe0u;
        if(port==0x0620u && reg==0x60u)value=c.vtft[c.selector&31u];
        else if(port==0x0a20u && reg==0u)value=c.ccca[c.selector&31u];
        else return false;
        ++c.logical_io;return true;
    }
    PortIo io() noexcept {return {this,out8,out16,in16,in32,nullptr};}
};
int main(){
    // Replay NONE of the original port values. This source-side state has
    // exactly the same x86 helper CALL *counts* as the nine repeated
    // owner stereo pair 9/10 original voice-allocation segments.
    DeviceState state;
    state.physical_capacity=32u;
    state.mode_count=32u;
    Chip chip;
    chip.vtft.fill(0x05000000u);
    chip.sus.fill(0x8000u);
    chip.ccca.fill(0u);
    for(unsigned i=0u;i<29u;++i){
        state.voices[i].key_marker=0x0101u;
        state.voices[i].sample_end=0x1000u;
        chip.vtft[i]=0x03000000u;
    }
    chip.vtft[9]=0u;
    chip.vtft[10]=0u;
    const auto result=allocate_pair_mode1(state,chip.io());
    CHECK(result.status==Status::Found && !result.immediate);
    CHECK(result.first==9u && result.second==10u);
    CHECK(result.vtft_reads==32u);
    CHECK(result.dcysusv_reads==29u);
    CHECK(result.ccca_reads==29u);
    CHECK(result.completed_release_writes==2u);
    CHECK(result.physical_port_callbacks()==245u);
    CHECK(result.io_reads==90u);
    CHECK(result.io_writes==94u);
    CHECK(chip.logical_io==184u);
    DeviceState idle;
    idle.physical_capacity=32u;
    idle.mode_count=32u;
    Chip other;
    other.vtft[31]=0xea600000u;
    const auto fast=allocate_pair_mode1(idle,other.io());
    CHECK(fast.status==Status::Found && fast.immediate);
    CHECK(fast.first==29u && fast.second==30u);
    CHECK(fast.vtft_reads==3u && fast.dcysusv_reads==0u && fast.ccca_reads==0u);
    CHECK(fast.completed_release_writes==2u);
    CHECK(fast.physical_port_callbacks()==13u);
    std::puts("ORIGINAL_CREATIVE_STEREO_SOURCE_CALL_PROFILE_PASS 245 callbacks, 32/29/29 reads, voice 9/10; early pair 13 callbacks");
}
