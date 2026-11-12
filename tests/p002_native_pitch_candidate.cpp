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
    // Exact integer octave/fraction conversion reconstructed from the
    // genuine Win95 Creative SBAWE.VXD helper (private binary, no copied code).
    SbkLayer extreme{};extreme.key=127;extreme.velocity=100;
    extreme.instrument_local.raw[55]=0;
    const auto upperOctave=candidateOriginalRomIp(extreme,127,8192);
    CHECK(upperOctave && upperOctave->ip==0xF955); // 15-octave field saturation
    const auto saturation=candidateOriginalRomIp(extreme,127,16383,2400);
    CHECK(saturation && saturation->ip==0xFFFF); // no wrap to wrong pitch
    extreme.key=0;extreme.instrument_local.raw[55]=0;
    CHECK(candidateOriginalRomIp(extreme,0,8192)->ip==0xE000);
    extreme.instrument_local.raw[55]=1199;
    CHECK(candidateOriginalRomIp(extreme,0,8192)->ip==0xD003);
    extreme.instrument_local.raw[55]=12799;
    CHECK(candidateOriginalRomIp(extreme,0,8192)->ip==0x3558);
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
    // Live original Win95 Creative pitchbend on a sounding GM0 C4.
    note.layers[0].metadata.velocity=100;
    auto bendMinimum=decodeWinMmShort(0x000000e2u);CHECK(bendMinimum);
    // The source pitchbend event has not yet been applied. Reject stale state.
    CHECK(!candidateOriginalCreativeLivePitchBendWrites(
        *bendMinimum,state,note,0,3));
    state.apply(*bendMinimum);
    auto livePitch=candidateOriginalCreativeLivePitchBendWrites(
        *bendMinimum,state,note,0,3);
    CHECK(livePitch && (*livePitch)[0].value==3 &&
          (*livePitch)[1].value==0xC6B8);
    auto bendCenter=decodeWinMmShort(0x004000e2u);CHECK(bendCenter);
    state.apply(*bendCenter);
    livePitch=candidateOriginalCreativeLivePitchBendWrites(*bendCenter,state,note,0,3);
    CHECK(livePitch && (*livePitch)[1].value==0xC962);
    auto bendMaximum=decodeWinMmShort(0x007f7fe2u);CHECK(bendMaximum);
    state.apply(*bendMaximum);
    livePitch=candidateOriginalCreativeLivePitchBendWrites(*bendMaximum,state,note,0,3);
    CHECK(livePitch && (*livePitch)[1].value==0xCC0C);
    auto otherChannel=decodeWinMmShort(0x007f7fe3u);CHECK(otherChannel);
    state.apply(*otherChannel);
    CHECK(!candidateOriginalCreativeLivePitchBendWrites(*otherChannel,state,note,0,3));
    CHECK(!candidateOriginalCreativeLivePitchBendWrites(*on,state,note,0,3));
    CHECK(!candidateOriginalCreativeLivePitchBendWrites(*bendMaximum,state,note,0,30));
    std::cout<<"P002_NATIVE_CREATIVE_IP_CANDIDATE_PASS original_GM0_GM40_C4=2 original_pitchbend=3 no_GM_ID_branches=TRUE\n";
}
