#include "OriginalCreativeMidiEngine.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(v) do{if(!(v)){std::fprintf(stderr,"RPN_FAIL %d %s\n",__LINE__,#v);std::exit(1);}}while(false)
int main() {
    MidiEngine e{};
    const PortIo io{}; const SoundFontSource source{};
    auto msg=[&](std::uint8_t cc,std::uint8_t value){
        return dispatch_midi(e,0xb3u,cc,value,io,source);
    };
    CHECK(msg(6,12)==MidiResult::Unsupported); // no RPN selected
    CHECK(msg(101,0)==MidiResult::Applied);
    CHECK(msg(100,0)==MidiResult::Applied);
    CHECK(e.channel[3].original_rpn_mode==0x0100u &&
          e.channel[3].original_rpn_number==0u);
    CHECK(msg(6,12)==MidiResult::Applied);
    CHECK(e.channel[3].original_bend_range==12u);
    CHECK(msg(100,2)==MidiResult::Applied);
    CHECK(msg(6,0)==MidiResult::Applied);
    CHECK(e.channel[3].original_coarse_tune_cents==-2400 &&
          e.channel[3].original_coarse_tune_verified);
    CHECK(msg(6,127)==MidiResult::Applied);
    CHECK(e.channel[3].original_coarse_tune_cents==2400);
    CHECK(msg(100,1)==MidiResult::Applied);
    CHECK(msg(38,64)==MidiResult::Applied);
    CHECK(e.channel[3].original_rpn_data_lsb==64u);
    CHECK(msg(6,70)==MidiResult::Applied);
    CHECK(e.channel[3].original_fine_tune_cents==10 &&
          e.channel[3].original_fine_tune_verified);
    CHECK(msg(38,0)==MidiResult::Applied);
    CHECK(msg(6,0)==MidiResult::Applied);
    CHECK(e.channel[3].original_fine_tune_cents==-100);
    CHECK(msg(101,127)==MidiResult::Applied);
    CHECK(msg(100,127)==MidiResult::Applied);
    CHECK(e.channel[3].original_rpn_number==0x7f7fu);
    CHECK(msg(6,66)==MidiResult::Unsupported);
    CHECK(msg(32,64)==MidiResult::Unsupported); // CC32 not dispatched by VXD
    std::puts("P003_ORIGINAL_WIN95_RPN_CC100_101_6_38_TEST_PASS");
}
