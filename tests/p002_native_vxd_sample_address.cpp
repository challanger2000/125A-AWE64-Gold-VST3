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
    // Actual NOTE-ON LCOD 5B81..5BE4, 5EEB..5F1B (distinct
    // from the earlier case5/6 UPDATE path).
    OriginalVxdRomNoteStartMode loopMode{};
    loopMode.original_driver_mode_verified=true;
    loopMode.encoded_mode_flags=3; // loop with retained end for NoteOff
    loopMode.original_pan_byte=0x47;
    loopMode.original_chorus_send_byte=0x53;
    const auto geometry=candidateOriginalVxdInitialRomNoteGeometry(
        piano,3,loopMode);
    CHECK(geometry && geometry->driver_retained_release_end==0x15302u);
    CHECK(geometry->loop_address_ports[0].value==0xC3 &&
          geometry->loop_address_ports[1].value==0x470152B9u);
    CHECK(geometry->loop_address_ports[2].value==0xE3 &&
          geometry->loop_address_ports[3].value==0x530152F7u);
    CHECK(geometry->loop_address_ports[1].access_width_bits==32 &&
          geometry->loop_address_ports[3].access_width_bits==32);
    loopMode.encoded_mode_flags=1; // loop only
    CHECK(candidateOriginalVxdInitialRomNoteGeometry(
        piano,3,loopMode)->driver_retained_release_end==0);
    loopMode.encoded_mode_flags=0; // one-shot
    const auto oneShot=candidateOriginalVxdInitialRomNoteGeometry(
        piano,3,loopMode);
    CHECK(oneShot && oneShot->driver_retained_release_end==0 &&
          oneShot->loop_address_ports[1].value==0x47015302u &&
          oneShot->loop_address_ports[3].value==0x53015306u);
    loopMode.encoded_mode_flags=0x80u; // unspecified driver flag
    CHECK(!candidateOriginalVxdInitialRomNoteGeometry(piano,3,loopMode));
    loopMode.encoded_mode_flags=1;loopMode.original_driver_mode_verified=false;
    CHECK(!candidateOriginalVxdInitialRomNoteGeometry(piano,3,loopMode));
    loopMode.original_driver_mode_verified=true;
    CHECK(!candidateOriginalVxdInitialRomNoteGeometry(piano,30,loopMode));

    // ACTUAL original owner RAM-SF2 sample 82 (Saw_a1):
    // shdr start 260287, loopStart 260296, loopEnd 260697;
    // measured Win95 source RAM pool base 0x200010.
    // The genuine driver selected voices 0/3 for C4, 6/9 for D4
    // and wrote the SAME verified RAM geometry, not the old ROM note.
    for(const auto physical : {0u,3u,6u,9u}) {
        const auto verified=candidateOriginalVxdInitialVerifiedRamNoteGeometry(
            0x23F8CFu,0x23FB29u,0x23F8D8u,0x23FA69u,
            static_cast<std::uint8_t>(physical),8u,0x7Fu,0x7Fu,true,true);
        CHECK(verified);
        CHECK(verified->plain_ccca[0].port==0xE22u &&
              verified->plain_ccca[0].value==physical &&
              verified->plain_ccca[0].access_width_bits==8u);
        CHECK(verified->plain_ccca[1].port==0xA20u &&
              verified->plain_ccca[1].value==0x23F8CBu &&
              verified->plain_ccca[1].access_width_bits==32u);
        CHECK(verified->filtered_ccca[1].value==0x8023F8CBu);
        CHECK(verified->loop_address_ports[0].port==0xE22u &&
              verified->loop_address_ports[0].value==(0xC0u|physical));
        CHECK(verified->loop_address_ports[1].port==0x620u &&
              verified->loop_address_ports[1].value==0x7F23F8D7u &&
              verified->loop_address_ports[1].access_width_bits==32u);
        CHECK(verified->loop_address_ports[2].value==(0xE0u|physical));
        CHECK(verified->loop_address_ports[3].value==0x7F23FA68u);
    }
    const auto ramSource=[](std::uint32_t start,std::uint32_t end,
                            std::uint32_t loopStart,std::uint32_t loopEnd,
                            std::uint8_t voice,std::uint8_t q,
                            bool placement,bool driver) {
        return candidateOriginalVxdInitialVerifiedRamNoteGeometry(
            start,end,loopStart,loopEnd,voice,q,0x7Fu,0x7Fu,
            placement,driver).has_value();
    };
    CHECK(!ramSource(0x23F8CFu,0x23FB29u,0x23F8D8u,0x23FA69u,
                     0u,8u,false,true));
    CHECK(!ramSource(0x23F8CFu,0x23FB29u,0x23F8D8u,0x23FA69u,
                     0u,8u,true,false));
    CHECK(!ramSource(0x23F8CFu,0x23FB29u,0x23F8D8u,0x23FA69u,
                     30u,8u,true,true));
    CHECK(!ramSource(0x23F8CFu,0x23FB29u,0x23F8D8u,0x23FA69u,
                     0u,16u,true,true));
    CHECK(!ramSource(0x001000u,0x002000u,0x001100u,0x001200u,
                     0u,8u,true,true)); // ROM is not sample RAM
    CHECK(!ramSource(0x23F8CFu,0x23FB29u,0x23FA69u,0x23F8D8u,
                     0u,8u,true,true)); // reversed loop
    CHECK(!ramSource(0x23F8CFu,0x1000001u,0x23F8D8u,0x23FA69u,
                     0u,8u,true,true)); // outside 28 MiB chip RAM
    CHECK(!ramSource(0x200003u,0x200100u,0x200010u,0x2000A0u,
                     0u,8u,true,true)); // -4 outside RAM
    std::cout<<"P002_ORIGINAL_VXD_1MGM_SAMPLE_ADDRESS_CASE5_CASE6_PASS"
             <<" CSL_offset_plus1_or_plus8=TRUE"
             <<" PSST_preserves_upper_byte=TRUE"
             <<" CCCA_start_minus4_and_Q_highbits=TRUE"
             <<" initial_NoteOn_PSST_CSL_real_geometry=TRUE"
             <<" source_RAM_sample82_all_four_real_voice_geometry=PASS"
             <<" RAM_full_noteon_PCM=NOT_PROVEN"
             <<" port_widths_8_32=TRUE original_full_noteon_pcm=NOT_PROVEN\n";
}
