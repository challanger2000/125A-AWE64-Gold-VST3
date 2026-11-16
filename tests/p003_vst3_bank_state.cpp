#include "P003VstBankState.h"
#include <array>
#include <cstdint>
#include <cstdio>
using namespace awe64::vst3::state;
#define CHECK(c) do{if(!(c)){std::fprintf(stderr,"P003_STATE_FAIL %s:%d %s\n",__FILE__,__LINE__,#c);return 1;}}while(false)
int main() {
    constexpr std::array<BankSelection,5> cases{{
        {0u,0u,100u,64u,127u},{1u,7u,0u,0u,0u},
        {127u,80u,127u,127u,127u},{128u,127u,1u,127u,1u},
        {2u,42u,61u,78u,99u}
    }};
    for(const auto& src:cases) {
        CHECK(valid(src));
        const auto data=encode(src);
        const auto parsed=decode(data);
        CHECK(parsed.has_value());
        CHECK(parsed->bank==src.bank && parsed->program==src.program &&
              parsed->cc7==src.cc7 && parsed->cc10==src.cc10 &&
              parsed->cc11==src.cc11);
        const auto atomicBits=pack(src);
        const auto fromBits=unpack(atomicBits);
        CHECK(fromBits && fromBits->bank==src.bank &&
              fromBits->program==src.program &&
              fromBits->cc7==src.cc7 &&
              fromBits->cc10==src.cc10 &&
              fromBits->cc11==src.cc11);
        // Exact byte mutation must not bypass CRC.
        for(std::size_t i=0;i<data.size();++i) {
            auto mutated=data;
            mutated[i]^=0x01u;
            CHECK(!decode(mutated));
        }
    }
    CHECK(!unpack(kNoSelection));
    CHECK(!unpack(0x0001000000000000ull));
    CHECK(!unpack(pack(BankSelection{129u,0u,0u,0u,0u})));
    CHECK(!unpack(pack(BankSelection{0u,128u,0u,0u,0u})));
    CHECK(!unpack(pack(BankSelection{0u,0u,128u,0u,0u})));
    CHECK(!unpack(pack(BankSelection{0u,0u,0u,128u,0u})));
    CHECK(!unpack(pack(BankSelection{0u,0u,0u,0u,128u})));
    CHECK(!decode(encode(BankSelection{129u,0u,0u,0u,0u})));
    std::puts("P003_VST3_STATE_CRC_LEGACY_VERSIONED_ATOMIC_PASS");
    return 0;
}
