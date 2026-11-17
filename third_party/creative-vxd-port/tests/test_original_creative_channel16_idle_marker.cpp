#include "OriginalCreativeMidiEngine.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"ORIGINAL_CH16_IDLE_QUIRK_FAIL:%d %s\n",__LINE__,#x);std::exit(1);}}while(false)
int main() {
    MidiEngine e{};
    // All rows start with the exact original 0x0FFE idle marker. Its
    // UPPER BYTE is 0x0F, the hexadecimal index of MIDI channel 16.
    for(auto const& v:e.device.voices)CHECK(v.key_marker==0x0ffeu);
    // LCOD+0x6C82 explicitly compares the *whole* marker to 0x0FFE;
    // CC120/123 must avoid writing/releasing these free physical rows.
    CHECK(dispatch_midi(e,0xbfu,120u,0u,{}, {})==MidiResult::Applied);
    CHECK(dispatch_midi(e,0xbfu,123u,0u,{}, {})==MidiResult::Applied);
    // Unlike 0x6C82, original LCOD+0x6FAC pitchbend and +0x6611
    // modulation use only SHR 8 / channel equality and *do not* skip
    // idle marker 0x0FFE. We must never silently 'fix' these paths by
    // filtering such rows. Unknown original idle-row fields fail closed.
    CHECK(dispatch_midi(e,0xefu,0u,64u,{}, {})==MidiResult::MissingSource);
    CHECK(dispatch_midi(e,0xbfu,1u,64u,{}, {})==MidiResult::MissingSource);
    static constexpr std::array<std::uint8_t,128> zeros{};
    e.volume_source={true,zeros.data(),zeros.data(),0,true,true,false};
    CHECK(dispatch_midi(e,0xbfu,7u,63u,{}, {})==MidiResult::MissingSource);
    CHECK(!e.fault);
    // Channel 15 (0-based 14) doesn't match the idle marker high byte.
    CHECK(dispatch_midi(e,0xeeu,0u,64u,{}, {})==MidiResult::Applied);
    CHECK(dispatch_midi(e,0xbeu,1u,0u,{}, {})==MidiResult::Applied);
    CHECK(dispatch_midi(e,0xbeu,11u,127u,{}, {})==MidiResult::Applied);
    std::puts("P003_ORIGINAL_CH16_IDLE_MARKER_DISPATCH_CONTRACT_PASS");
}
