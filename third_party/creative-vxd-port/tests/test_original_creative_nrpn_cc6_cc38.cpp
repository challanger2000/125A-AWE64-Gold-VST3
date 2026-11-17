#include "OriginalCreativeMidiEngine.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace awe64::original_vxd;
#define REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"NRPN_DATA_ENTRY_FAIL:%d %s\n",__LINE__,#x);std::abort();}}while(false)
namespace {
struct Event {char op;std::uint16_t port,value;};
struct Bus {
    std::vector<Event> event{};
    int failAfter=-1;
    static bool shouldFail(Bus const& b) noexcept {
        return b.failAfter>=0 && static_cast<int>(b.event.size())>=b.failAfter;
    }
    static bool sel(void* opaque,std::uint16_t port,std::uint8_t value) noexcept {
        auto& b=*static_cast<Bus*>(opaque);if(shouldFail(b))return false;
        b.event.push_back({'S',port,value});return true;
    }
    static bool rd(void* opaque,std::uint16_t port,std::uint16_t& value) noexcept {
        auto& b=*static_cast<Bus*>(opaque);if(shouldFail(b))return false;
        value=0xabcdu;b.event.push_back({'R',port,value});return true;
    }
    static bool wr(void* opaque,std::uint16_t port,std::uint16_t value) noexcept {
        auto& b=*static_cast<Bus*>(opaque);if(shouldFail(b))return false;
        b.event.push_back({'W',port,value});return true;
    }
};
void w16(std::vector<std::uint8_t>& d,std::size_t p,unsigned v) {
    d[p]=static_cast<std::uint8_t>(v);
    d[p+1u]=static_cast<std::uint8_t>(v>>8u);
}
std::uint16_t r16(std::vector<std::uint8_t> const& d,std::size_t p) {
    return static_cast<std::uint16_t>(static_cast<unsigned>(d[p])|
           (static_cast<unsigned>(d[p+1u])<<8u));
}
}
int main() {
    constexpr std::uint8_t ch=3u;
    constexpr auto channel=0x24u*ch;
    constexpr auto row=0x0f5eu+0x5cu*ch;
    std::vector<std::uint8_t> d(0x1592u,0u);
    std::array<std::uint8_t,180u> parameters{}; // SYNTHETIC
    std::array<std::uint16_t,128u> lookup{}; // SYNTHETIC
    parameters[6u]=9u;parameters[7u]=1u;parameters[8u]=0x60u;
    parameters[9u]=0x04u;
    parameters[0u]=4u;parameters[1u]=1u;
    w16(d,4u,0xe20u);w16(d,6u,2u);
    w16(d,0x42u,0x0300u);w16(d,0x62u,0x0301u);
    w16(d,0x42u+0x0eu,300u);w16(d,0x62u+0x0eu,301u);
    w16(d,0x442u+channel+0x14u,5000u);
    MidiEngine e{};
    e.device.physical_capacity=2u;
    // Native allocator and the historical VXD snapshot MUST select
    // the very same physical voices, cached pitch bases and channel bend.
    e.device.voices[0].key_marker=0x0300u;
    e.device.voices[1].key_marker=0x0301u;
    e.device.voices[0].original_pitch_base=300u;
    e.device.voices[1].original_pitch_base=301u;
    e.device.voices[0].original_pitch_base_verified=true;
    e.device.voices[1].original_pitch_base_verified=true;
    e.channel[ch].original_bend_delta=5000;
    e.original_nrpn_memory={d,true};
    e.original_nrpn_callbacks_installed_verified=true;
    e.original_nrpn_parameter_callback_installed_verified=true;
    e.original_nrpn_parameter_tables_verified=true;
    e.original_nrpn_voice_rows_verified=true;
    e.original_nrpn_parameter_entries=parameters;
    e.original_nrpn_parameter_lookup=lookup;
    Bus bus;
    const PortIo io{&bus,&Bus::sel,&Bus::wr,&Bus::rd,nullptr,nullptr};
    const SoundFontSource sf2{};
    const auto cc=[&](unsigned n,unsigned v)->MidiResult {
        return dispatch_midi(e,static_cast<std::uint8_t>(0xb0u|ch),
            static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(v),
            io,sf2);
    };
    REQUIRE(cc(99u,127u)==MidiResult::Applied);
    REQUIRE(cc(98u,1u)==MidiResult::Applied);
    REQUIRE(r16(d,0x464u+channel)==0x2000u);
    // Missing attestation and mismatched physical capacity are fail-closed.
    e.original_nrpn_parameter_tables_verified=false;
    REQUIRE(cc(6u,65u)==MidiResult::MissingSource);
    REQUIRE(bus.event.empty());
    REQUIRE(r16(d,0x464u+channel)==0x2000u);
    e.original_nrpn_parameter_tables_verified=true;
    e.device.physical_capacity=1u;
    REQUIRE(cc(38u,32u)==MidiResult::MissingSource);
    e.device.physical_capacity=2u;
    // An original VXD snapshot becomes stale as soon as the live
    // native allocator changes voice ownership. Fail BEFORE touching
    // the source parameter word, source state, or actual chip ports.
    e.device.voices[0].key_marker=0x0302u;
    REQUIRE(cc(6u,65u)==MidiResult::MissingSource);
    REQUIRE(r16(d,0x464u+channel)==0x2000u);
    REQUIRE(bus.event.empty());
    e.device.voices[0].key_marker=0x0300u;
    e.device.voices[1].original_pitch_base=777u;
    REQUIRE(cc(6u,65u)==MidiResult::MissingSource);
    REQUIRE(bus.event.empty());
    e.device.voices[1].original_pitch_base=301u;
    // Real CC6 -> B52 -> LCOD+0x70E5, two original voice RMW pairs.
    REQUIRE(cc(6u,65u)==MidiResult::Applied);
    REQUIRE(r16(d,0x464u+channel)==0x2080u);
    REQUIRE(r16(d,row+4u)==0x2080u);
    REQUIRE(bus.event.size()==8u);
    for(std::size_t k=0u;k<8u;k+=4u) {
        REQUIRE(bus.event[k].op=='S' && bus.event[k].port==0xe22u);
        REQUIRE(bus.event[k].value==0x60u+k/4u);
        REQUIRE(bus.event[k+1u].op=='R'&&bus.event[k+1u].port==0xe20u);
        REQUIRE(bus.event[k+2u].op=='S');
        REQUIRE(bus.event[k+3u].op=='W' && bus.event[k+3u].port==0xe20u);
        REQUIRE(bus.event[k+3u].value==0xab10u);
    }
    // CC38 preserves top 9 bits and merges the low 7 data bits.
    bus.event.clear();
    REQUIRE(cc(38u,32u)==MidiResult::Applied);
    REQUIRE(r16(d,0x464u+channel)==0x20a0u);
    REQUIRE(bus.event.size()==8u && bus.event[3u].value==0xab14u);
    REQUIRE(bus.event[7u].value==0xab14u);
    // Original code4 parameter uses DIRECT 16-bit OUT (no read).
    bus.event.clear();
    REQUIRE(cc(98u,0u)==MidiResult::Applied);
    // Stale channel bend or unverified active voice pitch blocks this
    // callback, even when the 30x6 lookup table was source-attested.
    e.channel[ch].original_bend_delta=4999;
    REQUIRE(cc(6u,65u)==MidiResult::MissingSource);
    REQUIRE(bus.event.empty());
    e.channel[ch].original_bend_delta=5000;
    e.device.voices[0].original_pitch_base_verified=false;
    REQUIRE(cc(6u,65u)==MidiResult::MissingSource);
    REQUIRE(bus.event.empty());
    e.device.voices[0].original_pitch_base_verified=true;
    REQUIRE(cc(6u,65u)==MidiResult::Applied);
    REQUIRE(bus.event.size()==4u);
    REQUIRE(bus.event[0u].op=='S' && bus.event[0u].value==0u);
    REQUIRE(bus.event[1u].op=='W' && bus.event[1u].value==5736u);
    REQUIRE(bus.event[2u].op=='S' && bus.event[2u].value==1u);
    REQUIRE(bus.event[3u].op=='W' && bus.event[3u].value==5737u);
    // Invalid parameter still updates source+0x464; EAX=1 is not I/O error.
    bus.event.clear();
    REQUIRE(cc(98u,30u)==MidiResult::Applied);
    REQUIRE(cc(38u,12u)==MidiResult::Applied);
    REQUIRE(bus.event.empty());
    REQUIRE(r16(d,0x464u+channel)==0x208cu);
    // Simulate failure during first voice OUT16: latch fault, no rollback.
    REQUIRE(cc(98u,1u)==MidiResult::Applied);
    bus.event.clear();bus.failAfter=3;
    REQUIRE(cc(6u,65u)==MidiResult::PortError);
    REQUIRE(e.fault && bus.event.size()==3u);
    REQUIRE(r16(d,0x464u+channel)==0x20a0u);
    REQUIRE(cc(38u,4u)==MidiResult::FaultLatched);
    std::puts("P003_CC6_CC38_ORIGINAL_B52_CALLBACK_CHIP_IO_PASS");
}
