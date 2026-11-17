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
    // P003/P107 cross-session guard: native mode_count has a source-based
    // <=2 blocked-voice predicate. This deliberately tests both branches,
    // NOT a 14-cycle patch and NOT an assertion that guest [ESI+0x18]
    // points to this native field (that mapping still needs guest proof).
    for(std::uint32_t mode : {0u,1u,2u,3u,4u}) {
        DeviceState gated=state;
        gated.physical_capacity=4u;
        gated.mode_count=mode;
        gated.voices[3].key_marker=0x0101u;
        gated.voices[3].blocked=true;
        gated.voices[2].key_marker=0x0101u;
        gated.voices[2].blocked=false;
        Chip witness;
        witness.sus.fill(0x8000u);
        witness.vtft.fill(0x03000000u);
        const auto selected=allocate_pair_mode1(gated,witness.io());
        CHECK(selected.status==Status::Found ||
              selected.status==Status::NoCandidate);
        CHECK(witness.count>0u);
        const auto first=witness.decisions[0];
        CHECK(first.voice==3u);
        CHECK(first.kind==(mode<=2u ?
              StereoDecisionKind::SkippedVoice :
              StereoDecisionKind::ScoredVoice));
        if(mode<=2u) {
            CHECK(selected.vtft_reads<4u);
            // The source scan also checks the *neighbor* voice:
            // row 2 is skipped because neighboring row 3 is blocked.
            CHECK(witness.decisions[1].voice==2u);
            CHECK(witness.decisions[1].kind==StereoDecisionKind::SkippedVoice);
            CHECK(witness.decisions[2].voice==1u);
            CHECK(witness.decisions[2].kind==StereoDecisionKind::ScoredVoice);
        }else {
            CHECK(selected.vtft_reads==4u);
        }
    }

    // Original V5 recorder cross-iteration address identity:
// old EBX+0x0D == (old EBX-0x20)+0x2D.
// Reading +0x0D after the 32-byte decrement is therefore exactly
// the SAME absolute guest address as reading +0x2D before it.
// These are recorder observation points, not two interchangeable
// offsets within the SAME x86 instruction.
static_assert(0x0Du + 0x20u == 0x2Du);
static_assert((0x1000u + 0x0Du) ==
              ((0x1000u - 0x20u) + 0x2Du));

// P003-V5 source disassembly maps [EBX+0Dh] to current row+0x17,
    // [EBX+2Dh] to next row+0x17. Test those *separately*, including
    // the original [ESI+06h] upper capacity boundary.
    for(std::uint32_t mode : {2u,4u}) {
        DeviceState gated=state;
        gated.physical_capacity=4u;
        gated.mode_count=mode;
        for(auto& voice : gated.voices)voice.blocked=false;
        gated.voices[2].blocked=true; // NOT the initial row 3.
        Chip witness;
        witness.sus.fill(0x8000u);
        witness.vtft.fill(0x03000000u);
        const auto result=allocate_pair_mode1(gated,witness.io());
        CHECK(result.status==Status::Found ||
              result.status==Status::NoCandidate);
        // Voice 3 must be scored: no non-existent next row 4 lookup.
        CHECK(witness.decisions[0].voice==3u);
        CHECK(witness.decisions[0].kind==StereoDecisionKind::ScoredVoice);
        // For mode 2, current voice 2 and preceding voice 1 skip;
        // for mode 4 neither blocked flag is enforced.
        bool saw2=false, saw1=false;
        for(std::size_t i=0;i<witness.count;++i){
            const auto d=witness.decisions[i];
            if(d.voice==2u &&
               d.kind==StereoDecisionKind::SkippedVoice)saw2=true;
            if(d.voice==1u &&
               d.kind==StereoDecisionKind::SkippedVoice)saw1=true;
        }
        CHECK(saw2==(mode<=2u));
        CHECK(saw1==(mode<=2u));
        CHECK(result.vtft_reads==(mode<=2u?2u:4u));
    }
    // On a failed first I/O, no completed score/cursor/priority decision is emitted.
    Chip failed;failed.reject_first_read=true;
    const auto error=allocate_pair_mode1(state,failed.io());
    CHECK(error.status==Status::PortError && failed.count==0);
    std::puts("P003_NATIVE_STEREO_DECISION_PATH_PASS 32 synthetic rows, 96 native decisions, no ghost events after I/O failure");
}
