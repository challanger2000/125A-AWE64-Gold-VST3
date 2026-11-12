#include "CreativeChipPortBridge.h"
#include "CreativePitchCandidate.h"
#include "CreativeControllerVolume.h"
#include "CreativeVxdSampleAddress.h"
#include "CreativeVxdNoteRelease.h"
#include "CreativeVxdVoiceEnvelopePrelude.h"
#include <86box/snd_emu8k.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
using namespace awe64;
using namespace awe64::creative;
#define CHECK(expr) do{if(!(expr)){std::cerr<<"P002_REAL_CHIP_PORT_BRIDGE_FAIL line="<<__LINE__<<" "<<#expr<<"\n";return 1;}}while(0)
int main(){
    // Completely synthetic ROM. The chip itself is the actual locked
    // original 86Box-AWE32 source, not a synthesized test mock.
    const auto rom=std::filesystem::temp_directory_path()/
        "p002-noncreative-synthetic-chip-port-test.raw";
    {
        std::ofstream out(rom,std::ios::binary|std::ios::trunc);
        CHECK(bool(out));
        const std::array<char,4096> zero{};
        for(int i=0;i<256;++i)out.write(zero.data(),zero.size());
        CHECK(bool(out));
    }
    Chip chip;
    std::string err;
    CHECK(chip.open(rom,err));
    CHECK(chip.ready());
    CHECK(chip.rawForContractTesting()->ram_end_addr==0x1000000u);
    const auto selected=candidateOriginalWin95SelectedVoiceDcysusvWrites(
        OriginalWin95MonoSelection{
            3,OriginalWin95MonoSelectionReason::CompletelyIdle,0});
    const auto vtft=candidateOriginalWin95AssignedVoiceVtftWrites(3);
    CHECK(selected && vtft);
    std::array<OriginalWin95VxdPortWrite,4> start{
        (*selected)[0],(*selected)[1],(*vtft)[0],(*vtft)[1]};
    CHECK(applyOriginalCreativeVxdVoicePorts(chip,start));
    const auto* real=chip.rawForContractTesting();
    CHECK(real->voice[3].dcysusv==0x00ffu);
    CHECK(real->voice[3].vtft==0x0000ffffu);
    // Real Windows95 VXD +0x5BE4..+0x5CA4 first seven NOTE START
    // register writes; already encoded original 0x94-byte source fields.
    OriginalVxdEncodedVoiceEnvelopePrelude envelope{};
    envelope.from_actual_original_driver_record=true;
    envelope.attack_hold_volume_low_byte_word=0x0080u;
    envelope.attack_hold_volume_high_word=0x56;
    envelope.lfo1_delay_word=static_cast<std::int16_t>(0x8000u);
    envelope.attack_hold_mod_low_byte_word=0x0080u;
    envelope.attack_hold_mod_high_word=0x34;
    envelope.decay_sustain_mod_low_byte=0x12;
    envelope.decay_sustain_mod_high_word=0x45;
    envelope.lfo2_delay_word=static_cast<std::int16_t>(0x8010u);
    const auto startEnvelope=candidateOriginalVxdVoiceEnvelopePrelude(3,envelope);
    CHECK(startEnvelope && startEnvelope->size()==14);
    CHECK(applyOriginalCreativeVxdVoicePorts(chip,*startEnvelope));
    CHECK(real->voice[3].vtft==0x0000ffffu);
    CHECK(real->voice[3].cvcf==0x0000ffffu);
    CHECK(real->voice[3].atkhldv==0x567fu);
    CHECK(real->voice[3].lfo1val==0x8000u);
    CHECK(real->voice[3].atkhld==0x347fu);
    CHECK(real->voice[3].dcysus==0x4512u);
    CHECK(real->voice[3].lfo2val==0x8010u);
    // Ensure no other physical voice was changed by the original prelude.
    CHECK(real->voice[4].dcysusv!=0x00ffu ||
          real->voice[4].vtft!=0x0000ffffu);
    // A private guest's real VXD 4AB5 group stop has 16-bit IFATN
    // then 16-bit DCYSUSV release phase; actual pin-locked chip must
    // reflect those register words, not merely an array equality test.
    const auto stop=candidateOriginalWin95VxdGroupChokeWrites(
        3,OriginalVxdGroupChokeEvidence{true,false,false});
    CHECK(stop && stop->count==4);
    CHECK(applyOriginalCreativeVxdVoicePorts(
        chip,std::span<const OriginalWin95VxdPortWrite>(
            stop->writes.data(),stop->count)));
    CHECK(real->voice[3].ifatn==0x00ffu);
    CHECK(real->voice[3].dcysusv==0x807fu);
    const auto stereo=candidateOriginalWin95VxdGroupChokeWrites(
        6,OriginalVxdGroupChokeEvidence{true,true,true});
    CHECK(stereo && stereo->count==8);
    CHECK(applyOriginalCreativeVxdVoicePorts(
        chip,std::span<const OriginalWin95VxdPortWrite>(
            stereo->writes.data(),stereo->count)));
    CHECK(real->voice[6].ifatn==0xffu && real->voice[7].ifatn==0xffu);
    CHECK(real->voice[6].dcysusv==0x807fu &&
          real->voice[7].dcysusv==0x807fu);
    // Live original WinMM note program/key + original 1MGM pitch field
    // + decoded Creative Win95 encoded volume/controller rules are now
    // actually written into the pinned original EMU8000 chip.
    CreativeRomNotePlan piano{};
    piano.channel=0;piano.program=0;piano.key=60;piano.velocity=96;
    piano.original_sbk_bank=0;piano.count=1;
    piano.layers[0].metadata.key=60;
    piano.layers[0].metadata.velocity=96;
    piano.layers[0].metadata.program=0;
    piano.layers[0].metadata.bank=0;
    piano.layers[0].metadata.instrument_local.raw[55]=7696;
    MidiState state;
    OriginalCreativeIfatnTerms encoded{};
    encoded.bank=0; encoded.initial_cutoff=178;
    encoded.encoded_volume_attack_rate=125;
    encoded.volume={100,127,16,0,true};
    const auto message=decodeWinMmShort(0x00603c90u);
    CHECK(message && message->kind==MidiKind::NoteOn);
    const auto ip=candidateOriginalRomIpWrites(piano,0,3,state);
    const auto ifatn=candidateOriginalCreativeIfatnWrites(
        *message,3,state,encoded);
    CHECK(ip && ifatn);
    const std::array<OriginalWin95VxdPortWrite,4> noteParameters{{
        {(*ip)[0].port,(*ip)[0].value,8},
        {(*ip)[1].port,(*ip)[1].value,16},
        {(*ifatn)[0].port,(*ifatn)[0].value,8},
        {(*ifatn)[1].port,(*ifatn)[1].value,16}
    }};
    CHECK(applyOriginalCreativeVxdVoicePorts(chip,noteParameters));
    CHECK(real->voice[3].ip==0xC962);
    const auto expected=candidateOriginalCreativeIfatn(96,encoded);
    CHECK(expected && real->voice[3].ifatn==expected->word);
    CHECK(real->voice[3].ifatn_init_filter==178);
    // Real live CC7 WinMM -> independent VXD lookup -> actual chip IFATN.
    const auto vol20=decodeWinMmShort(0x001407b0u);
    CHECK(vol20);state.apply(*vol20);
    const auto updated=candidateOriginalCreativeIfatnWrites(
        *message,3,state,encoded);
    CHECK(updated && (*updated)[1].value==0xB285);
    const std::array<OriginalWin95VxdPortWrite,2> liveVolume{{
        {(*updated)[0].port,(*updated)[0].value,8},
        {(*updated)[1].port,(*updated)[1].value,16}
    }};
    CHECK(applyOriginalCreativeVxdVoicePorts(chip,liveVolume));
    CHECK(real->voice[3].ifatn==0xB285);
    // Actual chip pitch update from live WinMM pitchbend maximum.
    const auto bend=decodeWinMmShort(0x007f7fe0u);
    CHECK(bend);state.apply(*bend);
    const auto livePitch=candidateOriginalRomIpWrites(piano,0,3,state);
    CHECK(livePitch && (*livePitch)[1].value==0xCC0C);
    const std::array<OriginalWin95VxdPortWrite,2> pitchUpdate{{
        {(*livePitch)[0].port,(*livePitch)[0].value,8},
        {(*livePitch)[1].port,(*livePitch)[1].value,16}
    }};
    CHECK(applyOriginalCreativeVxdVoicePorts(chip,pitchUpdate));
    CHECK(real->voice[3].ip==0xCC0C);
    // On the actual pinned chip: driver case5/case6 ROM address rewrite.
    // Establish the prior PSST high byte with a real chip write.
    const std::array<OriginalWin95VxdPortWrite,2> previousPsst{{
       {0xE22u,0xC3u,8u},{0x620u,0x47000055u,32u}
    }};
    CHECK(applyOriginalCreativeVxdVoicePorts(chip,previousPsst));
    CHECK(real->voice[3].psst==0x47000055u);
    const SbkRomAddress sample3{3,0xFEB9u,0x152FEu,0x152BAu,0x152F8u};
    const auto looped=candidateOriginalVxdSampleAddressUpdate(
        sample3,3,OriginalVxdSampleAddressCase::DriverCase5,real->voice[3].psst);
    CHECK(looped && applyOriginalCreativeVxdVoicePorts(chip,*looped));
    CHECK(real->voice[3].csl==0x000152F9u);
    CHECK(real->voice[3].psst==0x470152BAu);
    const auto noLoop=candidateOriginalVxdSampleAddressUpdate(
        sample3,3,OriginalVxdSampleAddressCase::DriverCase6,real->voice[3].psst);
    CHECK(noLoop && applyOriginalCreativeVxdVoicePorts(chip,*noLoop));
    CHECK(real->voice[3].csl==0x00015306u);
    CHECK(real->voice[3].psst==0x47015302u);
    // The verified *ordinary* VXD NoteOff is different from group choke:
    // 0x8000|previously ENCODED rate for both envelope generators.
    // It can also rewrite CSL/PSST if an original stored sample end exists.
    OriginalVxdNoteReleaseEvidence evidence{};
    evidence.original_key_channel_match=true;
    evidence.original_voice_record_verified=true;
    evidence.current_chip_registers_verified=true;
    evidence.encoded_volume_release=0x20;
    evidence.encoded_mod_release=0x45;
    evidence.stored_end_word=sample3.end_word;
    evidence.observed_chip_csl=real->voice[3].csl;
    evidence.observed_chip_psst=real->voice[3].psst;
    evidence.original_sustain_held=true;
    const auto deferred=candidateOriginalVxdMidiNoteRelease(3,evidence);
    CHECK(deferred && deferred->deferred_by_sustain && deferred->count==0);
    CHECK(real->voice[3].dcysusv==0x807fu); // no HW write when held!
    evidence.original_sustain_held=false;
    const auto release=candidateOriginalVxdMidiNoteRelease(3,evidence);
    CHECK(release && !release->deferred_by_sustain && release->count==8);
    CHECK(applyOriginalCreativeVxdVoicePorts(chip,
         std::span<const OriginalWin95VxdPortWrite>(
             release->writes.data(),release->count)));
    CHECK(real->voice[3].dcysusv==0x8020u);
    CHECK(real->voice[3].dcysus==0x8045u);
    CHECK(real->voice[3].csl==0x00015302u);
    CHECK(real->voice[3].psst==0x470152FEu);
    // First actual note-start CCCA is source start minus four.
    // Later original VXD writes *another* CCCA value with filter-Q bits.
    const auto startNoQ=candidateOriginalVxdRomCccaStart(
        sample3,3,5,true,OriginalVxdCccaStartPhase::StartWithoutFilterQ);
    const auto startWithQ=candidateOriginalVxdRomCccaStart(
        sample3,3,5,true,OriginalVxdCccaStartPhase::StartWithEncodedFilterQ);
    CHECK(startNoQ && startWithQ);
    CHECK(applyOriginalCreativeVxdVoicePorts(chip,*startNoQ));
    CHECK(real->voice[3].ccca==0x0000FEB5u);
    CHECK(applyOriginalCreativeVxdVoicePorts(chip,*startWithQ));
    CHECK(real->voice[3].ccca==0x5000FEB5u);
    // Original NoteOn voice geometry is a different VXD branch
    // from the earlier case5/6 sample-update paths.
    OriginalVxdRomNoteStartMode loopMode{};
    loopMode.original_driver_mode_verified=true;
    loopMode.encoded_mode_flags=3;
    loopMode.original_pan_byte=0x47;
    loopMode.original_chorus_send_byte=0x53;
    const auto initialGeometry=candidateOriginalVxdInitialRomNoteGeometry(
        sample3,3,loopMode);
    CHECK(initialGeometry &&
          initialGeometry->driver_retained_release_end==0x15302u);
    CHECK(applyOriginalCreativeVxdVoicePorts(
        chip,initialGeometry->loop_address_ports));
    CHECK(real->voice[3].psst==0x470152B9u);
    CHECK(real->voice[3].csl==0x530152F7u);
    loopMode.encoded_mode_flags=0;
    const auto oneShotGeometry=candidateOriginalVxdInitialRomNoteGeometry(
        sample3,3,loopMode);
    CHECK(oneShotGeometry &&
          applyOriginalCreativeVxdVoicePorts(chip,
              oneShotGeometry->loop_address_ports));
    CHECK(real->voice[3].psst==0x47015302u);
    CHECK(real->voice[3].csl==0x53015306u);
    const auto oldIfatn=real->voice[3].ifatn;
    // Malformed sequences must be rejected transactionally before writes.
    auto invalid=start;
    invalid[0].access_width_bits=16;
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,invalid));
    invalid=start;invalid[3].port=0xA20;
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,invalid));
    invalid=start;invalid[2].value=0x7eu; // voice30 reserved
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,invalid));
    invalid=start;invalid[1].value=0x10000u;
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,invalid));
    invalid=start;invalid[3].access_width_bits=8;
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,invalid));
    CHECK(real->voice[3].ifatn==oldIfatn);
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,{}));
    chip.close();
    CHECK(!applyOriginalCreativeVxdVoicePorts(chip,start));
    std::filesystem::remove(rom);
    std::cout<<"P002_ORIGINAL_VXD_PORTS_EXECUTED_ON_PINNED_EMU8000_PASS"
             <<" DCYSUSV=00FF VTFT=0000FFFF group_stop=807F"
             <<" original_GM0_ip=C962 live_CC7_ifatn=B285 bend_ip=CC0C"
             <<" sample_CSL=15306 PSST=47015302 mode5_6=TRUE"
             <<" original_note_start_envelope7_actual_chip=TRUE"
             <<" original_CCCA_source_minus4_Q_actual_chip=TRUE"
             <<" original_NoteOn_PSST_CSL_geometry_chip=TRUE"
             <<" ordinary_noteoff_DCYSUSV=8020_DCYSUS=8045"
             <<" release_CSL=15302_PSST_preserved=TRUE"
             <<" stereo=TRUE zero_Creative_bytes=TRUE"
             <<" original_driver_PCM_parity=NOT_PROVEN\n";
    return 0;
}
