#include "P007StateCodec.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
 using namespace p007::state;
 unsigned cases=0,corruption=0;
 for(unsigned b:{0u,1u,64u,127u})
  for(unsigned p:{0u,1u,40u,127u})
   for(unsigned m:{0u,1u,5000u,9999u,10000u}){
    const Values original{static_cast<std::uint8_t>(b),
             static_cast<std::uint8_t>(p),static_cast<std::uint16_t>(m)};
    const Image bytes=encode(original);
    Values got{};assert(decode(bytes,got));
    assert(got.bank==b&&got.program==p&&got.master==m);++cases;
    for(unsigned i=0;i<bytes.size();++i) {
      Image damaged=bytes;damaged[i]^=1u;
      Values sentinel{21u,22u,2222u};
      assert(!decode(damaged,sentinel));
      assert(sentinel.bank==21u&&sentinel.program==22u&&
             sentinel.master==2222u);
      ++corruption;
    }
   }
 assert(masterToBasisPoints(0.f)==0u);
 assert(masterToBasisPoints(.5f)==5000u);
 assert(masterToBasisPoints(1.f)==10000u);
 assert(masterToBasisPoints(-1.f)==0u);
 assert(masterToBasisPoints(2.f)==10000u);
 std::printf("P007V_PROJECT_STATE_ROUNDTRIP_PASS cases=%u tamper_rejections=%u\n",
             cases,corruption);
 return 0;
}
