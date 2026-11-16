#include "OriginalCreativePanController.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"PAN_FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(false)
struct Bus {
    std::uint8_t index=0;
    std::array<std::uint32_t,32> psst{},ptrx{};
    unsigned calls=0, write_psst=0, write_ptrx=0;
    bool fail=false;
    static bool out8(void* p,std::uint16_t port,std::uint8_t v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        ++b.calls;
        if(b.fail || port!=0xe22u)return false;
        b.index=v;return true;
    }
    static bool out16(void*,std::uint16_t,std::uint16_t) noexcept{return false;}
    static bool in16(void*,std::uint16_t,std::uint16_t&) noexcept{return false;}
    static bool in32(void* p,std::uint16_t port,std::uint32_t& val) noexcept {
        auto& b=*static_cast<Bus*>(p);
        ++b.calls;
        if(b.fail || port!=0x620u)return false;
        const auto voice=b.index&31u;
        if((b.index&0xe0u)==0xc0u)val=b.psst[voice];
        else if((b.index&0xe0u)==0x20u)val=b.ptrx[voice];
        else return false;
        return true;
    }
    static bool out32(void* p,std::uint16_t port,std::uint32_t val) noexcept {
        auto& b=*static_cast<Bus*>(p);
        ++b.calls;
        if(b.fail || port!=0x620u)return false;
        const auto voice=b.index&31u;
        if((b.index&0xe0u)==0xc0u){b.psst[voice]=val;++b.write_psst;}
        else if((b.index&0xe0u)==0x20u){b.ptrx[voice]=val;++b.write_ptrx;}
        else return false;
        return true;
    }
    PortIo io() noexcept{return {this,out8,out16,in16,in32,out32};}
};
int main() {
    MidiEngine e{};Bus b{};
    e.device.original_pan_device_mode_verified=true;
    e.device.original_pan_device_mode=0;
    auto& v=e.device.voices[1];
    v.key_marker=static_cast<std::uint16_t>((2u<<8u)|60u);
    v.pan_state_verified=true;
    v.original_pan_base=16u;
    v.pan_current=200u;v.pan_target=200u;
    b.psst[1]=0x55001234u;
    b.ptrx[1]=0x12345678u;
    // Original CC10+0x68F3: new target=383-2*(16+70)=211;
    // +0x6980: one step from current200 toward211, NOT instantaneous.
    CHECK(dispatch_midi(e,0xb2u,10u,70u,b.io(),{})==MidiResult::Incomplete);
    CHECK(e.channel[2].pan==70u && v.pan_target==211u && v.pan_current==201u);
    CHECK(b.psst[1]==0xc9001234u && b.ptrx[1]==0x12345637u);
    CHECK(b.write_psst==1u && b.write_ptrx==1u && b.calls==8u);
    // CC10 same value is skipped in the original; it does not silently
    // turn into a guessed high-frequency timer tick.
    CHECK(dispatch_midi(e,0xb2u,10u,70u,b.io(),{})==MidiResult::Applied);
    CHECK(b.calls==8u);
    for(unsigned i=0;i<9;++i)
        CHECK(service_original_pan_voice_step(e,1u,b.io())==MidiResult::Incomplete);
    CHECK(service_original_pan_voice_step(e,1u,b.io())==MidiResult::Applied);
    CHECK(v.pan_current==211u && b.psst[1]==0xd3001234u);
    CHECK(b.ptrx[1]==0x1234562du); // -211 modulo 256 = 45
    CHECK(b.write_psst==11u && b.write_ptrx==11u);
    const unsigned old_calls=b.calls;
    CHECK(service_original_pan_voice_step(e,1u,b.io())==MidiResult::Applied);
    CHECK(b.calls==old_calls);
    // Original high/low saturation and special zero -> low PTRX = FF.
    v.original_pan_base=64u;
    v.pan_current=1u;v.pan_target=1u;
    e.channel[2].pan=70u;
    CHECK(dispatch_midi(e,0xb2u,10u,127u,b.io(),{})==MidiResult::Applied);
    CHECK(v.pan_current==0u && v.pan_target==0u &&
          (b.ptrx[1]&0xffu)==0xffu);
    // When +0x12 original device mode equals 1, CC10 writes state only.
    e.device.original_pan_device_mode=1u;
    v.pan_current=50u;
    const unsigned before=b.calls;
    CHECK(dispatch_midi(e,0xb2u,10u,64u,b.io(),{})==MidiResult::Applied);
    CHECK(v.pan_current==50u && b.calls==before);
    e.device.original_pan_device_mode=0u;
    v.pan_state_verified=false;
    CHECK(dispatch_midi(e,0xb2u,10u,63u,b.io(),{})==MidiResult::MissingSource);
    CHECK(e.channel[2].pan==64u && b.calls==before);
    v.pan_state_verified=true;
    b.fail=true;
    CHECK(dispatch_midi(e,0xb2u,10u,63u,b.io(),{})==MidiResult::PortError);
    CHECK(e.fault);
    CHECK(dispatch_midi(e,0xb2u,10u,62u,b.io(),{})==MidiResult::FaultLatched);
    std::puts("P003_ORIGINAL_CC10_PAN_PREDICTOR_ONE_STEP_PXPTRX_PASSED");
}
