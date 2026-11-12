#include "CreativeCandidateRegister.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(x) do { if(!(x)) { std::cerr<<"P002_CANDIDATE_FAIL line "<<__LINE__<<" "<<#x<<"\n";return 1;} } while(0)
int main() {
    // Parameters are EXPLICIT input, NOT a built-in GM program switch:
    // observed GM0 example: combined term 16, unchanged cutoff 178.
    constexpr CandidateVoiceParameters a{16,0,178,false};
    // observed GM40 example: combined term 13, base cutoff 210,
    // velocity filter active above 70. Bank decoding is NOT completed.
    constexpr CandidateVoiceParameters b{13,0,210,true};
    CHECK(candidateInitialIfatn(0,a)==std::nullopt);
    CHECK(candidateInitialIfatn(1,a)->word==0xB2AD);
    CHECK(candidateInitialIfatn(64,a)->word==0xB248);
    CHECK(candidateInitialIfatn(127,a)->word==0xB22A);
    CHECK(candidateInitialIfatn(1,b)->word==0x74A5);
    CHECK(candidateInitialIfatn(70,b)->cutoff==116);
    CHECK(candidateInitialIfatn(71,b)->cutoff==117);
    CHECK(candidateInitialIfatn(100,b)->cutoff==165);
    CHECK(candidateInitialIfatn(127,b)->word==0xD122);
    // Original Creative driver code (SBAWE.VXD) confirms: melodic bank
    // and encoded volume attack below 0x7D activates velocity cutoff.
    // Raw SoundFont generator 34 is NOT interchangeable with this byte.
    CHECK(candidateOriginalIfatnFromEncodedAttack(90,b,0,124)->cutoff==148);
    CHECK(candidateOriginalIfatnFromEncodedAttack(90,b,0,125)->cutoff==210);
    CHECK(candidateOriginalIfatnFromEncodedAttack(90,b,128,10)->cutoff==210);
    CHECK(!candidateOriginalIfatnFromEncodedAttack(90,b,0,128));
    CHECK(!candidateOriginalIfatnFromEncodedAttack(0,b,0,10));
    // Explicitly calculated parameter combinations are not
    // hardcoded program-specific gain or filter exceptions.
    const CandidateVoiceParameters alternate{13,8,210,true};
    CHECK(candidateInitialIfatn(64,alternate)->attenuation ==
          candidateInitialIfatn(64,b)->attenuation+8);
    unsigned checked=0;
    std::uint8_t lastA=255,lastB=255;
    for(unsigned v=1;v<=127;++v) {
        const auto x=candidateInitialIfatn(static_cast<std::uint8_t>(v),a);
        const auto y=candidateInitialIfatn(static_cast<std::uint8_t>(v),b);
        CHECK(x && y);
        CHECK(x->cutoff==178);
        CHECK(y->cutoff==((std::max(70u,v)*210u+160u)>>7));
        CHECK(x->attenuation == y->attenuation+8u);
        CHECK(x->attenuation<=lastA && y->attenuation<=lastB);
        lastA=x->attenuation;lastB=y->attenuation;
        // Actual MIDI event => candidate IFATN port writes for an explicitly
        // nominated 0..31 voice. No trace playback; no file IO.
        const auto msg=decodeWinMmShort(0x90u | (60u<<8) | (v<<16));
        CHECK(msg && msg->kind==MidiKind::NoteOn);
        const auto writes=candidateInitialIfatnWrites(*msg,static_cast<std::uint8_t>(v%32),a);
        CHECK(writes);
        CHECK((*writes)[0].port==0xE22);
        CHECK((*writes)[0].value==(0x20u|v%32u));
        CHECK((*writes)[1].port==0xE20);
        CHECK((*writes)[1].value==x->word);
        ++checked;
    }
    const auto off=decodeWinMmShort(0x90u|(60u<<8)); // velocity 0
    CHECK(off && off->kind==MidiKind::NoteOff);
    CHECK(!candidateInitialIfatnWrites(*off,1,a));
    const auto on=decodeWinMmShort(0x90u|(60u<<8)|(100u<<16));
    CHECK(on && !candidateInitialIfatnWrites(*on,32,a));
    CHECK(on && !candidateInitialIfatnWrites(*on,255,a));
    const auto saturated=candidateInitialIfatn(1,CandidateVoiceParameters{255,255,255,true});
    CHECK(saturated && saturated->attenuation==255 && saturated->cutoff==140);
    std::cout<<"P002_CANDIDATE_NATIVE_MIDI_TO_IFATN_CONTRACT_PASS"
             <<" velocity_cases="<<checked
             <<" no_GM_program_hardcode=TRUE"
             <<" independent_complete_Creative_register_parity=NOT_PROVEN\n";
}
