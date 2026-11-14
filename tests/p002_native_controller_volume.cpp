#include "CreativeControllerVolume.h"
#include <array>
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(x) do{if(!(x)){std::cerr<<"P002_VXD_CONTROLLER_VOLUME_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
std::uint32_t midi(unsigned status,unsigned d1=0,unsigned d2=0) {
    return status|(d1<<8)|(d2<<16);
}
int main() {
    // The original installed Win95 SBAWE.VXD is a different independent
    // source from the 254 original 86Box EMU8000 IFATN measurements.
    OriginalCreativeVolumeTerms piano{100,127,16,0,true};
    OriginalCreativeVolumeTerms violin{100,127,8,0,true};
    CHECK(candidateOriginalCreativeAttenuation(1,piano)==173);
    CHECK(candidateOriginalCreativeAttenuation(64,piano)==72);
    CHECK(candidateOriginalCreativeAttenuation(127,piano)==42);
    CHECK(candidateOriginalCreativeAttenuation(1,violin)==165);
    CHECK(candidateOriginalCreativeAttenuation(64,violin)==64);
    CHECK(candidateOriginalCreativeAttenuation(127,violin)==34);
    unsigned sweepCount=0;
    for(unsigned v=1;v<128;++v){
        auto p=candidateOriginalCreativeAttenuation(static_cast<std::uint8_t>(v),piano);
        auto q=candidateOriginalCreativeAttenuation(static_cast<std::uint8_t>(v),violin);
        CHECK(p && q && static_cast<unsigned>(*p)==static_cast<unsigned>(*q)+8);
        // P002 254/254 original driver IFATN sweep values use an
        // independently measured common velocity function. The *new* CC7,
        // CC11 and separate ROM compensation law must reduce to those
        // original numbers for the two known encoded per-voice inputs.
        const auto legacyP=candidateInitialIfatn(
            static_cast<std::uint8_t>(v),CandidateVoiceParameters{16,0,178,false});
        const auto legacyV=candidateInitialIfatn(
            static_cast<std::uint8_t>(v),CandidateVoiceParameters{13,0,210,true});
        CHECK(legacyP && legacyV);
        CHECK(*p==legacyP->attenuation && *q==legacyV->attenuation);
        ++sweepCount;
    }
    CHECK(sweepCount==127);
    // Original 62-message 7-scenario Win95 matrix. Held C4 velocity96,
    // actual chronological controller values and hardware IFATN low byte.
    auto heldPiano=piano;
    heldPiano.cc7=20;heldPiano.cc11=127;
    CHECK(candidateOriginalCreativeAttenuation(96,heldPiano)==133); // B285
    heldPiano.cc7=127;
    CHECK(candidateOriginalCreativeAttenuation(96,heldPiano)==45);  // B22D
    heldPiano.cc11=30;
    CHECK(candidateOriginalCreativeAttenuation(96,heldPiano)==108); // B26C
    heldPiano.cc11=127;
    CHECK(candidateOriginalCreativeAttenuation(96,heldPiano)==45);
    // ROM-specific +16 must be applied AFTER expression, not before.
    auto nonRom=heldPiano;nonRom.original_1mgm_rom_sample=false;
    CHECK(candidateOriginalCreativeAttenuation(96,nonRom)==29);
    nonRom.cc11=30;
    CHECK(candidateOriginalCreativeAttenuation(96,nonRom)==92);
    // Hard mute, low CC threshold, validation and saturation.
    piano.cc7=0;CHECK(candidateOriginalCreativeAttenuation(1,piano)==255);
    piano.cc7=10;CHECK(candidateOriginalCreativeAttenuation(127,piano)==255);
    piano.cc7=11;CHECK(candidateOriginalCreativeAttenuation(127,piano)!=255);
    piano.cc7=128;CHECK(!candidateOriginalCreativeAttenuation(64,piano));
    piano.cc7=100;piano.cc11=128;
    CHECK(!candidateOriginalCreativeAttenuation(64,piano));
    piano.cc11=127;CHECK(!candidateOriginalCreativeAttenuation(0,piano));
    CHECK(!candidateOriginalCreativeAttenuation(128,piano));
    piano.device_state_additive_attenuation=256;
    CHECK(!candidateOriginalCreativeAttenuation(64,piano));
    piano.device_state_additive_attenuation=16;piano.encoded_voice_amplitude=-13;
    CHECK(!candidateOriginalCreativeAttenuation(64,piano));
    piano.encoded_voice_amplitude=0;
    OriginalCreativeIfatnTerms violinIfatn{};
    violinIfatn.volume=violin;violinIfatn.initial_cutoff=210;
    violinIfatn.bank=0;violinIfatn.encoded_volume_attack_rate=124;
    const auto instrument=candidateOriginalCreativeIfatn(90,violinIfatn);
    CHECK(instrument && instrument->cutoff==148);
    violinIfatn.bank=128;
    CHECK(candidateOriginalCreativeIfatn(90,violinIfatn)->cutoff==210);
    violinIfatn.bank=0;violinIfatn.encoded_volume_attack_rate=125;
    CHECK(candidateOriginalCreativeIfatn(90,violinIfatn)->cutoff==210);
    violinIfatn.encoded_volume_attack_rate=128;
    // 128 is valid in original SF2 descriptors; only values >128 reject.
    CHECK(candidateOriginalCreativeIfatn(90,violinIfatn)->cutoff==210);
    violinIfatn.encoded_volume_attack_rate=129;
    CHECK(!candidateOriginalCreativeIfatn(90,violinIfatn));
    // 16-channel live MIDI CC7/CC11 state feeds real E22/E20 IFATN writes.
    MidiState midiState;
    OriginalCreativeIfatnTerms terms{};
    terms.volume=OriginalCreativeVolumeTerms{100,127,16,0,true};
    terms.initial_cutoff=178;terms.bank=0;terms.encoded_volume_attack_rate=125;
    const auto note=decodeWinMmShort(midi(0x90+2,60,96));
    CHECK(note);
    const auto control7=decodeWinMmShort(midi(0xB0+2,7,20));
    CHECK(control7);midiState.apply(*control7);
    const auto words=candidateOriginalCreativeIfatnWrites(
        *note,3,midiState,terms);
    CHECK(words && (*words)[0].port==0xE22 &&
          (*words)[0].value==(0x20|3) &&
          (*words)[1].port==0xE20 && (*words)[1].value==0xB285);
    const auto control7max=decodeWinMmShort(midi(0xB0+2,7,127));
    CHECK(control7max);midiState.apply(*control7max);
    CHECK((*candidateOriginalCreativeIfatnWrites(
        *note,3,midiState,terms))[1].value==0xB22D);
    const auto exp30=decodeWinMmShort(midi(0xB0+2,11,30));
    CHECK(exp30);midiState.apply(*exp30);
    CHECK((*candidateOriginalCreativeIfatnWrites(
        *note,3,midiState,terms))[1].value==0xB26C);
    const auto zero=decodeWinMmShort(midi(0x90+2,60,0));CHECK(zero);
    CHECK(!candidateOriginalCreativeIfatnWrites(*zero,3,midiState,terms));
    CHECK(!candidateOriginalCreativeIfatnWrites(*note,30,midiState,terms));

    // The original 62-MIDI-message Win95 trace measured these four IFATN
    // updates WHILE a note was already sounding, not on new NoteOn.
    ActiveOriginalCreativeVoice held{};
    held.channel=2;held.midi_key=60;held.note_on_velocity=96;
    held.physical_voice=3;held.sounding=true;
    held.instrument=terms;
    MidiState live;
    const auto cc7low=decodeWinMmShort(midi(0xB0+2,7,20));
    CHECK(cc7low);
    // A stale channel state must not silently emit a register write.
    CHECK(!candidateOriginalCreativeLiveVolumeWrites(*cc7low,live,held));
    live.apply(*cc7low);
    auto liveWrites=candidateOriginalCreativeLiveVolumeWrites(*cc7low,live,held);
    CHECK(liveWrites && (*liveWrites)[0].value==0x23 &&
          (*liveWrites)[1].value==0xB285);
    const auto cc7high=decodeWinMmShort(midi(0xB0+2,7,127));CHECK(cc7high);
    live.apply(*cc7high);
    liveWrites=candidateOriginalCreativeLiveVolumeWrites(*cc7high,live,held);
    CHECK(liveWrites && (*liveWrites)[1].value==0xB22D);
    const auto cc11low=decodeWinMmShort(midi(0xB0+2,11,30));CHECK(cc11low);
    live.apply(*cc11low);
    liveWrites=candidateOriginalCreativeLiveVolumeWrites(*cc11low,live,held);
    CHECK(liveWrites && (*liveWrites)[1].value==0xB26C);
    const auto cc11high=decodeWinMmShort(midi(0xB0+2,11,127));CHECK(cc11high);
    live.apply(*cc11high);
    liveWrites=candidateOriginalCreativeLiveVolumeWrites(*cc11high,live,held);
    CHECK(liveWrites && (*liveWrites)[1].value==0xB22D);
    const auto otherVoice=decodeWinMmShort(midi(0xB0+3,7,127));CHECK(otherVoice);
    live.apply(*otherVoice);
    CHECK(!candidateOriginalCreativeLiveVolumeWrites(*otherVoice,live,held));
    const auto unrelated=decodeWinMmShort(midi(0xB0+2,10,64));CHECK(unrelated);
    live.apply(*unrelated);
    CHECK(!candidateOriginalCreativeLiveVolumeWrites(*unrelated,live,held));
    CHECK(!candidateOriginalCreativeLiveVolumeWrites(*note,live,held));
    held.sounding=false;CHECK(!candidateOriginalCreativeLiveVolumeWrites(*cc11high,live,held));
    held.sounding=true;held.physical_voice=30;
    CHECK(!candidateOriginalCreativeLiveVolumeWrites(*cc11high,live,held));
    std::cout<<"P002_REAL_DRIVER_CC7_CC11_VELOCITY_REGISTER_CANDIDATE_PASS"
        <<" original_sweep=254 original_cc7_cc11=4"
        <<" original_1MGM_order=TRUE private_driver_bytes=NONE"
        <<" full_original_register_trace_NOT_PROVEN=TRUE\n";
}
