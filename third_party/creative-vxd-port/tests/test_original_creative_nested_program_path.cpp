#include "OriginalCreativeNestedProgramPath.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
using namespace awe64::original_vxd;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(false)
namespace {
template<std::size_t N>
void set16(std::array<std::uint8_t,N>& a,std::size_t i,unsigned v){
    a[i]=static_cast<std::uint8_t>(v);
    a[i+1u]=static_cast<std::uint8_t>(v>>8u);
}
template<std::size_t N>
void set32(std::array<std::uint8_t,N>& a,std::size_t i,unsigned v){
    set16(a,i,v);set16(a,i+2u,v>>16u);
}
}
int main(){
    // A small, entirely synthetic cross-region shape. No VXD/SF2 bytes.
    std::array<OriginalPresetTableRow,3> rows{{
        {1u,0u,0u,0u},{1u,1u,0u,1u},{0xffffu,0u,0u,2u}
    }};
    std::array<std::uint8_t,12> po{}; // 0,1,2
    set32(po,4u,1u);set32(po,8u,2u);
    std::array<std::uint8_t,6> pg{{41u,0u,0u,41u,1u,0u}};
    std::array<std::uint8_t,6> io{}; // 0,1,2
    set16(io,2u,1u);set16(io,4u,2u);
    std::array<std::uint8_t,12> ig{}; // 0,1,2
    set32(ig,4u,1u);set32(ig,8u,2u);
    std::array<std::uint8_t,6> gen{{53u,7u,0u,53u,9u,0u}};
    OriginalNestedProgramTables t{rows,po,pg,io,ig,gen,false};
    NestedPathCounts counts{};
    std::array<std::uint16_t,2> output{{0xbeefu,0xbeefu}};
    CHECK(original_nested_program_sample_ids(t,0u,output,counts)==
          NestedPathResult::Unverified);
    CHECK(output[0]==0xbeefu && counts.sample_id_references==0u);
    t.independently_authenticated=true;
    CHECK(original_nested_program_sample_ids(t,0u,output,counts)==
          NestedPathResult::Ok);
    CHECK(output[0]==7u && output[1]==0xbeefu);
    CHECK(counts.groups==1u && counts.instrument_bags==1u &&
          counts.sample_id_references==1u);
    CHECK(original_nested_program_sample_ids(t,1u,output,counts)==
          NestedPathResult::Ok);
    CHECK(output[0]==9u && counts.sample_id_references==1u);
    // Caller output is transactional on bad index, missing source, and
    // insufficient destination; no guessed sample offset is emitted.
    output={0xbeefu,0xbeefu};
    CHECK(original_nested_program_sample_ids(t,0u,
          std::span<std::uint16_t>{},counts)==
          NestedPathResult::OutputTooSmall);
    CHECK(output[0]==0xbeefu);
    CHECK(original_nested_program_sample_ids(t,2u,output,counts)==
          NestedPathResult::Malformed);
    pg[0]=42u;
    CHECK(original_nested_program_sample_ids(t,0u,output,counts)==
          NestedPathResult::MissingReference);
    CHECK(output[0]==0xbeefu);
    pg[0]=41u;
    pg[1]=255u; // instrument 255 missing from original u16 index region
    CHECK(original_nested_program_sample_ids(t,0u,output,counts)==
          NestedPathResult::MissingReference);
    pg[1]=0u;
    ig[4]=255u; // nonmonotone/out-of-bounds generator offset
    CHECK(original_nested_program_sample_ids(t,0u,output,counts)==
          NestedPathResult::Malformed);
    ig[4]=1u;
    gen[0]=59u; // opcode outside observed Creative generator range
    CHECK(original_nested_program_sample_ids(t,0u,output,counts)==
          NestedPathResult::Malformed);
    gen[0]=53u;
    CHECK(original_nested_program_sample_ids(t,0u,output,counts)==
          NestedPathResult::Ok);
    CHECK(output[0]==7u);
    std::puts("P005_NESTED_PROGRAM_PATH_PASS source-indexed two programs, transactional malformed/unknown/unverified controls");
}
