// P007AR: deterministic host block placement checks, *not* a test of
// actual Win95 Creative COM2/WinMM output latency or sample accuracy.
#include "../vst3/P007HostMidiOffset.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
static void check(bool pass,const char* message) {
    if(!pass){std::fprintf(stderr,"P007AR_FAIL %s\n",message);std::exit(1);}
}
int main() {
    using p007::midi_offset::place;
    check(place(0,1024,0)==0u,"zero offset");
    check(place(1,1024,0)==1u,"1-sample offset");
    check(place(512,1024,0)==512u,"mid-block offset");
    check(place(1023,1024,0)==1023u,"last host frame");
    check(place(1024,1024,0)==1024u,"exact block boundary");
    check(place(999999,1024,0)==1024u,"overshoot clamping");
    check(place(-12,1024,0)==0u,"negative offset clamping");
    check(place(90,1024,512)==512u,"unsorted timestamps monotonic");
    check(place(1023,1024,512)==1023u,"later sorted timestamp");
    check(place(0,0,0)==0u,"empty audio block");
    check(place(123,-1,0)==0u,"negative block count");
    check(place(900,1024,2048)==1024u,"render cursor bounded");
    check(place(2147483647,1024,0)==1024u,"max int32 clamped");
    for(const int frames :{1,64,128,256,512,1024,4096}) {
        std::uint32_t rendered=0u;
        for(const int off :{-25,0,1,frames/2,frames-1,frames,frames+25}) {
            const auto next=place(off,frames,rendered);
            check(next>=rendered&&next<=static_cast<unsigned>(frames),
                  "events moved output position backwards/out of bounds");
            rendered=next;
        }
        check(rendered==static_cast<unsigned>(frames),
              "last host event was not clamped to block end");
    }
    std::puts("P007AR_BOUNDED_MONOTONIC_HOST_MIDI_SAMPLE_OFFSET_TEST_PASS");
    std::puts("P007AR_NOT_SAMPLE_ACCURATE_WINDOWS95_GUEST_AUDIO_PROOF");
    return 0;
}
