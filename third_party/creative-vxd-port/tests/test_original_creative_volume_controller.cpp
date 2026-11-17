#include "OriginalCreativeVolumeController.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(v) do{if(!(v)){std::fprintf(stderr,"VOLUME_FAIL %d %s\n",__LINE__,#v);std::exit(1);}}while(false)
struct Bus {
    std::uint8_t selected=0;
    std::array<std::uint16_t,32> ifatn{};
    unsigned out8_count=0,out16_count=0,in16_count=0;
    bool fail=false;
    static bool out8(void* p,std::uint16_t port,std::uint8_t value) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(b.fail || port!=0xe22u)return false;
        b.selected=value;++b.out8_count;return true;
    }
    static bool out16(void* p,std::uint16_t port,std::uint16_t value) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(b.fail || port!=0xe20u || (b.selected&0xe0u)!=0x20u)return false;
        b.ifatn[b.selected&31u]=value;++b.out16_count;return true;
    }
    static bool in16(void* p,std::uint16_t port,std::uint16_t& value) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(b.fail || port!=0xe20u || (b.selected&0xe0u)!=0x20u)return false;
        value=b.ifatn[b.selected&31u];++b.in16_count;return true;
    }
    PortIo io() noexcept{return {this,out8,out16,in16,nullptr,nullptr};}
};
int main() {
    MidiEngine e{};
    Bus b{};
    b.ifatn[30]=0xb40cu;
    auto& voice=e.device.voices[30];
    voice.key_marker=static_cast<std::uint16_t>((3u<<8u)|60u);
    voice.volume_terms_verified=true;
    voice.original_velocity_volume_level=10u;
    voice.original_amplitude_word=24u;
    std::array<std::uint8_t,128> vol{},expr{};
    vol.fill(0u);expr.fill(0u);
    e.volume_source.verified=true;
    e.volume_source.cc7_table_128=vol.data();
    e.volume_source.cc11_table_128=expr.data();
    e.volume_source.device_additive=10u;
    e.volume_source.device_additive_verified=true;
    e.volume_source.format1_verified=true;
    // No hardcoded 30-music-voice limit: physical30 is original stereo.
    CHECK(dispatch_midi(e,0xb3u,7u,64u,b.io(),{})==MidiResult::Applied);
    CHECK(e.channel[3].volume==64u && b.ifatn[30]==0xb427u);
    CHECK(b.out8_count==2u && b.in16_count==1u && b.out16_count==1u);
    // CC11 table value enters the ORIGINAL pre-scaling sum:
    // (10+32+(24+12)/24)*8/3 + 10 = 124 (0x7c).
    expr[64]=32u;
    CHECK(dispatch_midi(e,0xb3u,11u,64u,b.io(),{})==MidiResult::Applied);
    CHECK(e.channel[3].expression==64u && b.ifatn[30]==0xb47cu);
    // Original ROM/1MGM +16 adjustment uses the voice+0x17 flag.
    e.volume_source.format1=true;
    voice.blocked=true;
    CHECK(dispatch_midi(e,0xb3u,7u,65u,b.io(),{})==MidiResult::Applied);
    CHECK(b.ifatn[30]==0xb48cu);
    // CC7 does NOT trigger the +0x648E CC11 gate. The volume table
    // scales the *remainder*: 124 + ((256-124)*64 >> 7) + 16 = 206.
    vol[10]=64u;
    CHECK(dispatch_midi(e,0xb3u,7u,10u,b.io(),{})==MidiResult::Applied);
    CHECK(b.ifatn[30]==0xb4ceu);
    // CC11 <=10 DOES jump to +0x652A: hard attenuation 0xff.
    CHECK(dispatch_midi(e,0xb3u,11u,10u,b.io(),{})==MidiResult::Applied);
    CHECK(b.ifatn[30]==0xb4ffu);
    const auto before=b.out16_count;
    // A missing original per-voice lookup must not perform partial writes.
    voice.volume_terms_verified=false;
    CHECK(dispatch_midi(e,0xb3u,7u,50u,b.io(),{})==MidiResult::MissingSource);
    CHECK(e.channel[3].volume==10u && b.out16_count==before);
    voice.volume_terms_verified=true;
    // An unknown original VXD table never silently uses a modern curve.
    e.volume_source.verified=false;
    CHECK(dispatch_midi(e,0xb3u,11u,70u,b.io(),{})==MidiResult::Unsupported);
    CHECK(e.channel[3].expression==70u && b.out16_count==before);
    e.volume_source.verified=true;
    // Corrupt imported channel data must not index beyond either table
    // when the OTHER controller is updated. No channel/hardware mutation.
    const auto savedVolume=e.channel[3].volume;
    e.channel[3].volume=128u;
    CHECK(dispatch_midi(e,0xb3u,11u,71u,b.io(),{})==MidiResult::Invalid);
    CHECK(e.channel[3].expression==70u && b.out16_count==before);
    e.channel[3].volume=savedVolume;
    e.channel[3].expression=128u;
    CHECK(dispatch_midi(e,0xb3u,7u,71u,b.io(),{})==MidiResult::Invalid);
    CHECK(e.channel[3].volume==savedVolume && b.out16_count==before);
    e.channel[3].expression=70u;
    b.fail=true;
    CHECK(dispatch_midi(e,0xb3u,11u,71u,b.io(),{})==MidiResult::PortError);
    CHECK(e.fault && dispatch_midi(e,0xb3u,11u,72u,b.io(),{})==MidiResult::FaultLatched);
    std::puts("P003_ORIGINAL_LIVE_CC7_CC11_FILTER_PRESERVE_PASS");
}
