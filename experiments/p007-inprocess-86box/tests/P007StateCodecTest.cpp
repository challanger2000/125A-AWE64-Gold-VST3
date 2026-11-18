#include "P007StateCodec.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <limits>
int main() {
 using namespace p007::state;
 unsigned good=0,rejected=0,legacy=0;
 for(unsigned bank:{0u,1u,64u,127u})
  for(unsigned program:{0u,1u,40u,127u})
   for(unsigned master:{0u,1u,5000u,9999u,10000u}) {
    Values expected{static_cast<std::uint8_t>(bank),
      static_cast<std::uint8_t>(program),static_cast<std::uint16_t>(master)};
    for(unsigned i=0;i<12u;++i)
     expected.modulation[i]=static_cast<std::uint16_t>(
       (bank*59u+program*31u+master+i*701u)%10001u);
    const Image bytes=encode(expected);
    assert(bytes.size()==44u&&bytes[4]==2u);
    Values decoded{};
    assert(decode(bytes,decoded));
    assert(decoded.bank==bank&&decoded.program==program&&decoded.master==master);
    assert(decoded.modulation==expected.modulation);
    ++good;
    for(unsigned j=0;j<bytes.size();++j) {
      Image broken=bytes;broken[j]^=1u;
      Values sentinel{21u,22u,2222u};
      sentinel.modulation[0]=5511u;
      assert(!decode(broken,sentinel));
      assert(sentinel.bank==21u&&sentinel.program==22u&&
             sentinel.master==2222u&&sentinel.modulation[0]==5511u);
      ++rejected;
    }
    const auto old=encodeLegacy(expected);
    Values migrated=expected;
    assert(decode(old,migrated));
    assert(migrated.bank==bank&&migrated.program==program&&migrated.master==master);
    for(const auto m:migrated.modulation)assert(m==0u);
    ++legacy;
   }
 assert(masterToBasisPoints(.5f)==5000u);
 assert(masterToBasisPoints(0.f)==0u);
 assert(masterToBasisPoints(1.f)==10000u);
 assert(normalizedToBasisPoints(0.2525)==2525u);
 assert(normalizedToBasisPoints(-1.)==0u);
 assert(normalizedToBasisPoints(2.)==10000u);
 assert(normalizedToBasisPoints(std::numeric_limits<double>::quiet_NaN())==0u);
 std::printf("P007AA_PROJECT_RECALL_V2_AND_LEGACY_V1_PASS v2=%u legacy=%u corruptions=%u\n",
             good,legacy,rejected);
 return 0;
}
