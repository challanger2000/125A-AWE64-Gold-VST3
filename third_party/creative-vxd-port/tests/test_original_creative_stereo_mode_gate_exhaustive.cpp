#include "OriginalCreativeVxdLive.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}} while(false)

// Synthetic state-space test of the *native* LCOD+4029..4042 mode gate.
// Independent mask predicate, no original recorder bytes/CPU timings,
// no guessed 14/36-cycle constants and no source-session replays.
struct Spy {
    std::uint8_t selector=0;
    std::array<unsigned,32> vtft_calls{},sus_calls{},ccca_calls{};
    std::array<StereoDecisionEvent,96> decisions{};
    std::size_t used=0;
    static void observe(void* ptr,StereoDecisionEvent e) noexcept {
        auto& s=*static_cast<Spy*>(ptr);
        if(s.used<s.decisions.size())s.decisions[s.used++]=e;
    }
    static bool out8(void* p,std::uint16_t port,std::uint8_t value) noexcept {
        auto& s=*static_cast<Spy*>(p);
        if(port!=0xe22u)return false;
        s.selector=value;return true;
    }
    static bool out16(void* p,std::uint16_t port,std::uint16_t value) noexcept {
        auto& s=*static_cast<Spy*>(p);
        return port==0xa20u && (s.selector&0xe0u)==0xa0u && value==0xffu;
    }
    static bool in16(void* p,std::uint16_t port,std::uint16_t& value) noexcept {
        auto& s=*static_cast<Spy*>(p);
        if(port!=0xa20u || (s.selector&0xe0u)!=0xa0u)return false;
        ++s.sus_calls[s.selector&31u];
        value=0x8000u;return true;
    }
    static bool in32(void* p,std::uint16_t port,std::uint32_t& value) noexcept {
        auto& s=*static_cast<Spy*>(p);
        const auto voice=static_cast<unsigned>(s.selector&31u);
        if(port==0x620u && (s.selector&0xe0u)==0x60u) {
            ++s.vtft_calls[voice];value=0x03000000u;return true;
        }
        if(port==0xa20u && (s.selector&0xe0u)==0u) {
            ++s.ccca_calls[voice];value=0u;return true;
        }
        return false;
    }
    PortIo io() noexcept {
        PortIo p{this,out8,out16,in16,in32,nullptr};
        p.stereo_decision_opaque=this;
        p.stereo_decision_observer=observe;
        return p;
    }
};

int main() {
    unsigned cases=0,skips=0,reads=0;
    for(unsigned capacity=2;capacity<=8;++capacity) {
        for(unsigned mode : {0u,1u,2u,3u,4u,32u}) {
            const unsigned possibilities=1u<<capacity;
            for(unsigned mask=0;mask<possibilities;++mask) {
                DeviceState state;
                state.physical_capacity=static_cast<std::uint16_t>(capacity);
                state.mode_count=mode;
                for(unsigned voice=0;voice<capacity;++voice) {
                    auto& row=state.voices[voice];
                    row.blocked=((mask>>voice)&1u)!=0u;
                    row.key_marker=0x0101u;
                    row.sample_end=0x1000u;
                }
                Spy spy;
                auto result=allocate_pair_mode1(state,spy.io());
                CHECK(result.status==Status::Found ||
                      result.status==Status::NoCandidate);
                std::size_t pos=0;
                unsigned expected_vtft=0;
                for(unsigned n=capacity;n>0;) {
                    const unsigned v=--n;
                    const bool blocked=(mask&(1u<<v))!=0;
                    const bool upper_blocked=v+1u<capacity &&
                                          (mask&(1u<<(v+1u)))!=0u;
                    // The upper-neighbor check is bounded by [ESI+06h].
                    const bool skip=mode<=2u && (blocked||upper_blocked);
                    const auto expected=skip?
                        StereoDecisionKind::SkippedVoice:
                        StereoDecisionKind::ScoredVoice;
                    CHECK(pos<spy.used);
                    CHECK(spy.decisions[pos].voice==v);
                    CHECK(spy.decisions[pos].kind==expected);
                    ++pos;
                    if(skip) {
                        ++skips;
                        CHECK(spy.vtft_calls[v]==0u);
                        CHECK(spy.sus_calls[v]==0u);
                        CHECK(spy.ccca_calls[v]==0u);
                    }else {
                        ++reads;
                        ++expected_vtft;
                        CHECK(spy.vtft_calls[v]==1u);
                        CHECK(spy.sus_calls[v]==1u);
                        CHECK(spy.ccca_calls[v]==1u);
                        // Every scored synthetic row has a live,
                        // unfinished cursor, then one priority decision.
                        CHECK(pos+1u<spy.used);
                        CHECK(spy.decisions[pos].voice==v);
                        CHECK(spy.decisions[pos].kind==
                              StereoDecisionKind::CursorReadNotFinished);
                        ++pos;
                        CHECK(spy.decisions[pos].voice==v);
                        CHECK(spy.decisions[pos].kind==
                              StereoDecisionKind::PriorityReplaced ||
                              spy.decisions[pos].kind==
                              StereoDecisionKind::PriorityRetained);
                        ++pos;
                    }
                }
                CHECK(pos==spy.used);
                CHECK(result.vtft_reads==expected_vtft);
                CHECK(result.dcysusv_reads==expected_vtft);
                CHECK(result.ccca_reads==expected_vtft);
                ++cases;
            }
        }
    }
    CHECK(cases==3024u);
    std::printf("P003_ORIGINAL_MODE_GATE_STATE_SPACE_PASS cases=%u skips=%u scored=%u\n",
                cases,skips,reads);
}
