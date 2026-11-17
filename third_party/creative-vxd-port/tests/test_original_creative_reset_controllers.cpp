#include "OriginalCreativeMidiEngine.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"CC121_TEST_FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
struct Bus {
    struct Event {char op;std::uint16_t port;std::uint32_t val;std::uint8_t sel;};
    std::array<Event,128> ev{};std::size_t n=0;
    std::array<std::uint16_t,32> ifatn{},fmmod{};
    std::uint8_t sel=0;
    bool emit(char t,std::uint16_t port,std::uint32_t val) noexcept {
        if(n==ev.size())return false;ev[n++]={t,port,val,sel};return true;
    }
    static bool out8(void* p,std::uint16_t port,std::uint8_t v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(port!=0xe22u)return false;
        b.sel=v;return b.emit('b',port,v);
    }
    static bool out16(void* p,std::uint16_t port,std::uint16_t v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(port==0xe20u) {
            if((b.sel&0xe0u)==0x20u)b.ifatn[b.sel&31u]=v;
            if((b.sel&0xe0u)==0x60u)b.fmmod[b.sel&31u]=v;
        }else if(port!=0xa20u)return false;
        return b.emit('w',port,v);
    }
    static bool in16(void* p,std::uint16_t port,std::uint16_t& v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(port!=0xe20u)return false;
        if((b.sel&0xe0u)==0x20u)v=b.ifatn[b.sel&31u];
        else if((b.sel&0xe0u)==0x60u)v=b.fmmod[b.sel&31u];
        else return false;
        return b.emit('r',port,v);
    }
    static bool in32(void*,std::uint16_t,std::uint32_t&) noexcept {return false;}
    static bool out32(void*,std::uint16_t,std::uint32_t) noexcept {return false;}
    PortIo io() noexcept {return {this,out8,out16,in16,in32,out32};}
};

int main(){
    {
        // The original device+0xB5A callback is optional, NOT a
        // generically safe no-op. Missing verification must fail closed.
        MidiEngine e{};Bus b;
        auto& c=e.channel[0];c.expression=24;c.modulation=40;
        CHECK(dispatch_midi(e,0xb0u,121u,0u,b.io(),{})==MidiResult::MissingSource);
        CHECK(c.expression==24u && c.modulation==40u && b.n==0u);
        e.original_reset_callback_absent_verified=true;
        c.volume=43;c.pitch_bend=12000;
        c.original_rpn_mode=0x100u;c.original_rpn_number=1u;
        c.original_rpn_aux_462=10u;
        CHECK(dispatch_midi(e,0xb0u,121u,0u,b.io(),{})==MidiResult::Applied);
        CHECK(c.volume==43u && c.expression==127u && c.modulation==0u);
        CHECK(c.pitch_bend==8192u && c.original_bend_delta==0);
        CHECK(c.original_rpn_mode==0u && c.original_rpn_number==0u &&
              c.original_rpn_aux_462==0u);
        CHECK(b.n==0u);
    }
    {
        MidiEngine e{};Bus b;
        e.original_reset_callback_absent_verified=true;
        auto& c=e.channel[3];
        c.volume=70;c.expression=88;c.modulation=121;
        c.original_pitch_tune=0;
        c.original_rpn_mode=0x100u;c.original_rpn_number=2u;
        c.original_modulation_byte_44d=13u;
        c.original_modulation_byte_44d_verified=true;
        auto& v=e.device.voices[5];
        v.key_marker=0x033eu;
        v.original_pitch_base=0xe000u;v.original_pitch_base_verified=true;
        v.volume_terms_verified=true;
        v.original_velocity_volume_level=9u;
        v.original_modulation_word_verified=true;
        v.original_modulation_word_signed=12;
        static constexpr std::array<std::uint8_t,128> zeros{};
        e.volume_source={true,zeros.data(),zeros.data(),0u,true,true,false};
        b.ifatn[5]=0x1200u;b.fmmod[5]=0x0011u;
        CHECK(dispatch_midi(e,0xb3u,121u,0u,b.io(),{})==MidiResult::Applied);
        CHECK(c.expression==127u && c.volume==70u && c.modulation==0u);
        CHECK(c.original_modulation_byte_44d==0u && c.original_modulation_byte_44d_verified);
        CHECK(c.original_rpn_mode==0u && c.original_rpn_number==0u);
        CHECK(!e.fault);
        // The order is CC11 live IFATN, pitch center IP, and CC1 PEFE
        // modulation, exactly as CC121 calls +0x640A,+0x6F56,+0x6F1E.
        CHECK(b.n==10u);
        CHECK(b.ev[0].sel==0x25u && b.ev[1].op=='r' &&
              b.ev[3].port==0xe20u && b.ev[3].op=='w');
        CHECK(b.ev[4].sel==0x05u && b.ev[5].port==0xe20u &&
              b.ev[5].val==0xe000u);
        CHECK(b.ev[6].sel==0x65u && b.ev[7].op=='r' &&
              b.ev[9].port==0xe20u && b.ev[9].op=='w');
        CHECK(b.fmmod[5]==0x0c11u);
    }
    {
        // Missing live volume input must reject CC121 BEFORE any reset
        // state mutation. This is deliberate fail-closed behavior.
        MidiEngine e{};Bus b;e.original_reset_callback_absent_verified=true;
        auto& v=e.device.voices[1];v.key_marker=0x003cu;
        v.original_pitch_base_verified=true;
        v.original_modulation_word_verified=true;
        v.volume_terms_verified=false;
        e.channel[0].expression=8u;
        CHECK(dispatch_midi(e,0xb0u,121u,0u,b.io(),{})==MidiResult::MissingSource);
        CHECK(e.channel[0].expression==8u && v.key_marker==0x003cu && b.n==0u);
    }
    std::puts("P003_ORIGINAL_CC121_RESET_CONTROLLERS_PORT_PASS");
}
