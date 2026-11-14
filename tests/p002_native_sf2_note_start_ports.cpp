#include "CreativeSf2NoteStartPorts.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(x) do {if(!(x)){std::cerr<<"P002_SF2_NOTE_PREFIX_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
namespace {
void set(CreativeSf2VoiceDescriptorBytes& b,unsigned gen,std::int16_t v){
    const auto n=static_cast<std::uint16_t>(v);
    b[gen*2u]=static_cast<std::uint8_t>(n);
    b[gen*2u+1u]=static_cast<std::uint8_t>(n>>8);
}
}
int main(){
    // Field-level, public numerical reference to original VXD GM0.
    // PRIVATE original SF2/driver byte arrays remain off GitHub.
    CreativeSf2VoiceDescriptorBytes b{};
    set(b,58,77); // original GM0 root note
    set(b,56,100);set(b,52,4);
    set(b,8,179);set(b,19,58); // original IFATN=B33A
    set(b,34,125);set(b,35,120); // ATKHLDV=787D
    set(b,21,static_cast<std::int16_t>(0x8000u));
    set(b,26,125);set(b,27,127); // ATKHLD=7F7D
    set(b,28,127);set(b,29,0); // DCYSUS=007F
    set(b,23,static_cast<std::int16_t>(0x8000u));
    b[0x88u]=0xe0u; // 44.1kHz original dwPitch = 00E00000
    b[0x8au]=60u; // shdr original root
    b[0x8cu]=1u; // 0x8001 has been converted by NOTE START
    const auto ports=candidateOriginalSf2NoteStartRegisterPrefix(
        b,60u,0u,true,0,0);
    CHECK(ports && ports->size()==18u);
    // ACTUAL GM0 VXD trace events 5..22, not a rearranged register set.
    // Need pointer/word widths and sequence to match each physical OUT.
    constexpr std::array<OriginalWin95VxdPortWrite,18> gm0{{
        {0xe22u,0x60u,8},{0x620u,0x0000ffffu,32},
        {0xe22u,0x40u,8},{0x620u,0x0000ffffu,32},
        {0xe22u,0x80u,8},{0xa22u,0x787du,16},
        {0xe22u,0xa0u,8},{0xa22u,0x8000u,16},
        {0xe22u,0xc0u,8},{0xa22u,0x7f7du,16},
        {0xe22u,0xe0u,8},{0xa20u,0x007fu,16},
        {0xe22u,0xe0u,8},{0xa22u,0x8000u,16},
        {0xe22u,0x00u,8},{0xe20u,0xc961u,16},
        {0xe22u,0x20u,8},{0xe20u,0xb33au,16}
    }};
    for(unsigned i=0;i<gm0.size();++i){
        CHECK((*ports)[i].port==gm0[i].port);
        CHECK((*ports)[i].value==gm0[i].value);
        CHECK((*ports)[i].access_width_bits==gm0[i].access_width_bits);
    }

    // Same source-derived fields on the independently ASSIGNED physical
    // voice3 must change selectors, never confuse MIDI channel with voice.
    const auto voice3=candidateOriginalSf2NoteStartRegisterPrefix(
        b,60u,3u,true,0,0);
    CHECK(voice3);
    for(unsigned i=0;i<18u;i+=2) {
        CHECK((*voice3)[i].port==0xe22u);
        CHECK((*voice3)[i].value==gm0[i].value+3u);
        CHECK((*voice3)[i+1].value==gm0[i+1].value);
    }

    // Original SF2 GM100 special attack=128 remains 128 in descriptor,
    // but physical ATKHLDV low byte must be 127. Original IFATN=FF2A.
    auto attack128=b;
    set(attack128,34,128);
    set(attack128,8,255);set(attack128,19,42);
    const auto slow=candidateOriginalSf2NoteStartRegisterPrefix(
        attack128,60u,3u,true,0,0);
    CHECK(slow && (*slow)[5].value==0x787fu);
    CHECK((*slow)[17].value==0xff2au);
    CHECK((*slow)[15].value==0xc961u);

    // The prefix requires a SOURCE-VERIFIED *post*-start 0x94 record.
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(b,60,0,false,0,0));
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(b,128,0,true,0,0));
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(b,60,30,true,0,0));
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(b,60,31,true,0,0));
    auto invalid=b;
    invalid[0x8du]=0x80u; // pre-note ROM qualifier 0x8001
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(invalid,60,0,true,0,0));
    invalid=b;set(invalid,8,256);
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(invalid,60,0,true,0,0));
    invalid=b;set(invalid,19,256);
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(invalid,60,0,true,0,0));
    invalid=b;set(invalid,34,129);
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(invalid,60,0,true,0,0));
    invalid=b;set(invalid,26,129);
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(invalid,60,0,true,0,0));
    invalid=b;set(invalid,28,128);
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(invalid,60,0,true,0,0));
    invalid=b;set(invalid,29,128);
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(invalid,60,0,true,0,0));
    invalid=b;invalid[0x89u]=1u; // pitch >24-bit documented bound
    CHECK(!candidateOriginalSf2NoteStartRegisterPrefix(invalid,60,0,true,0,0));

    std::cout<<"P002_ORIGINAL_SF2_18_TYPED_NOTE_START_PORTS_PASS "
             <<"actual_gm0_18_of_18=TRUE original_attack128_to_hw127=TRUE "
             <<"physical_voice0_and3=TRUE unsupported_fail_closed=TRUE "
             <<"full_noteon_timing_pcm=NOT_PROVEN\n";
    return 0;
}
