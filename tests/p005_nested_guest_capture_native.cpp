// Source-only C++23 integration: authenticated schema is NOT session attestation.
// Both 125AVXD1/125AVXR1 fixtures are synthetic, original binaries excluded.
#include "P005OriginalNestedGuestCaptureFile.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>
using namespace awe64::vst3;
using namespace awe64::original_vxd;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"P005_NESTED_GUEST_FAIL:%d %s\n",__LINE__,#x);return 1;}}while(false)
namespace {
void put16(std::vector<std::uint8_t>& data,std::size_t at,unsigned value) {
    data[at]=static_cast<std::uint8_t>(value);
    data[at+1u]=static_cast<std::uint8_t>(value>>8u);
}
void put32(std::vector<std::uint8_t>& data,std::size_t at,std::uint32_t value) {
    put16(data,at,value);put16(data,at+2u,value>>16u);
}
void append32(std::vector<std::uint8_t>& data,std::uint32_t value) {
    const auto old=data.size();
    data.resize(old+4u);
    put32(data,old,value);
}
}
int main() {
    constexpr std::uint32_t root=0xc11b891cu;
    constexpr std::uint32_t deviceVa=0xc30b09f0u;
    constexpr std::uint32_t cr3=0x3000u;
    constexpr std::size_t deviceLen=0x1592u;
    constexpr std::array<std::uint32_t,7> pointers{{
        root+76u,root+76u+78u,root+76u+78u+12u,
        root+76u+78u+18u,root+76u+78u+24u,
        root+76u+78u+36u,root+76u+78u+42u
    }};
    std::vector<std::uint8_t> primary{
        '1','2','5','A','V','X','D','1'};
    append32(primary,1u);
    append32(primary,deviceVa);
    append32(primary,cr3);
    append32(primary,static_cast<std::uint32_t>(deviceLen));
    append32(primary,288u);
    append32(primary,1u);
    append32(primary,1u);
    const auto dev=primary.size();
    primary.resize(dev+deviceLen);
    put16(primary,dev+6u,32u);
    put32(primary,dev+0x158eu,0x4d474d31u);
    put32(primary,dev+0x6cau,root);
    append32(primary,root);
    append32(primary,76u+3u*26u);
    const auto bank=primary.size();
    primary.resize(bank+76u+3u*26u);
    for(std::size_t i=0u;i<7u;++i)
        put32(primary,bank+i*4u,pointers[i]);
    for(unsigned i=0u;i<3u;++i) {
        const auto row=bank+76u+26u*i;
        put16(primary,row,i==2u?0xffffu:1u);
        primary[row+2u]=static_cast<std::uint8_t>(i);
        put16(primary,row+4u,i);
    }
    std::vector<std::uint8_t> nested{
        '1','2','5','A','V','X','R','1'};
    append32(nested,1u);
    append32(nested,deviceVa);
    append32(nested,cr3);
    append32(nested,root);
    for(auto pointer:pointers)append32(nested,pointer);
    append32(nested,6u);
    std::array<std::vector<std::uint8_t>,6> regions{};
    regions[0].resize(12u);
    put32(regions[0],4u,1u);
    put32(regions[0],8u,2u);
    regions[1]={41u,0u,0u,41u,1u,0u};
    regions[2].resize(6u);
    put16(regions[2],2u,1u);
    put16(regions[2],4u,2u);
    regions[3].resize(12u);
    put32(regions[3],4u,1u);
    put32(regions[3],8u,2u);
    regions[4]={53u,7u,0u,53u,9u,0u};
    regions[5].resize(4096u);
    for(std::size_t i=0u;i<6u;++i) {
        append32(nested,pointers[i+1u]);
        append32(nested,static_cast<std::uint32_t>(regions[i].size()));
        append32(nested,i==5u?0u:1u);
        nested.insert(nested.end(),regions[i].begin(),regions[i].end());
    }

    using R=P005NestedGuestCaptureResult;
    P005OriginalNestedGuestCaptureFile capture;
    CHECK(capture.loadNonRealtime(primary,nested)==R::Ok);
    CHECK(capture.sourceFormatReady());
    CHECK(capture.sourceRootGuestAddress()==root);
    CHECK(capture.sourceProgramRows()==3u);
    CHECK(!capture.finalGuestRegionComplete());
    std::array<std::uint16_t,2> sample{{0xbeefu,0xbeefu}};
    NestedPathCounts tally{};
    CHECK(original_nested_program_sample_ids(
        capture.tables(),0u,sample,tally)==NestedPathResult::Unverified);
    CHECK(original_nested_program_sample_ids(
        capture.tables(true),0u,sample,tally)==NestedPathResult::Ok);
    CHECK(sample[0]==7u && sample[1]==0xbeefu);
    CHECK(tally.groups==1u && tally.sample_id_references==1u);
    CHECK(original_nested_program_sample_ids(
        capture.tables(true),1u,sample,tally)==NestedPathResult::Ok);
    CHECK(sample[0]==9u);
    // Owned source: modifying caller arrays after load never changes
    // original rows or compact generator references in the parser.
    const auto originalRowByte=primary[bank+76u+4u];
    const auto originalIndexByte=nested[56u+12u+4u];
    primary[bank+76u+4u]=99u;
    nested[56u+12u+4u]=99u;
    CHECK(original_nested_program_sample_ids(
        capture.tables(true),0u,sample,tally)==NestedPathResult::Ok);
    CHECK(sample[0]==7u);
    primary[bank+76u+4u]=originalRowByte;
    nested[56u+12u+4u]=originalIndexByte;
    // Cross-capture CR3, root, header, and bounded section tampering.
    auto corrupted=nested;
    put32(corrupted,16u,0x4000u);
    CHECK(capture.loadNonRealtime(primary,corrupted)==R::InconsistentOriginalSource);
    CHECK(!capture.sourceFormatReady());
    corrupted=nested;
    put32(corrupted,24u,pointers[0]+1u);
    CHECK(capture.loadNonRealtime(primary,corrupted)==R::InconsistentOriginalSource);
    corrupted=nested;
    put32(corrupted,56u,pointers[1]+1u);
    CHECK(capture.loadNonRealtime(primary,corrupted)==R::InconsistentOriginalSource);
    corrupted=nested;
    put32(corrupted,56u+8u,0u); // bounded region falsely marked incomplete
    CHECK(capture.loadNonRealtime(primary,corrupted)==R::InconsistentOriginalSource);
    corrupted=nested;
    corrupted.pop_back();
    CHECK(capture.loadNonRealtime(primary,corrupted)==R::IncompleteRegion);
    // On mismatch, never retain the previously accepted bank.
    CHECK(!capture.sourceFormatReady());
    CHECK(capture.sourceProgramRows()==0u);
    CHECK(capture.loadNonRealtime(primary,nested)==R::Ok);
    capture.resetNonRealtime();
    CHECK(!capture.sourceFormatReady());
    // A reset parser must NOT manufacture an authenticated view.
    CHECK(original_nested_program_sample_ids(
        capture.tables(true),0u,sample,tally)==NestedPathResult::Unverified);
    std::puts("P005_ORIGINAL_NESTED_PRIMARY_NATIVE_PASS authenticated-format-only no-production-claim");
    return 0;
}
