#include "OriginalCreativeMidiEngine.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

using namespace awe64::original_vxd;
namespace {
std::atomic<bool> measure{false};
std::atomic<unsigned> allocations{0};
}
void* operator new(std::size_t n) {
    if(measure.load(std::memory_order_relaxed))
        allocations.fetch_add(1,std::memory_order_relaxed);
    void* p=std::malloc(n?n:1u);
    if(!p)throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t n) {
    if(measure.load(std::memory_order_relaxed))
        allocations.fetch_add(1,std::memory_order_relaxed);
    void* p=std::malloc(n?n:1u);
    if(!p)throw std::bad_alloc();
    return p;
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

struct Bus {
    std::uint8_t selector=0;
    std::array<std::uint32_t,32> vtft{};
    unsigned counts=0;
    static bool out8(void* p,std::uint16_t,std::uint8_t v) noexcept {
        auto& b=*static_cast<Bus*>(p);b.selector=v;++b.counts;return true;
    }
    static bool out16(void* p,std::uint16_t,std::uint16_t) noexcept {
        auto& b=*static_cast<Bus*>(p);++b.counts;return true;
    }
    static bool out32(void* p,std::uint16_t port,std::uint32_t x) noexcept {
        auto& b=*static_cast<Bus*>(p);
        if(port==0x620u && (b.selector&0xe0u)==0x60u)
            b.vtft[b.selector&31u]=x;
        ++b.counts;return true;
    }
    static bool in16(void*,std::uint16_t,std::uint16_t& v) noexcept {
        v=0x8000u;return true;
    }
    static bool in32(void* p,std::uint16_t,std::uint32_t& v) noexcept {
        auto& b=*static_cast<Bus*>(p);
        v=(b.selector&0xe0u)==0x60u?b.vtft[b.selector&31u]:0u;
        return true;
    }
    [[nodiscard]] PortIo io() noexcept {
        return {this,out8,out16,in16,in32,out32};
    }
};
struct Source {
    static bool resolve(void*,MidiChannel const&,std::uint8_t,
                        std::uint8_t,std::array<SourceVoice,max_source_note_layers>& voices,
                        std::uint8_t& n) noexcept {
        n=1;
        voices[0].source_verified=true;
        voices[0].descriptor[0x8cu]=1u;
        voices[0].original_pitch_base_verified=true;
        voices[0].original_pitch_base=0xe000u;
        return true;
    }
    static bool compile(void*,SourceVoice const&,std::uint8_t,
                        std::uint8_t,OriginalVoiceProgram& p) noexcept {
        p.source_verified=true;
        p.data[27]=0x7f7fu;
        return true;
    }
    [[nodiscard]] SoundFontSource provider() noexcept {
        return {this,resolve,compile};
    }
};
int main(){
    MidiEngine e{};Bus b{};Source source{};
    for(auto& row:e.device.voices)row.key_marker=0x0ffeu;
    const auto io=b.io();
    const auto provider=source.provider();
    measure.store(true,std::memory_order_relaxed);
    for(unsigned i=0;i<10000u;++i) {
        auto r=dispatch_midi(e,0x90u,60u,100u,io,provider);
        if(r!=MidiResult::Applied)std::abort();
        r=dispatch_midi(e,0xe0u,0u,64u,io,provider);
        if(r!=MidiResult::Applied)std::abort();
        r=dispatch_midi(e,0x80u,60u,64u,io,provider);
        if(r!=MidiResult::Applied)std::abort();
        r=dispatch_midi(e,0xb0u,64u,127u,io,provider);
        if(r!=MidiResult::Applied)std::abort();
        r=dispatch_midi(e,0xb0u,64u,0u,io,provider);
        if(r!=MidiResult::Applied)std::abort();
    }
    measure.store(false,std::memory_order_relaxed);
    if(allocations.load()!=0u){std::fprintf(stderr,"RT_HEAP_ALLOCATIONS %u\n",allocations.load());return 1;}
    std::puts("P003_50000_MIDI_CALLBACKS_WITH_ZERO_HEAP_ALLOCATIONS_PASS");
}
