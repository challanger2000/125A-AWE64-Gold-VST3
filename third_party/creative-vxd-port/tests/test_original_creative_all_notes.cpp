#include "OriginalCreativeMidiEngine.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

using namespace awe64::original_vxd;
#define REQUIRE(x) do { if(!(x)){ std::fprintf(stderr,"CC120_123_FAILURE line=%d %s\n",__LINE__,#x);std::exit(1);} }while(false)

struct TracePort {
    struct Write {std::uint16_t port;std::uint32_t value;std::uint8_t sel;};
    std::array<Write,256> written{};
    std::size_t n=0;
    std::uint8_t selected=0;
    std::array<std::uint32_t,32> csl{},psst{};
    bool emit(std::uint16_t port,std::uint32_t value) noexcept {
        if(n==written.size())return false;
        written[n++]={port,value,selected};return true;
    }
    static bool out8(void* p,std::uint16_t port,std::uint8_t v) noexcept {
        auto& self=*static_cast<TracePort*>(p);
        if(port!=0xe22u)return false;
        self.selected=v;return self.emit(port,v);
    }
    static bool out16(void* p,std::uint16_t port,std::uint16_t v) noexcept {
        auto& self=*static_cast<TracePort*>(p);
        if(port!=0xa20u)return false;
        return self.emit(port,v);
    }
    static bool out32(void* p,std::uint16_t port,std::uint32_t v) noexcept {
        auto& self=*static_cast<TracePort*>(p);
        if(port!=0x620u)return false;
        const auto voice=self.selected&31u;
        if((self.selected&0xe0u)==0xe0u)self.csl[voice]=v;
        else if((self.selected&0xe0u)==0xc0u)self.psst[voice]=v;
        else return false;
        return self.emit(port,v);
    }
    static bool in32(void* p,std::uint16_t port,std::uint32_t& value) noexcept {
        auto& self=*static_cast<TracePort*>(p);
        if(port!=0x620u)return false;
        const auto voice=self.selected&31u;
        if((self.selected&0xe0u)==0xe0u)value=self.csl[voice];
        else if((self.selected&0xe0u)==0xc0u)value=self.psst[voice];
        else return false;
        return true;
    }
    static bool in16(void*,std::uint16_t,std::uint16_t&) noexcept {return false;}
    PortIo io() noexcept {return {this,out8,out16,in16,in32,out32};}
};

static void assign(MidiEngine& e,unsigned voice,unsigned channel,unsigned key,
                   unsigned sample_end=0) {
    auto& r=e.device.voices[voice];
    r.key_marker=static_cast<std::uint16_t>(channel*256u+key);
    r.volume_release=0x37u;
    r.modulation_release=0x18u;
    r.release_sample_end=sample_end;
}
int main(){
    {
        // CC123 respects held sustain: all current voices on THIS channel
        // are deferred without touching EMU8000; other channel untouched.
        MidiEngine e;TracePort bus;
        assign(e,0,2,60);assign(e,30,2,65);assign(e,1,4,67);
        e.device.channel_sustain[2]=true;
        REQUIRE(dispatch_midi(e,0xB2u,123u,0u,bus.io(),{})==MidiResult::Applied);
        REQUIRE(bus.n==0u);
        REQUIRE(e.device.voices[0].key_marker==0x02ffu);
        REQUIRE(e.device.voices[30].key_marker==0x02ffu);
        REQUIRE(e.device.voices[1].key_marker==0x0443u);
        REQUIRE(e.device.channel_sustain[2]);
        // Original CC120 bypasses sustain entirely: release BOTH deferred
        // voices on this channel; sustain latch remains held.
        REQUIRE(dispatch_midi(e,0xB2u,120u,0u,bus.io(),{})==MidiResult::Applied);
        REQUIRE(bus.n==8u);
        REQUIRE(e.device.voices[0].key_marker==0x0ffeu);
        REQUIRE(e.device.voices[30].key_marker==0x0ffeu);
        REQUIRE(e.device.voices[1].key_marker==0x0443u);
        REQUIRE(e.device.channel_sustain[2]);
        const std::array<unsigned,8> expected_indices{0xa0u,0xa0u,0xe0u,0xe0u,
                                                       0xbeu,0xbeu,0xfeu,0xfeu};
        for(std::size_t i=0;i<8u;++i)REQUIRE(bus.written[i].sel==expected_indices[i]);
        REQUIRE(bus.written[1].value==0x8037u && bus.written[3].value==0x8018u);
        REQUIRE(bus.written[5].value==0x8037u && bus.written[7].value==0x8018u);
    }
    {
        // Sustain inactive: CC123 releases active voices immediately.
        MidiEngine e;TracePort bus;
        assign(e,5,0,0);assign(e,6,1,60);
        REQUIRE(dispatch_midi(e,0xB0u,123u,0u,bus.io(),{})==MidiResult::Applied);
        REQUIRE(bus.n==4u && e.device.voices[5].key_marker==0x0ffeu);
        REQUIRE(e.device.voices[6].key_marker==0x013cu);
        // No matching voice: source driver returns success without I/O.
        const auto n=bus.n;
        REQUIRE(dispatch_midi(e,0xB0u,120u,0u,bus.io(),{})==MidiResult::Applied);
        REQUIRE(bus.n==n);
    }
    {
        // Original CSL/PSST preservation on CC120 (same release routine
        // as MIDI note-off); both 32-bit reads precede their writes.
        MidiEngine e;TracePort bus;
        assign(e,29,3,74,0x12345u);
        bus.csl[29]=0x55099999u;bus.psst[29]=0x77088888u;
        REQUIRE(dispatch_midi(e,0xB3u,120u,0u,bus.io(),{})==MidiResult::Applied);
        REQUIRE(bus.csl[29]==0x55012349u);
        REQUIRE(bus.psst[29]==0x77012345u);
        REQUIRE(e.device.voices[29].key_marker==0x0ffeu);
    }
    {
        // A later missing authenticated sample endpoint cannot leave an
        // earlier voice half-released. Check source state BEFORE any OUT.
        MidiEngine e;TracePort bus;
        assign(e,0,6,60);assign(e,3,6,65,0x00fffffcu);
        const auto a=e.device.voices[0].key_marker;
        const auto b=e.device.voices[3].key_marker;
        REQUIRE(dispatch_midi(e,0xB6u,120u,0u,bus.io(),{})==MidiResult::MissingSource);
        REQUIRE(bus.n==0u && e.device.voices[0].key_marker==a &&
                e.device.voices[3].key_marker==b && !e.fault);
    }
    std::puts("P003_ORIGINAL_CC120_CC123_SHARED_RELEASE_PORT_PASS");
}
