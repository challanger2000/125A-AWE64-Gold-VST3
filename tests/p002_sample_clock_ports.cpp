#include "CreativeSampleClockPorts.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#define CHECK(x) do{if(!(x)){std::cerr<<"P002_CLOCK_PORT_FAIL "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
int main(){
    using namespace awe64;
    using namespace awe64::creative;
    const auto path=std::filesystem::temp_directory_path()/
        "125a-p002-clock-ports-synthetic-rom.raw";
    {
        std::ofstream f(path,std::ios::binary|std::ios::trunc);
        CHECK(f);
        std::array<char,4096> zero{};
        for(int i=0;i<256;++i)f.write(zero.data(),zero.size());
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
    expected.close();clocked.close();chunked.close();
    std::filesystem::remove(path);
    std::cout<<"P002_ORIGINAL_CHIP_SAMPLE_CLOCK_PORT_EXECUTOR_PASS "
                "same_frame_order=TRUE chunk_invariant=TRUE "
                "pcm_exact=TRUE source_offsets_must_be_verified=TRUE\n";
}
