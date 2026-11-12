#include "CreativeVxdSampleAddress.h"
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(x) do{if(!(x)){std::cerr<<"P002_ORIGINAL_VXD_SAMPLE_ADDRESS_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
int main(){
    // Authentic owner 1MGM sample ID 3, numeric only; no original ROM bytes.
    const SbkRomAddress piano{3,0xfeb9u,0x152feu,0x152bau,0x152f8u};
    const auto loop=candidateOriginalVxdSampleAddressUpdate(
        piano,3,OriginalVxdSampleAddressCase::DriverCase5,0x47000055u);
    CHECK(loop);
    CHECK((*loop)[0].port==0xE22 && (*loop)[0].value==0xE3 &&
          (*loop)[0].access_width_bits==8);
    CHECK((*loop)[1].port==0x620 && (*loop)[1].value==0x152F9u &&
          (*loop)[1].access_width_bits==32);
    CHECK((*loop)[2].port==0xE22 && (*loop)[2].value==0xC3 &&
          (*loop)[2].access_width_bits==8);
    CHECK((*loop)[3].port==0x620 && (*loop)[3].value==0x470152BAu &&
          (*loop)[3].access_width_bits==32);
    const auto noLoop=candidateOriginalVxdSampleAddressUpdate(
        piano,3,OriginalVxdSampleAddressCase::DriverCase6,0x8A123456u);
    CHECK(noLoop && (*noLoop)[1].value==0x15306u &&
          (*noLoop)[3].value==0x8A015302u);
    const SbkRomAddress final{151,0x7c845u,0x7fc31u,0x7e567u,0x7fc2bu};
    const auto last=candidateOriginalVxdSampleAddressUpdate(
        final,29,OriginalVxdSampleAddressCase::DriverCase5,0xFFAA0000u);
    CHECK(last && (*last)[0].value==0xFD &&
          (*last)[1].value==0x7FC2Cu &&
          (*last)[3].value==0xFF07E567u);
    CHECK(!candidateOriginalVxdSampleAddressUpdate(
        piano,30,OriginalVxdSampleAddressCase::DriverCase5,0));
    CHECK(!candidateOriginalVxdSampleAddressUpdate(
        piano,31,OriginalVxdSampleAddressCase::DriverCase6,0));
    CHECK(!candidateOriginalVxdSampleAddressUpdate(
        piano,3,static_cast<OriginalVxdSampleAddressCase>(7),0));
    auto invalid=piano;invalid.loop_start_word=invalid.loop_end_word;
    CHECK(!candidateOriginalVxdSampleAddressUpdate(
        invalid,3,OriginalVxdSampleAddressCase::DriverCase5,0));
    invalid=piano;invalid.end_word=0x80001u;
    CHECK(!candidateOriginalVxdSampleAddressUpdate(
        invalid,3,OriginalVxdSampleAddressCase::DriverCase6,0));
    invalid=piano;invalid.loop_end_word=invalid.end_word+1u;
    CHECK(!candidateOriginalVxdSampleAddressUpdate(
        invalid,3,OriginalVxdSampleAddressCase::DriverCase5,0));
    // Actual VXD NoteOn LCOD +0x5B81/+0x5F28/+0x5F58:
    // exact -4 initial ROM pointer, then *separate* filter-Q phase.
    const auto rawCcca=candidateOriginalVxdRomCccaStart(
        piano,3,5,true,OriginalVxdCccaStartPhase::StartWithoutFilterQ);
    CHECK(rawCcca && (*rawCcca)[0].port==0xE22 &&
          (*rawCcca)[0].value==3 &&
          (*rawCcca)[0].access_width_bits==8 &&
          (*rawCcca)[1].port==0xA20 &&
          (*rawCcca)[1].value==0xFEB5u &&
          (*rawCcca)[1].access_width_bits==32);
    const auto qCcca=candidateOriginalVxdRomCccaStart(
        piano,3,5,true,OriginalVxdCccaStartPhase::StartWithEncodedFilterQ);
    CHECK(qCcca && (*qCcca)[1].value==0x5000FEB5u);
    CHECK(!candidateOriginalVxdRomCccaStart(
        piano,3,5,false,OriginalVxdCccaStartPhase::StartWithEncodedFilterQ));
    CHECK(!candidateOriginalVxdRomCccaStart(
        piano,3,16,true,OriginalVxdCccaStartPhase::StartWithEncodedFilterQ));
    CHECK(!candidateOriginalVxdRomCccaStart(
        piano,30,5,true,OriginalVxdCccaStartPhase::StartWithEncodedFilterQ));
    auto badStart=piano;badStart.start_word=3;
    CHECK(!candidateOriginalVxdRomCccaStart(
        badStart,3,5,true,OriginalVxdCccaStartPhase::StartWithEncodedFilterQ));
    CHECK(!candidateOriginalVxdRomCccaStart(
        piano,3,5,true,static_cast<OriginalVxdCccaStartPhase>(255)));
    std::cout<<"P002_ORIGINAL_VXD_1MGM_SAMPLE_ADDRESS_CASE5_CASE6_PASS"
             <<" CSL_offset_plus1_or_plus8=TRUE"
             <<" PSST_preserves_upper_byte=TRUE"
             <<" CCCA_start_minus4_and_Q_highbits=TRUE"
             <<" port_widths_8_32=TRUE original_full_noteon_pcm=NOT_PROVEN\n";
}
