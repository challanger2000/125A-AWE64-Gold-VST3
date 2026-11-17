// Full normalized original 288-slot bank directory -> MIDI C0 -> verified
// 148-byte SF2 descriptor -> live voice selector -> real typed port writes,
// repeated without any new/new[] during MIDI callbacks.
// Synthetic source state, not an original Windows95/PCM-parity claim.
#include "OriginalCreativeVerifiedSf2Bank.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
using namespace awe64::original_vxd;
namespace {std::atomic<bool> measuring=false;std::atomic<unsigned> allocations=0;}
void* operator new(std::size_t n) {
    if(measuring.load(std::memory_order_relaxed))allocations.fetch_add(1,std::memory_order_relaxed);
    void* p=std::malloc(n?n:1u);if(!p)throw std::bad_alloc();return p;
}
void* operator new[](std::size_t n) {
    if(measuring.load(std::memory_order_relaxed))allocations.fetch_add(1,std::memory_order_relaxed);
    void* p=std::malloc(n?n:1u);if(!p)throw std::bad_alloc();return p;
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
struct Bus {
    std::uint8_t selector=0u;
    std::array<std::uint32_t,32> vtft{};
    unsigned writes=0;
    static bool out8(void* p,std::uint16_t,std::uint8_t x) noexcept {
        auto& v=*static_cast<Bus*>(p);v.selector=x;++v.writes;return true;
    }
    static bool out16(void* p,std::uint16_t,std::uint16_t) noexcept {
        ++static_cast<Bus*>(p)->writes;return true;
    }
    static bool out32(void* p,std::uint16_t port,std::uint32_t val) noexcept {
        auto& v=*static_cast<Bus*>(p);
        if(port==0x620u && (v.selector&0xe0u)==0x60u)
            v.vtft[v.selector&31u]=val;
        ++v.writes;return true;
    }
    static bool in16(void*,std::uint16_t,std::uint16_t& x) noexcept {x=0x8000u;return true;}
    static bool in32(void* p,std::uint16_t,std::uint32_t& x) noexcept {
        auto& v=*static_cast<Bus*>(p);
        x=(v.selector&0xe0u)==0x60u?v.vtft[v.selector&31u]:0u;
        return true;
    }
    PortIo io() noexcept{return {this,out8,out16,in16,in32,out32};}
};
static void word(Descriptor& d,unsigned at,std::uint16_t x) noexcept {
    d[at]=static_cast<std::uint8_t>(x);d[at+1u]=static_cast<std::uint8_t>(x>>8u);
}
static void dword(Descriptor& d,unsigned at,std::uint32_t x) noexcept {
    for(unsigned i=0;i<4;++i)d[at+i]=static_cast<std::uint8_t>(x>>(i*8u));
}
int main(){
    VerifiedSf2Zone zone{};
    zone.original_zone_verified=true;zone.bank=128u;zone.program=7u;
    zone.original_preset_identity_verified=true;
    zone.original_bank_slot=130u;zone.original_preset_row=0u;
    zone.key_low=60u;zone.key_high=60u;zone.velocity_low=1u;zone.velocity_high=127u;
    zone.voice.source_verified=true;
    zone.voice.original_pitch_base_verified=true;zone.voice.original_pitch_base=0xe000u;
    zone.voice.note_start_descriptor_verified=true;
    auto& d=zone.voice.note_start_descriptor;
    word(d,0x8cu,1u);word(d,54u*2u,1u);word(d,8u*2u,180u);
    word(d,9u*2u,7u);word(d,19u*2u,20u);word(d,17u*2u,64u);
    word(d,18u*2u,128u);word(d,26u*2u,128u);word(d,27u*2u,127u);
    word(d,28u*2u,4u);word(d,29u*2u,127u);word(d,30u*2u,12u);
    word(d,33u*2u,0x8000u);word(d,34u*2u,128u);word(d,35u*2u,127u);
    word(d,36u*2u,63u);word(d,37u*2u,5u);word(d,38u*2u,52u);
    word(d,21u*2u,0x8000u);word(d,23u*2u,0x8000u);
    word(d,22u*2u,205u);word(d,24u*2u,205u);
    word(d,56u*2u,100u);word(d,58u*2u,0xffffu);d[0x8au]=60u;
    dword(d,0x86u,0xe00000u);
    dword(d,0x76u,0x1000u);dword(d,0x7au,0x2000u);
    dword(d,0x7eu,0x1100u);dword(d,0x82u,0x1f00u);
    zone.voice.descriptor=d;word(zone.voice.descriptor,0x6au,20u);
    zone.original_state.original_descriptor_verified=true;
    zone.original_state.original_device_state_verified=true;
    zone.original_state.original_sample_placement_verified=true;
    zone.original_state.original_pitch_target=0x8000u;
    std::array<VerifiedSf2Zone,1> zones{zone};
    std::array<OriginalProgramBankSlot,original_program_bank_slots> slots{};
    const std::array<OriginalPresetTableRow,2> bank128_rows{{
        {1u,7u,128u},{0xffffu,0u,0u}}};
    slots[130].rows=bank128_rows;
    VerifiedSf2Bank bank{zones};bank.original_program_map={true,slots};
    auto provider=original_verified_sf2_bank_source(bank);
    MidiEngine eng{};Bus bus{};
    auto io=bus.io();
    measuring.store(true,std::memory_order_relaxed);
    for(unsigned i=0;i<5000u;++i){
        if(dispatch_midi(eng,0xb0u,0u,2u,io,provider)!=MidiResult::Applied)std::abort();
        if(dispatch_midi(eng,0xc0u,7u,0u,io,provider)!=MidiResult::Applied)std::abort();
        if(dispatch_midi(eng,0x90u,60u,100u,io,provider)!=MidiResult::Applied)std::abort();
        if(dispatch_midi(eng,0x80u,60u,0u,io,provider)!=MidiResult::Applied)std::abort();
        // Missing C0 invalidates selection and must NOT touch the chip.
        if(dispatch_midi(eng,0xc0u,9u,0u,io,provider)!=MidiResult::MissingSource)std::abort();
        if(dispatch_midi(eng,0x90u,60u,100u,io,provider)!=MidiResult::MissingSource)std::abort();
    }
    measuring.store(false,std::memory_order_relaxed);
    if(allocations.load()!=0u) {
        std::fprintf(stderr,"BANKED_MIDI_HEAP_ALLOCATIONS=%u\n",allocations.load());return 1;
    }
    if(bus.writes==0u || eng.fault) return 2;
    std::puts("P003_VERIFIED_288_SLOT_BANK_30000_EVENTS_ZERO_HEAP_PASS");
}
