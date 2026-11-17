// P005/P116 ORIGINAL TABLE -> C++23 P54C translator full-path test.
// Synthetic decoder fields; NOT executed original Creative x86 or guest TSC.
// Source exact original table selector separately checked in 40,960 vectors.
#include "P005OriginalP54cOpcodeSourceFeed.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
using awe64::creative::P005OriginalPentiumCompiledBlockClock;
using awe64::creative::P005OriginalP54cOpcodeSourceFeed;
using awe64::creative::P005OriginalDecoderOpcodeInput;
using awe64::research_p116::originalP54cLookup;
#define CHECK(x) do {if(!(x)){ std::printf("P005_P116_SOURCE_FEED_FAIL line=%d: %s\n",__LINE__,#x);return 1; }} while(false)
int main() {
    constexpr std::uint32_t pc=0xc1019207u,phys=0x000a0000u;
    constexpr std::array<std::uint8_t,1> prefix0f{{0x0fu}};
    const auto movRegister=originalP54cLookup(0u,0x89u,0xc0u);
    const auto movMemory=originalP54cLookup(0u,0x89u,0u);
    const auto nearJcc=originalP54cLookup(0x0fu,0x85u,0u);
    CHECK(movRegister && movMemory && nearJcc);
    CHECK(movRegister->integer_subset && movMemory->integer_subset);
    CHECK(nearJcc->integer_subset);
    CHECK(movRegister->timing_word!=movMemory->timing_word);
    P005OriginalPentiumCompiledBlockClock compiled{};
    CHECK(compiled.startFreshVirtualEpoch());
    CHECK(compiled.beginTranslation(7u,1u,pc,phys));
    CHECK(P005OriginalP54cOpcodeSourceFeed::appendSourceDecodedInteger(
        compiled,{0u,0x89u,0xc0u,0x300u,{}}));
    CHECK(P005OriginalP54cOpcodeSourceFeed::appendSourceDecodedInteger(
        compiled,{0u,0x89u,0u,0x300u,{}}));
    CHECK(compiled.finishTranslation());
    CHECK(compiled.compiledCost(7u,1u)>0u);
    const auto cycleCost=compiled.compiledCost(7u,1u);
    CHECK(compiled.executeCompiledBlock(7u,1u,pc,phys,{}));
    CHECK(compiled.executeCompiledBlock(7u,1u,pc,phys,{}));
    CHECK(compiled.cpuTsc()==2u*cycleCost);
    // Original near-Jcc uses source 0F lookup and the actual 0F prefix.
    CHECK(compiled.beginTranslation(8u,2u,pc+4u,phys+4u));
    CHECK(P005OriginalP54cOpcodeSourceFeed::appendSourceDecodedInteger(
        compiled,{0x0fu,0x85u,0u,0x300u,prefix0f}));
    CHECK(compiled.finishTranslation());
    CHECK(compiled.compiledCost(8u,2u)>0u);
    // Any absent or forged prefix context rejects the complete block.
    compiled.reset();
    CHECK(compiled.startFreshVirtualEpoch());
    CHECK(compiled.beginTranslation(1u,1u,pc,phys));
    CHECK(!P005OriginalP54cOpcodeSourceFeed::appendSourceDecodedInteger(
        compiled,{0x0fu,0x85u,0u,0x300u,{}}));
    CHECK(compiled.failed() && compiled.compiledCost(1u,1u)==0u);
    // x87 family isn't silently translated with integer timing.
    compiled.reset();
    CHECK(compiled.startFreshVirtualEpoch());
    CHECK(compiled.beginTranslation(1u,1u,pc,phys));
    CHECK(!P005OriginalP54cOpcodeSourceFeed::appendSourceDecodedInteger(
        compiled,{0xd9u,0x00u,0u,0x300u,{}}));
    CHECK(compiled.failed());
    // Unsupported/forged 86Box decoder flags also fail closed.
    compiled.reset();
    CHECK(compiled.startFreshVirtualEpoch());
    CHECK(compiled.beginTranslation(1u,1u,pc,phys));
    CHECK(!P005OriginalP54cOpcodeSourceFeed::appendSourceDecodedInteger(
        compiled,{0u,0x89u,0xc0u,0x400u,{}}));
    CHECK(compiled.failed());
    std::puts("P005_P116_ORIGINAL_SOURCE_LOOKUP_TO_P54C_TRANSLATOR_PASS table_source=PINNED temporary_oracle=TRUE guest_x86_execution=NOT_PROVEN private_data=NONE");
}
