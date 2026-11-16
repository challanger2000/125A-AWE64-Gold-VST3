#include "OriginalCreativeVxdLive.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
using namespace awe64::original_vxd;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}} while(false)

// This tests the *native* source decision trace, not reconstructed x86 timing.
// The fixture contains no original recorded data or time stamps.
struct Chip {
    std::uint8_t selector=0;
    bool reject_first_read=false;
    std::array<std::uint32_t,32> vtft{},ccca{};
    std::array<std::uint16_t,32> sus{};
    std::array<StereoDecisionEvent,128> decisions{};
    std::size_t count=0;
    static void on_decision(void* opaque,StereoDecisionEvent e) noexcept {
        auto& c=*static_cast<Chip*>(opaque);
        if(c.count<c.decisions.size())c.decisions[c.count++]=e;
    }
    static bool out8(void* ctx,std::uint16_t p,std::uint8_t v) noexcept {
        auto& c=*static_cast<Chip*>(ctx);
        if(p!=0x0e22u)return false;c.selector=v;return true;
    }
    static bool out16(void* ctx,std::uint16_t p,std::uint16_t v) noexcept {
        auto& c=*static_cast<Chip*>(ctx);
        return p==0x0a20u && (c.selector&0xe0u)==0xa0u && v==0xffu;
    }
    static bool in16(void* ctx,std::uint16_t p,std::uint16_t& v) noexcept {
        auto& c=*static_cast<Chip*>(ctx);
        if(p!=0x0a20u || (c.selector&0xe0u)!=0xa0u)return false;
        v=c.sus[c.selector&31u];return true;
    }
    static bool in32(void* ctx,std::uint16_t p,std::uint32_t& v) noexcept {
        auto& c=*static_cast<Chip*>(ctx);
        if(c.reject_first_read)return false;
        if(p==0x0620u && (c.selector&0xe0u)==0x60u) v=c.vtft[c.selector&31u];
        else if(p==0x0a20u && (c.selector&0xe0u)==0u) v=c.ccca[c.selector&31u];
        else return false;
        return true;
    }
    PortIo io() noexcept {
        PortIo p{this,out8,out16,in16,in32,nullptr};
        p.stereo_decision_opaque=this;p.stereo_decision_observer=on_decision;return p;
    }
};
static_assert(std::is_trivially_copyable_v<StereoDecisionEvent>);
int main(){
    DeviceState state;
    Chip c;
    c.sus.fill(0x8000u);
    c.vtft.fill(0x03000000u);
    for(unsigned i=0;i<32;++i){
        state.voices[i].key_marker=0x0101u;
        state.voices[i].sample_end=0x1000u;
    }
    for(unsigned i:{31u,28u,25u}) {
        state.voices[i].key_marker=0x0ffeu;
        state.voices[i].sample_end=0u;
    }
    c.vtft[9]=0u;c.vtft[10]=0u;
    const auto outcome=allocate_pair_mode1(state,c.io());
    CHECK(outcome.status==Status::Found && outcome.first==9 && outcome.second==10);
    CHECK(outcome.physical_port_callbacks()==245);
    CHECK(c.count==96); // 32 rows: scored + cursor status + priority decision.
    std::size_t at=0;
    unsigned replacements=0;
    for(unsigned n=32;n>0;){
        const auto voice=static_cast<std::uint8_t>(--n);
        CHECK(c.decisions[at].kind==StereoDecisionKind::ScoredVoice);
        CHECK(c.decisions[at++].voice==voice);
        const auto expected=(n==31u || n==28u || n==25u) ?
            StereoDecisionKind::CursorNotRead : StereoDecisionKind::CursorReadNotFinished;
        CHECK(c.decisions[at].kind==expected);
        CHECK(c.decisions[at++].voice==voice);
        const auto priority=c.decisions[at];
        CHECK(priority.voice==voice);
        CHECK(priority.kind==StereoDecisionKind::PriorityReplaced ||
              priority.kind==StereoDecisionKind::PriorityRetained);
        if(priority.kind==StereoDecisionKind::PriorityReplaced)++replacements;
        ++at;
    }
    CHECK(at==c.count && replacements>0u && replacements<32u);
    // A live device state can take different paths, independently of a
    // recorded V4 reference trace. This must alter the native decision trace.
    DeviceState other=state;
    other.voices[24].sample_end=1u;
    Chip finished;c.sus.fill(0x8000u);
    finished.sus.fill(0x8000u);finished.vtft.fill(0x03000000u);
    finished.ccca[24]=2u;finished.vtft[9]=0u;finished.vtft[10]=0u;
    const auto changed=allocate_pair_mode1(other,finished.io());
    CHECK(changed.status==Status::Found);
    bool observed_finished=false;
    for(std::size_t i=0;i<finished.count;++i)
        if(finished.decisions[i].voice==24 &&
           finished.decisions[i].kind==StereoDecisionKind::CursorReadFinished)
            observed_finished=true;
    CHECK(observed_finished);
    // On a failed first I/O, no completed score/cursor/priority decision is emitted.
    Chip failed;failed.reject_first_read=true;
    const auto error=allocate_pair_mode1(state,failed.io());
    CHECK(error.status==Status::PortError && failed.count==0);
    std::puts("P003_NATIVE_STEREO_DECISION_PATH_PASS 32 synthetic rows, 96 native decisions, no ghost events after I/O failure");
}
