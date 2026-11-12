#include "CreativePitchCandidate.h"
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(x) do{ if(!(x)){std::cerr<<"P002_CREATIVE_IP_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;} }while(0)
int main() {
    SbkLayer piano{};piano.key=60;piano.velocity=100;piano.program=0;
    piano.instrument_local.raw[55]=7696; // measured private GM0 C4 bank input
    const auto p=candidateOriginalRomIp(piano,60,8192);
    CHECK(p && p->ip==0xC962 && p->source_pitch_raw==7696);
    CHECK(candidateOriginalRomIp(piano,60,0)->ip==0xC6B8);
    CHECK(candidateOriginalRomIp(piano,60,16383)->ip==0xCC0C);
    CHECK(candidateOriginalRomIp(piano,60,8192)->ip==0xC962);
    // The independently measured GM40 C4 original IP was BC66.
    SbkLayer violin{};violin.key=60;violin.velocity=90;violin.program=40;
    violin.instrument_global.raw[55]=8670;
    const auto v=candidateOriginalRomIp(violin,60,8192);
    CHECK(v && v->ip==0xBC66);
    CHECK(!candidateOriginalRomIp(violin,61,8192)); // stale selected key
    CHECK(!candidateOriginalRomIp(violin,60,16384));
    CHECK(!candidateOriginalRomIp(violin,60,8192,2401));
    violin.instrument_global.raw[51]=1;
    CHECK(!candidateOriginalRomIp(violin,60,8192)); // unknown coarse tune
    violin.instrument_global.raw[51].reset();
    violin.instrument_global.raw[58]=60;
    CHECK(!candidateOriginalRomIp(violin,60,8192)); // unknown root override
    violin.instrument_global.raw[58].reset();
    violin.instrument_global.raw[55].reset();
    CHECK(!candidateOriginalRomIp(violin,60,8192)); // never invent source
    CreativeRomNotePlan note{};note.channel=2;note.program=0;note.key=60;
    note.velocity=100;note.original_sbk_bank=0;note.count=1;
    note.layers[0].metadata=piano;
    MidiState state;
    const auto writes=candidateOriginalRomIpWrites(note,0,3,state);
    CHECK(writes && (*writes)[0].port==0xE22 &&
          (*writes)[0].value==3 && (*writes)[1].port==0xE20 &&
          (*writes)[1].value==0xC962);
    auto bend=decodeWinMmShort(0x007f7fe2u);CHECK(bend);
    state.apply(*bend); // raw WinMM channel 2 pitchbend max
    const auto upper=candidateOriginalRomIpWrites(note,0,3,state);
    CHECK(upper && (*upper)[1].value==0xCC0C);
    CHECK(!candidateOriginalRomIpWrites(note,0,30,state));
    CHECK(!candidateOriginalRomIpWrites(note,1,3,state));
    note.layers[0].metadata.key=61;
    CHECK(!candidateOriginalRomIpWrites(note,0,3,state));
    note.layers[0].metadata.key=60;
    note.layers[0].metadata.velocity=99;
    CHECK(!candidateOriginalRomIpWrites(note,0,3,state));
    std::cout<<"P002_NATIVE_CREATIVE_IP_CANDIDATE_PASS original_GM0_GM40_C4=2 original_pitchbend=3 no_GM_ID_branches=TRUE\n";
}
