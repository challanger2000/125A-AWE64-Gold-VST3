// P007AM public unit tests: every original Creative MIDI note may use a
// different EMU8000 voice. Reproduces the exact pointer/port encoding from
// owner-original P007AL observations without copying private source traces.
#include "P007ALOriginalChipRegisterProof.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

using p007::original_chip_register_proof::Capture;
static void require(bool ok,const char* description) {
    if(!ok) {
        std::fprintf(stderr,"P007AM_REGISTER_DECODER_FAIL %s\n",description);
        std::exit(1);
    }
}
static void sendNote(Capture& c,unsigned voice,unsigned filterRelease=0x807fu) {
    c.observe(0x0e22u,static_cast<unsigned short>(0x40u|voice));
    c.observe(0x0e20u,0x00c0u); // original Creative NRPN24 +96
    c.observe(0x0e22u,static_cast<unsigned short>(0xa0u|voice));
    c.observe(0x0a20u,0x0003u); // not note-off; do not interpret as release
    c.observe(0x0a20u,0x8001u); // original Creative NRPN15 1800
    c.observe(0x0e22u,static_cast<unsigned short>(0xe0u|voice));
    c.observe(0x0a20u,static_cast<unsigned short>(filterRelease));
}
int main() {
    for(const unsigned voice :{0u,3u,6u,9u,12u,31u}) {
        Capture baseline{};
        sendNote(baseline,voice);
        require(baseline.complete(),"original voice 0..31 must be decodable");
        require(baseline.voice==voice,"voice ID lost");
        require(baseline.pefe==0x00c0u,"PEFE signed polarity wrongly decoded");
        require(baseline.volumeRelease==0x8001u,
                "Creative volume release decoding failed");
        require(baseline.filterRelease==0x807fu,
                "Creative filter release decoding failed");
        Capture changed{};
        sendNote(changed,voice,0x8002u);
        require(changed.complete() && changed.filterRelease!=baseline.filterRelease,
                "changed original modulation release not detected");
    }
    Capture missing{};
    missing.observe(0x0e22u,0xe3u);
    missing.observe(0x0a20u,0x807fu);
    require(!missing.complete(),"missing PEFE/AMP wrongly accepted");
    Capture mixed{};
    sendNote(mixed,0u);
    mixed.observe(0x0e22u,0xe3u);
    mixed.observe(0x0a20u,0x807fu);
    require(!mixed.complete() && mixed.conflict,
            "simultaneous voices must not be conflated");
    Capture duplicate{};
    sendNote(duplicate,6u);
    duplicate.observe(0x0e22u,0x46u);
    duplicate.observe(0x0e20u,0x0040u);
    require(!duplicate.complete(),"conflicting PEFE writes wrongly accepted");
    Capture noRelease{};
    noRelease.observe(0x0e22u,0x46u);
    noRelease.observe(0x0e20u,0x00c0u);
    noRelease.observe(0x0e22u,0xa6u);
    noRelease.observe(0x0a20u,0x0003u);
    noRelease.observe(0x0e22u,0xe6u);
    noRelease.observe(0x0a20u,0x007fu);
    require(!noRelease.complete(),"note-on DCYSUS must not count as release");
    std::puts("P007AM_ORIGINAL_REGISTER_DECODER_ALL_VOICES_AND_NEGATIVE_CONTROLS_PASS");
    std::puts("P007AM_PUBLIC_SYNTHETIC_STRUCTURAL_TEST_NOT_ORIGINAL_AUDIO_PROOF");
    return 0;
}
