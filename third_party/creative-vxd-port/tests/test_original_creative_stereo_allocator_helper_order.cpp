#include "OriginalCreativeVxdLive.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace awe64::original_vxd;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}} while(false)

// Native data is SYNTHETIC: it does not replay any captured register values,
// CPU timestamps, or original per-voice trace. Real V4 comparison is private.
struct Chip {
    std::uint8_t selector=0;
    std::array<std::uint32_t,32> vtft{},ccca{};
    std::array<std::uint16_t,32> sus{};
    std::array<StereoHelperEvent,128> events{};
    std::size_t size=0;
    bool fail_in32=false;
    static void observe(void* opaque,StereoHelperEvent e) noexcept {
        auto& c=*static_cast<Chip*>(opaque);
        if(c.size<c.events.size()) c.events[c.size++]=e;
    }
    static bool out8(void* ctx,std::uint16_t port,std::uint8_t v) noexcept {
        auto& c=*static_cast<Chip*>(ctx);if(port!=0x0e22u)return false;c.selector=v;return true;
    }
    static bool out16(void* ctx,std::uint16_t port,std::uint16_t value) noexcept {
        auto& c=*static_cast<Chip*>(ctx);
        return port==0x0a20u && (c.selector&0xe0u)==0xa0u && value==0xffu;
    }
    static bool in16(void* ctx,std::uint16_t port,std::uint16_t& v) noexcept {
        auto& c=*static_cast<Chip*>(ctx);
        if(port!=0x0a20u || (c.selector&0xe0u)!=0xa0u)return false;
        v=c.sus[c.selector&31u];return true;
    }
    static bool in32(void* ctx,std::uint16_t port,std::uint32_t& v) noexcept {
        auto& c=*static_cast<Chip*>(ctx);const auto reg=c.selector&0xe0u;
        if(c.fail_in32)return false;
        if(port==0x0620u && reg==0x60u)v=c.vtft[c.selector&31u];
        else if(port==0x0a20u && reg==0u)v=c.ccca[c.selector&31u];
        else return false;
        return true;
    }
    PortIo io() noexcept {
        PortIo p{this,out8,out16,in16,in32,nullptr};
        p.stereo_helper_opaque=this;p.stereo_helper_observer=observe;
        return p;
    }
};
int main(int argc,char** argv){
    DeviceState state;state.physical_capacity=32u;state.mode_count=32u;
    Chip chip;chip.sus.fill(0x8000u);chip.vtft.fill(0x03000000u);
    for(unsigned i=0;i<32u;++i){
        state.voices[i].key_marker=0x0101u;
        state.voices[i].sample_end=0x1000u;
    }
    for(const auto i:{31u,28u,25u}){
        state.voices[i].key_marker=0x0ffeu;
        state.voices[i].sample_end=0u;
    }
    chip.vtft[9]=0u;chip.vtft[10]=0u;
    const auto result=allocate_pair_mode1(state,chip.io());
    CHECK(result.status==Status::Found && !result.immediate);
    CHECK(result.first==9u && result.second==10u);
    CHECK(result.physical_port_callbacks()==245u);
    CHECK(result.vtft_reads==32u && result.dcysusv_reads==29u && result.ccca_reads==29u);
    CHECK(chip.size==92u);
    std::size_t i=0;
    for(unsigned voice=32u;voice>0u;){
        --voice;
        CHECK(chip.events[i].kind==StereoHelperKind::Vtft && chip.events[i].voice==voice);++i;
        if(voice==31u || voice==28u || voice==25u)continue;
        CHECK(chip.events[i].kind==StereoHelperKind::Dcysusv && chip.events[i].voice==voice);++i;
        CHECK(chip.events[i].kind==StereoHelperKind::Ccca && chip.events[i].voice==voice);++i;
    }
    CHECK(i==90u);
    CHECK(chip.events[i].kind==StereoHelperKind::Release && chip.events[i].voice==9u);++i;
    CHECK(chip.events[i].kind==StereoHelperKind::Release && chip.events[i].voice==10u);++i;
    CHECK(i==chip.size);
    Chip failed;failed.fail_in32=true;
    const auto error=allocate_pair_mode1(state,failed.io());
    CHECK(error.status==Status::PortError && failed.size==0u);
    if(argc==2 && std::strcmp(argv[1],"--helper-path")==0) {
        for(std::size_t k=0;k<chip.size;++k)
            std::printf("%u %u\n",static_cast<unsigned>(chip.events[k].kind),
                        static_cast<unsigned>(chip.events[k].voice));
    }else std::puts("P003_NATIVE_STEREO_HELPER_ORDER_PASS 92 calls / 245 physical callbacks");
}
