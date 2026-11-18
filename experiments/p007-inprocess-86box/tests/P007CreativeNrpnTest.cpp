#include "P007CreativeNrpn.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
int main() {
 using namespace p007::creative_nrpn;
 unsigned good=0,bad=0;
 constexpr std::array<std::uint8_t,12> originalIds{
     11,13,14,15,5,7,8,9,21,22,26,25};
 for(unsigned i=0;i<count;++i) {
  assert(lsb[i]==originalIds[i]);
  for(unsigned bps:{0u,1u,2500u,5000u,7500u,9999u,10000u}) {
   const auto bytes=encode(i,static_cast<std::uint16_t>(bps));
   assert(bytes[0].number==99u&&bytes[0].value==127u);
   assert(bytes[1].number==98u&&bytes[1].value==originalIds[i]);
   assert(bytes[2].number==6u&&bytes[3].number==38u);
   const unsigned data=(unsigned(bytes[2].value)<<7u)+bytes[3].value;
   assert(data==8192u+raw(i,static_cast<std::uint16_t>(bps)));
   assert(data>=8192u&&data<=8192u+maximum[i]);
   if(bps==0u)assert(data==8192u);
   ++good;
  }
  unsigned messages=0;
  assert(!submit(i,7200u,[&](std::uint8_t,std::uint8_t) noexcept {
      return ++messages!=2u; // Reject NRPN selector, never send CC6/38
  }));
  assert(messages==2u);
  ++bad;
  messages=0;
  assert(submit(i,7200u,[&](std::uint8_t,std::uint8_t) noexcept {
      ++messages;return true;
  }));
  assert(messages==4u);
  ++good;
 }
 assert(!submit(12u,5000u,[](auto,auto) noexcept {return true;}));
 assert(raw(99u,5000u)==0u);
 assert(raw(8u,65535u)==127u);
 assert(raw(10u,10000u)==255u && raw(11u,10000u)==255u);
 std::printf("P007AA_CREATIVE_NRPN_12_REAL_GENERATORS_PASS positives=%u rejections=%u\n",
             good,bad+1u);
 return 0;
}
