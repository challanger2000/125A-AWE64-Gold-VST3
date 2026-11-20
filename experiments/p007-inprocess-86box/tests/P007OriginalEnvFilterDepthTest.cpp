#include "P007OriginalEnvFilterDepth.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <array>
using namespace p007::original_filter_env_depth;
int main() {
 unsigned ok=0u;
 for(int signedDepth:{-400,-127,-96,-64,-1,0,1,64,96,127,400}) {
    const auto clamped=std::clamp(signedDepth,-127,127);
    const auto cc=originalCreativeNrpn24(signedDepth);
    assert(cc[0].controller==99u&&cc[0].value==127u);
    assert(cc[1].controller==98u&&cc[1].value==24u);
    assert(cc[2].controller==6u&&cc[3].controller==38u);
    assert(decodeOriginalCreativeSignedValue(cc)==clamped);
    ++ok;
 }
 const auto negative=originalCreativeNrpn24(-96);
 assert(negative[2].value==63u&&negative[3].value==32u);
 const auto positive=originalCreativeNrpn24(96);
 assert(positive[2].value==64u&&positive[3].value==96u);
 const auto zero=originalCreativeNrpn24(0);
 assert(zero[2].value==64u&&zero[3].value==0u);
 assert(decodeOriginalCreativeSignedValue(
          {{{98u,127u},{99u,24u},{6u,64u},{38u,96u}}})==-10000);
 Transmitter t;
 t.arm(-96);
 assert(t.active()&&t.stage()==0u);
 unsigned sends=0u;
 const auto accept=[&](std::uint8_t,std::uint8_t)noexcept{
   ++sends;return true;
 };
 assert(t.tick(accept)&&sends==1u&&t.stage()==1u);
 const auto fail=[&](std::uint8_t,std::uint8_t)noexcept{
    ++sends;return false;
 };
 assert(t.tick(fail)&&sends==2u&&t.stage()==1u); // FIFO refusal
 for(unsigned i=0;i<3u;++i)assert(t.tick(accept));
 assert(sends==5u&&!t.active()&&t.stage()==0u);
 assert(!t.tick(accept)&&sends==5u);
 // Return to underlying SoundFont must use CC121; we do NOT assume
 // a NRPN24 value of 0 equals the source preset's original modulation.
 std::puts("P007AI_ORIGINAL_CREATIVE_SIGNED_NRPN24_15_PROOFS_PASS"
           " negative96=63,32 positive96=64,96"
           " selector_24=YES four_packet_retries=YES");
 return 0;
}
