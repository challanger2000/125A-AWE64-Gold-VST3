// 1996 Creative observed source sample conversion regression.
// SHA-backed full 260,921-word owner PCM is tested only privately.
// Public fixture has no copyrighted ROM, VXD or SoundFont bytes.
#include "CreativeOriginalSmplThreeTap.h"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <vector>
using awe64::creative::p003PredictOwnerObservedThreeTapRamWords;
#define CHECK(x) do {if(!(x)){std::cerr<<"P003_THREE_TAP_SOURCE_MODEL_FAIL line "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
std::vector<std::uint8_t> toLe16(const std::span<const std::int16_t> input) {
    std::vector<std::uint8_t> bytes;
    for(const auto signedWord:input){
        const auto w=std::bit_cast<std::uint16_t>(signedWord);
        bytes.push_back(static_cast<std::uint8_t>(w));
        bytes.push_back(static_cast<std::uint8_t>(w>>8u));
    }
    return bytes;
}
int main() {
    // Immutable independent 26-word oracle computed from the existing
    // source-private Python transfer auditor's exact integer formula.
    const std::array<std::int16_t,26> input{
        0,-48,-307,-1672,-2574,-2441,-2913,1519,7839,0,0,0,
        5399,-3251,0,-7564,-2904,-1123,2895,0,0,7881,2812,
        -7346,-10681,0
    };
    const std::array<std::int16_t,26> expected{
        0,2,-16,-894,-1417,-1145,-2070,523,5689,0,0,0,
        4455,-3114,1351,-5310,-1093,-842,2311,-362,-986,5559,
        2042,-4526,-7093,0
    };
    const auto source=toLe16(input);
    const auto result=p003PredictOwnerObservedThreeTapRamWords(source);
    CHECK(result && result->size()==input.size());
    for(std::size_t i=0;i<input.size();++i)
        CHECK((*result)[i]==expected[i]);
    CHECK((*result)[1]==2); // signed division FLOORS negative values
    CHECK((*result)[9]==0 && (*result)[10]==0 &&
          (*result)[11]==0); // long 3-zero source run untouched
    CHECK((*result)[14]==1351); // isolated zero != long-zero run
    CHECK((*result)[19]==-362 && (*result)[20]==-986);
    // Negative controls must never silently promote corrupt or
    // insufficient owner source data to original driver parity.
    auto invalid=source;
    invalid[0]=1u;
    CHECK(!p003PredictOwnerObservedThreeTapRamWords(invalid));
    invalid=source;invalid.back()=1u;
    CHECK(!p003PredictOwnerObservedThreeTapRamWords(invalid));
    invalid=source;invalid.pop_back();
    CHECK(!p003PredictOwnerObservedThreeTapRamWords(invalid));
    invalid.assign(4u,0u);
    CHECK(!p003PredictOwnerObservedThreeTapRamWords(invalid));
    // It is a model only: structural and SHA equality against owner
    // SF2/final-RAM is a DISTINCT test run BEFORE actual RAM upload.
    std::cout<<"P003_ORIGINAL_OWNER_THREE_TAP_SOURCE_MODEL_PASS"
             <<" synthetic_words=26 exact_words=26"
             <<" three_zero_guard=PASS isolated_zero=PASS"
             <<" negative_controls=4"
             <<" owner_full_260921_byte_parity=EXTERNAL_SOURCE_GATE"
             <<" private_licensed_bytes=NONE\n";
    return 0;
}
