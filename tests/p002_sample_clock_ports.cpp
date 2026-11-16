#include "CreativeSampleClockPorts.h"
#include <algorithm>
#include <array>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#define CHECK(x) do{if(!(x)){std::cerr<<"P002_CLOCK_PORT_FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
namespace {
// A completely SYNTHETIC audible instrument, from the existing
// direct_fork_fixture.cpp regression. This does not impersonate a
// captured Creative Win95 driver register sequence.
void startSyntheticVoice(awe64::Chip& chip) {
    const auto write16=[&](unsigned reg,unsigned voice,
                           std::uint16_t port,std::uint16_t val) {
        chip.writePort(0xe22u,static_cast<std::uint16_t>((reg<<5u)|voice));
        chip.writePort(port,val);
    };
    const auto write32=[&](unsigned reg,unsigned voice,
                           std::uint16_t port,std::uint32_t val) {
        chip.writePort(0xe22u,static_cast<std::uint16_t>((reg<<5u)|voice));
        chip.writePort(port,static_cast<std::uint16_t>(val));
        chip.writePort(static_cast<std::uint16_t>(port+2u),
                       static_cast<std::uint16_t>(val>>16u));
    };
    constexpr unsigned v=29u;
    write16(1,31u,0xa20u,0x0004u);
    write16(5,v,0xa20u,0x0080u);
    write32(3,v,0x620u,0x0000ffffu);
    write32(2,v,0x620u,0x0000ffffu);
    write32(6,v,0x620u,(128u<<24u)|459001u);
    write32(7,v,0x620u,467282u);
    write32(0,v,0xa20u,458995u);
    write16(0,v,0xe20u,0xe000u);
    write16(1,v,0xe20u,0xff00u);
    write16(2,v,0xe20u,0u);
    write16(3,v,0xe20u,0u);
    write16(4,v,0xe20u,0u);
    write16(5,v,0xe20u,0u);
    write16(6,v,0xa20u,0x8000u);
    write16(6,v,0xa22u,0x7f7fu);
    write16(7,v,0xa20u,0x7f00u);
    write16(4,v,0xa20u,0x8000u);
    write16(4,v,0xa22u,0x7f7fu);
    write16(5,v,0xa22u,0x8000u);
    write16(7,v,0xa22u,0x8000u);
    write32(1,v,0x620u,0x40000000u);
    write16(5,v,0xa20u,0x7f00u);
}
}
int main(){
    using namespace awe64;
    using namespace awe64::creative;
    const auto path=std::filesystem::temp_directory_path()/
        "125a-p002-clock-ports-synthetic-rom.raw";
    {
        std::ofstream f(path,std::ios::binary|std::ios::trunc);
        CHECK(f);
        // Entirely synthetic audible ROM, same 96-sample saw as P001.
        for(std::uint32_t i=0;i<524288u;++i) {
            const std::int16_t pcm=(i>=458900u && i<467400u)
                ? static_cast<std::int16_t>(
                    (static_cast<int>(i%96u)-48)*350) : 0;
            const auto word=static_cast<std::uint16_t>(pcm);
            f.put(static_cast<char>(word));
            f.put(static_cast<char>(word>>8u));
        }
        CHECK(f.good());
    }
    Chip expected,clocked,chunked;
    std::string err;
    CHECK(expected.open(path,err));
    CHECK(clocked.open(path,err));
    CHECK(chunked.open(path,err));
    CHECK(expected.armPortWriteTrace(32));
    CHECK(clocked.armPortWriteTrace(32));
    CHECK(chunked.armPortWriteTrace(32));
    constexpr std::array<SourceClockedPort,6> events{{
        {0,0xe22,0x1f},{0,0xa20,0x0004},
        {1,0xe22,0x60},{3,0x620,0xffff},
        {3,0x622,0},{4,0xe22,0xa0}
    }};
    // Exercise the SAME strict source/native frame gate used by the
    // exported VST3, with entirely synthetic values in public CI.
    // Source time offsets and independently generated OUT values must
    // agree in count, order, port and data before output is modified.
    std::array<awe64::ChipPortWrite,events.size()> nativeEvents{};
    for(std::size_t i=0u;i<events.size();++i)
        nativeEvents[i]={77u,events[i].port,events[i].value};
    std::array<SourceClockedPort,events.size()> approved{};
    std::size_t approvedCount=999u;
    CHECK(checkedNativePortsAtSourceFrames(
        events,nativeEvents,approved,approvedCount));
    CHECK(approvedCount==events.size());
    for(std::size_t i=0;i<events.size();++i) {
        CHECK(approved[i].relative_frame==events[i].relative_frame);
        CHECK(approved[i].port==nativeEvents[i].port);
        CHECK(approved[i].value==nativeEvents[i].value);
    }
    const auto intact=approved;
    auto badNative=nativeEvents;
    badNative[2].port=0xa22u;
    CHECK(!checkedNativePortsAtSourceFrames(
        events,badNative,approved,approvedCount));
    CHECK(approvedCount==0u && approved==intact);
    badNative=nativeEvents;
    badNative[4].value^=1u;
    CHECK(!checkedNativePortsAtSourceFrames(
        events,badNative,approved,approvedCount));
    CHECK(approvedCount==0u && approved==intact);
    CHECK(!checkedNativePortsAtSourceFrames(
        events,std::span<const awe64::ChipPortWrite>(
            nativeEvents.data(),nativeEvents.size()-1u),
        approved,approvedCount));
    CHECK(approvedCount==0u && approved==intact);
    CHECK(!checkedNativePortsAtSourceFrames(
        events,nativeEvents,
        std::span<SourceClockedPort>(approved.data(),approved.size()-1u),
        approvedCount));
    CHECK(approvedCount==0u && approved==intact);
    auto badSource=events;
    badSource[3].relative_frame=0u; // original source clock moved back
    CHECK(!checkedNativePortsAtSourceFrames(
        badSource,nativeEvents,approved,approvedCount));
    CHECK(approvedCount==0u && approved==intact);
    badSource=events;
    badSource[0].relative_frame=1u; // missing exact onset
    CHECK(!checkedNativePortsAtSourceFrames(
        badSource,nativeEvents,approved,approvedCount));
    CHECK(approvedCount==0u && approved==intact);
    CHECK(!checkedNativePortsAtSourceFrames(
        {},nativeEvents,approved,approvedCount));
    CHECK(approvedCount==0u && approved==intact);
    SourceClockedPortPlayer a,b;
    CHECK(!a.stage(clocked,{}));
    constexpr std::array<SourceClockedPort,2> reverse{{
        {0,0xe22,0x60},{0,0x999,0}
    }};
    CHECK(!a.stage(clocked,reverse));
    CHECK(clocked.portWriteTrace().empty());
    CHECK(a.stage(clocked,events));
    CHECK(b.stage(chunked,events));
    CHECK(!a.stage(clocked,events));
    // The independent control applies identical OUT values at the
    // measured sample boundaries without using SourceClockedPortPlayer.
    std::array<std::int32_t,2*32> reference{},continuous{},blocks{};
    std::size_t cursor=0;
    for(std::size_t frame=0;frame<32;++frame){
        while(cursor<events.size() && events[cursor].relative_frame==frame){
            expected.writePort(events[cursor].port,events[cursor].value);
            ++cursor;
        }
        expected.render(reference.data()+2*frame,1);
    }
    a.render(clocked,continuous.data(),32);
    b.render(chunked,blocks.data(),1);
    b.render(chunked,blocks.data()+2,2);
    b.render(chunked,blocks.data()+6,4);
    b.render(chunked,blocks.data()+14,25);
    CHECK(a.finished() && b.finished());
    CHECK(reference==continuous && reference==blocks);
    const auto referenceTrace=expected.portWriteTrace();
    for(const auto* chip:{&clocked,&chunked}){
        const auto trace=chip->portWriteTrace();
        CHECK(trace.size()==referenceTrace.size());
        for(std::size_t j=0;j<trace.size();++j)
            CHECK(trace[j].frame==referenceTrace[j].frame &&
                  trace[j].port==referenceTrace[j].port &&
                  trace[j].value==referenceTrace[j].value);
        CHECK(!chip->portWriteTraceOverflow());
    }
    CHECK(referenceTrace[0].frame==0u &&
          referenceTrace[2].frame==1u &&
          referenceTrace[3].frame==3u &&
          referenceTrace[5].frame==4u);
    a.clear();CHECK(a.pending()==0u);
    // Adversarial source timing: another caller moved the chip forward.
    // Never silently execute the old frame-0 commands at a later frame.
    Chip late;
    CHECK(late.open(path,err));
    CHECK(late.armPortWriteTrace(32u));
    SourceClockedPortPlayer stale;
    CHECK(stale.stage(late,events));
    std::array<std::int32_t,16u> staleAudio{};
    late.render(staleAudio.data(),1u);
    stale.render(late,staleAudio.data()+2u,7u);
    CHECK(stale.missedDeadline());
    CHECK(!stale.finished() && stale.pending()==events.size());
    CHECK(late.portWriteTrace().empty());
    stale.clear();
    CHECK(!stale.missedDeadline() && stale.pending()==0u);
    late.close();
    expected.close();clocked.close();chunked.close();

    // Actual audible chip regression: source-observed C4 register timing
    // has five buckets 8/20/82/16/26, totaling 152 OUTs. Contents
    // below are SYNTHETIC controls, NOT copied Creative register values.
    // This test asserts that the executor preserves the source's
    // five-frame temporal geometry when the unchanged DSP is sounding.
    Chip manualTone,oneBlockTone,splitTone;
    CHECK(manualTone.open(path,err));
    CHECK(oneBlockTone.open(path,err));
    CHECK(splitTone.open(path,err));
    for(auto* chip:{&manualTone,&oneBlockTone,&splitTone})
        startSyntheticVoice(*chip);
    std::array<std::int32_t,2u*512u> warm{};
    for(auto* chip:{&manualTone,&oneBlockTone,&splitTone})
        chip->render(warm.data(),512u);
    CHECK(std::any_of(warm.begin(),warm.end(),
                      [](auto v){return v!=0;}));
    CHECK(manualTone.armPortWriteTrace(152));
    CHECK(oneBlockTone.armPortWriteTrace(152));
    CHECK(splitTone.armPortWriteTrace(152));
    constexpr std::array<unsigned,5> sourceOriginalC4FrameCounts{
        8u,20u,82u,16u,26u};
    constexpr std::array<std::uint16_t,5> syntheticIfatn{
        0xff00u,0xff20u,0xff40u,0xff10u,0xff00u};
    std::array<SourceClockedPort,152> tonePlan{};
    std::size_t n=0;
    for(std::uint32_t frame=0u;frame<5u;++frame) {
        for(unsigned j=0;j<sourceOriginalC4FrameCounts[frame];++j) {
            const bool data=(j+1u==sourceOriginalC4FrameCounts[frame]);
            tonePlan[n++]={frame,
                static_cast<std::uint16_t>(data?0xe20u:0xe22u),
                static_cast<std::uint16_t>(
                    data?syntheticIfatn[frame]:((1u<<5u)|29u))};
        }
    }
    CHECK(n==tonePlan.size());
    SourceClockedPortPlayer oneBlockPlayer,splitPlayer;
    CHECK(oneBlockPlayer.stage(oneBlockTone,tonePlan));
    CHECK(splitPlayer.stage(splitTone,tonePlan));
    constexpr std::size_t nFrames=1024u;
    std::vector<std::int32_t> manual(nFrames*2u);
    std::vector<std::int32_t> allAtOnce(nFrames*2u);
    std::vector<std::int32_t> split(nFrames*2u);
    std::size_t next=0u;
    for(std::size_t frame=0;frame<nFrames;++frame) {
        while(next<tonePlan.size() &&
              tonePlan[next].relative_frame==frame) {
            manualTone.writePort(tonePlan[next].port,
                                 tonePlan[next].value);
            ++next;
        }
        manualTone.render(manual.data()+2u*frame,1u);
    }
    oneBlockPlayer.render(oneBlockTone,allAtOnce.data(),nFrames);
    std::size_t done=0u;
    for(const auto chunk:{1u,2u,17u,64u,121u,819u}) {
        CHECK(done+chunk<=nFrames);
        splitPlayer.render(splitTone,split.data()+2u*done,chunk);
        done+=chunk;
    }
    CHECK(done==nFrames && oneBlockPlayer.finished() &&
          splitPlayer.finished());
    CHECK(manual==allAtOnce && manual==split);
    CHECK(std::any_of(manual.begin(),manual.end(),
                      [](auto v){return v!=0;}));
    const auto source=manualTone.portWriteTrace();
    CHECK(source.size()==152u);
    for(const auto* chip:{&oneBlockTone,&splitTone}) {
        const auto rendered=chip->portWriteTrace();
        CHECK(rendered.size()==source.size());
        for(std::size_t i=0;i<source.size();++i)
            CHECK(rendered[i].frame==source[i].frame &&
                  rendered[i].port==source[i].port &&
                  rendered[i].value==source[i].value);
    }
    std::size_t at=0u;
    for(std::size_t frame=0u;frame<sourceOriginalC4FrameCounts.size();++frame)
        for(std::size_t j=0u;j<sourceOriginalC4FrameCounts[frame];++j)
            CHECK(source[at++].frame==512u+frame);
    manualTone.close();oneBlockTone.close();splitTone.close();
    std::filesystem::remove(path);
    std::cout<<"P002_ORIGINAL_CHIP_SAMPLE_CLOCK_PORT_EXECUTOR_PASS "
                "same_frame_order=TRUE chunk_invariant=TRUE "
                "pcm_exact=TRUE source_offsets_must_be_verified=TRUE\n";
}
