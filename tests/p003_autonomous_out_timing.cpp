// P003 autonomous clock research regression: NO original Creative media.
// Synthetic native chip-port transactions only. Never certifies Win95
// parity from this unit test; a separate private original WAV is required.
#include "CreativeAutonomousOutTiming.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
using awe64::ChipPortWrite;
using awe64::creative::P003AutonomousTimingStats;
using awe64::creative::SourceClockedPort;
using awe64::creative::p003CandidateAutonomousNoteTimeline;
#define CHECK(c) do {if(!(c)) {std::cerr<<"P003_AUTONOMOUS_TIMING_FAIL "<<__LINE__<<" "<<#c<<"\n";return 1;}}while(false)
namespace {
std::vector<ChipPortWrite> generatedTwoVoices(
    const bool additionalInitialReads) {
    std::vector<ChipPortWrite> v;
    const auto add=[&](const unsigned port,const unsigned word) {
        v.push_back({777u,static_cast<std::uint16_t>(port),
                     static_cast<std::uint16_t>(word)});
    };
    for(unsigned voice=0;voice<2;++voice) {
        add(0xe22u,0x60u+voice);
        if(additionalInitialReads) {
            add(0xe22u,0x60u+voice);
            add(0xe22u,0x60u+voice);
        }
        add(0xe22u,0xa0u+voice);
        add(0xa20u,0xffu);
        add(0xe22u,0x60u+voice);
        add(0x620u,0xffffu);
        add(0x622u,0u);
    }
    // 140 extra program writes. The type mix resembles normal E22
    // index selection plus A20/E20/620 data, without copied source
    // port values or notes.
    for(unsigned i=0;i<140;++i) {
        if(i%5u==0 || i%5u==2) add(0xe22u,i%32u);
        else add((i%3u==0)?0xe20u:((i%3u==1)?0x620u:0xa20u),
                 0x4000u+i);
    }
    return v;
}
}
int main() {
    std::array<SourceClockedPort,512u> output{};
    std::size_t count=0u;
    P003AutonomousTimingStats stats{};
    auto a=generatedTwoVoices(false);
    auto b=generatedTwoVoices(true);
    CHECK(a.size()==152u && b.size()==156u);
    CHECK(p003CandidateAutonomousNoteTimeline(a,false,output,count,&stats));
    CHECK(count==a.size() && stats.initial_voice_prelude_writes==12u);
    for(unsigned i=0;i<6;++i)CHECK(output[i].relative_frame==0u);
    for(unsigned i=6;i<12;++i)CHECK(output[i].relative_frame==1u);
    CHECK(output[12].relative_frame==2u);
    CHECK(output.back().relative_frame>=3u);
    for(std::size_t i=0;i<count;++i) {
        CHECK(output[i].port==a[i].port && output[i].value==a[i].value);
        if(i)CHECK(output[i].relative_frame>=output[i-1u].relative_frame);
    }
    std::array<SourceClockedPort,512u> independent{};
    std::size_t otherCount=0u;
    CHECK(p003CandidateAutonomousNoteTimeline(
        b,false,independent,otherCount,&stats));
    CHECK(otherCount==156u && stats.initial_voice_prelude_writes==16u);
    for(unsigned i=0;i<8;++i)CHECK(independent[i].relative_frame==0u);
    for(unsigned i=8;i<16;++i)CHECK(independent[i].relative_frame==1u);
    CHECK(independent[16].relative_frame==2u);
    CHECK(independent[155].relative_frame>=3u);
    // Arbitrary MIDI data bytes MUST NOT change the same synthetic
    // autonomous frame distribution or be replaced with fixture bytes.
    auto mutated=a;
    for(auto& e:mutated)if(e.port!=0xe22u && e.port!=0x622u)e.value^=0x1fu;
    std::array<SourceClockedPort,512u> changed{};
    std::size_t changedCount=0u;
    CHECK(p003CandidateAutonomousNoteTimeline(
        mutated,false,changed,changedCount));
    CHECK(changedCount==count);
    for(std::size_t i=0;i<count;++i) {
        CHECK(changed[i].relative_frame==output[i].relative_frame);
        CHECK(changed[i].value==mutated[i].value);
    }
    auto bad=a;bad[42].port=0x999u;
    CHECK(!p003CandidateAutonomousNoteTimeline(bad,false,output,count));
    CHECK(count==0u);
    bad=a;bad[5].value=0xffffu; // first voice completion absent
    CHECK(!p003CandidateAutonomousNoteTimeline(bad,false,output,count));
    bad=a;bad.resize(9u);
    CHECK(!p003CandidateAutonomousNoteTimeline(bad,false,output,count));
    bad=a;bad.resize(513u);
    CHECK(!p003CandidateAutonomousNoteTimeline(bad,false,output,count));
    bad=a;bad.resize(20u);
    CHECK(!p003CandidateAutonomousNoteTimeline(bad,false,output,count));
    auto release=std::vector<ChipPortWrite>{
        {0u,0xe22u,0xa0u},{0u,0xa20u,0x8027u},
        {0u,0xe22u,0xe0u},{0u,0xa20u,0x8004u}};
    CHECK(p003CandidateAutonomousNoteTimeline(
        release,true,output,count,&stats));
    CHECK(count==release.size() && stats.final_relative_frame==0u);
    for(std::size_t i=0;i<count;++i)
        CHECK(output[i].relative_frame==0u);
    release.resize(17u);
    CHECK(!p003CandidateAutonomousNoteTimeline(release,true,output,count));
    CHECK(!p003CandidateAutonomousNoteTimeline({},false,output,count));
    std::cout<<"P003_AUTONOMOUS_NATIVE_PACING_CANDIDATE_CONTROLS_PASS"
             <<" authentic_source_timing_parity=NOT_TESTED"
             <<" release_approved=FALSE\n";
    return 0;
}
