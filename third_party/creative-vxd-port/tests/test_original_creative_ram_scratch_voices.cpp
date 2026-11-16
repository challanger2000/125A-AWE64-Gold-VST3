#include "OriginalCreativeRamScratchVoices.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"SCRATCH_FAIL:%d %s\n",__LINE__,#x);std::exit(1);}}while(false)
namespace {
struct Event{std::uint16_t port{},value{};};
struct Mock {
    std::array<Event,10240> events{};
    std::size_t count=0;
    std::uint8_t selector=0;
    bool reject=false;
    [[nodiscard]] bool put(std::uint16_t port,std::uint16_t value) noexcept {
        if(reject || count==events.size())return false;
        events[count++]={port,value};return true;
    }
    [[nodiscard]] static bool out8(void*p,std::uint16_t port,std::uint8_t value) noexcept {
        auto& m=*static_cast<Mock*>(p);
        if(!m.put(port,value))return false;
        if(port==0xe22)m.selector=value;
        return true;
    }
    [[nodiscard]] static bool out16(void*p,std::uint16_t port,std::uint16_t v) noexcept {
        return static_cast<Mock*>(p)->put(port,v);
    }
    [[nodiscard]] static bool out32(void*p,std::uint16_t port,std::uint32_t v) noexcept {
        auto& m=*static_cast<Mock*>(p);
        return m.put(port,static_cast<std::uint16_t>(v)) &&
            m.put(static_cast<std::uint16_t>(port+2u),static_cast<std::uint16_t>(v>>16u));
    }
    [[nodiscard]] static bool in16(void*,std::uint16_t,std::uint16_t&v) noexcept {
        v=0x8000u;return true;
    }
    [[nodiscard]] static bool in32(void*,std::uint16_t,std::uint32_t&v) noexcept {
        v=0u;return true;
    }
    [[nodiscard]] PortIo bind() noexcept {return {this,out8,out16,in16,in32,out32};}
};
}
int main() {
    DeviceState device{};
    device.mode_count=22u;
    Mock hw{};
    OriginalCreativeRamScratchCycle cycle{};
    cycle.device=&device;cycle.io=hw.bind();cycle.active=true;
    for(std::size_t i=0;i<10u;++i) {
        cycle.selected[i]=static_cast<std::uint8_t>(3u*i);
        device.voices[3u*i].key_marker=0x1234u;
        device.voices[3u*i].blocked=false;
    }
    CHECK(cycle.after_block() && cycle.cycles_completed==1u && !cycle.active);
    CHECK(hw.count==120u && device.mode_count==32u);
    for(std::size_t i=0;i<10u;++i) {
        const auto v=static_cast<std::uint8_t>(i*3u);
        const auto j=i*12u;
        const std::array<Event,12> expected{{
            {0xe22u,v},{0xa20u,0u},{0xa22u,0u},
            {0xe22u,static_cast<std::uint16_t>(v|0xc0u)},
                {0x620u,0x1d8u},{0x622u,0u},
            {0xe22u,static_cast<std::uint16_t>(v|0xe0u)},
                {0x620u,0x1e0u},{0x622u,0u},
            {0xe22u,static_cast<std::uint16_t>(v|0x20u)},
                {0x620u,0u},{0x622u,0x4800u}}};
        for(std::size_t k=0;k<expected.size();++k){
            CHECK(hw.events[j+k].port==expected[k].port);
            CHECK(hw.events[j+k].value==expected[k].value);
        }
        CHECK(device.voices[v].key_marker==0xffeu && device.voices[v].blocked);
    }
    CHECK(!cycle.after_block() && hw.count==120u); // no duplicate restore
    // A missing x86 allocator candidate (FF) MUST NOT modify physical31.
    hw.count=0;device.mode_count=17u;
    cycle.active=true;cycle.selected.fill(no_voice);cycle.selected[1]=31u;
    CHECK(cycle.after_block() && hw.count==12u && device.mode_count==18u);
    CHECK(device.voices[31].blocked && device.voices[31].key_marker==0xffeu);
    // Reconstruct PCOD+0x26AF from the active SOURCE allocator, not from
    // a repeated list of captured physical voice numbers.
    device=DeviceState{};
    device.physical_capacity=32u;
    device.mode_count=32u;
    hw=Mock{};
    cycle=OriginalCreativeRamScratchCycle{};
    cycle.device=&device;cycle.io=hw.bind();
    CHECK(cycle.before_block());
    CHECK(cycle.active && cycle.selected_voices==10u);
    for(std::size_t i=0;i<cycle.selected.size();++i) {
        CHECK(cycle.selected[i]==static_cast<std::uint8_t>(i*3u));
        CHECK(device.voices[i*3u].key_marker==0xff00u);
    }
    CHECK(hw.count==290u);
    // First voice is original VTFT-read selector, release, VTFT reset,
    // then original A20=0x0080 and seven source 32-bit scratch resets.
    CHECK(hw.events[0].port==0xe22u && hw.events[0].value==0x60u);
    CHECK(hw.events[1].port==0xe22u && hw.events[1].value==0xa0u);
    CHECK(hw.events[2].port==0xa20u && hw.events[2].value==0xffu);
    CHECK(hw.events[3].port==0xe22u && hw.events[3].value==0x60u);
    CHECK(hw.events[4].port==0x620u && hw.events[4].value==0xffffu);
    CHECK(hw.events[5].port==0x622u && hw.events[5].value==0u);
    CHECK(hw.events[6].port==0xe22u && hw.events[6].value==0xa0u);
    CHECK(hw.events[7].port==0xa20u && hw.events[7].value==0x80u);
    CHECK(!cycle.before_block() && hw.count==290u); // no double prepare
    CHECK(cycle.after_block());
    CHECK(hw.count==410u && device.mode_count==42u);
    CHECK(cycle.cycles_completed==1u);
    // Faulted transport must be latched; no fabricated hardware rollback.
    hw=Mock{};device=DeviceState{};
    cycle=OriginalCreativeRamScratchCycle{};
    cycle.device=&device;cycle.io=hw.bind();
    hw.reject=true;
    CHECK(!cycle.before_block() && cycle.faulted && cycle.active);
    hw.reject=false;
    CHECK(!cycle.after_block() && !cycle.before_block());
    std::puts("Original PCOD RAM scratch: source-order begin/restore PASS");
}
