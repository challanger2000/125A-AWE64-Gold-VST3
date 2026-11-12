#include "CreativeBoundVoiceController.h"
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
#define CHECK(pred) do {if(!(pred)){std::cerr<<"P002_BOUND_VOICE_FAIL line="<<__LINE__<<" "<<#pred<<"\n";return 1;}}while(0)
std::uint32_t msg(unsigned status,unsigned d1=0,unsigned d2=0) {
    return status|(d1<<8)|(d2<<16);
}
MidiMessage decode(std::uint32_t packed) {
    const auto r=decodeWinMmShort(packed);
    return r.value();
}
CreativeRomNotePlan pianoNote(std::uint8_t channel=2) {
    CreativeRomNotePlan n{};
    n.channel=channel;n.program=0;n.key=60;n.velocity=96;
    n.original_sbk_bank=0;n.count=1;
    n.layers[0].metadata.bank=0;n.layers[0].metadata.program=0;
    n.layers[0].metadata.key=60;n.layers[0].metadata.velocity=96;
    n.layers[0].metadata.rom_sample_selector=3;
    n.layers[0].metadata.instrument_local.raw[55]=7696; // original GM0
    n.layers[0].address={3,0xFEB9,0x152FE,0x10000,0x10100};
    return n;
}
OriginalCreativeIfatnTerms originalPianoTerms() {
    OriginalCreativeIfatnTerms out{};
    out.bank=0;out.initial_cutoff=178;out.encoded_volume_attack_rate=125;
    out.volume={100,127,16,0,true}; // VXD measured encoded terms
    return out;
}
int main() {
    CreativeBoundVoiceController original{};
    auto note=pianoNote();
    auto terms=originalPianoTerms();
    const auto first=original.bind(note,0,3,terms);
    CHECK(first && original.occupiedVoiceCount()==1);
    CHECK(first->pitch[0].port==0xE22 && first->pitch[0].value==3);
    CHECK(first->pitch[1].port==0xE20 && first->pitch[1].value==0xC962);
    CHECK(first->filter_and_attenuation[0].value==(0x20|3));
    CHECK(first->filter_and_attenuation[1].port==0xE20);
    CHECK(!original.bind(note,0,3,terms)); // duplicate physical voice
    CHECK(!original.bind(note,0,30,terms)); // DRAM refresh voices
    CHECK(!original.bind(note,1,9,terms)); // unknown bank layer
    auto broken=note;broken.layers[0].address.sample_id=55;
    CHECK(!original.bind(broken,0,9,terms));
    broken=note;broken.layers[0].address.loop_end_word=0x20000;
    CHECK(!original.bind(broken,0,9,terms));
    auto wrongTerms=terms;wrongTerms.volume.original_1mgm_rom_sample=false;
    CHECK(!original.bind(note,0,9,wrongTerms));
    wrongTerms=terms;wrongTerms.bank=128;
    CHECK(!original.bind(note,0,9,wrongTerms));

    auto actions=original.process(decode(msg(0xB2,7,20)));
    CHECK(actions && actions->count==1);
    CHECK(actions->actions[0].kind==BoundVoiceActionKind::UpdateIfatn);
    CHECK(actions->actions[0].physical_voice==3);
    CHECK(actions->actions[0].writes[0].value==0x23);
    CHECK(actions->actions[0].writes[1].value==0xB285);
    actions=original.process(decode(msg(0xB2,7,127)));
    CHECK(actions && actions->count==1 && actions->actions[0].writes[1].value==0xB22D);
    actions=original.process(decode(msg(0xB2,11,30)));
    CHECK(actions && actions->count==1 && actions->actions[0].writes[1].value==0xB26C);
    actions=original.process(decode(msg(0xB2,11,127)));
    CHECK(actions && actions->count==1 && actions->actions[0].writes[1].value==0xB22D);

    actions=original.process(decode(msg(0xE2,0,0)));
    CHECK(actions && actions->count==1 && actions->actions[0].kind==BoundVoiceActionKind::UpdatePitch);
    CHECK(actions->actions[0].writes[0].value==3 && actions->actions[0].writes[1].value==0xC6B8);
    actions=original.process(decode(msg(0xE2,0,64)));
    CHECK(actions && actions->count==1 && actions->actions[0].writes[1].value==0xC962);
    actions=original.process(decode(msg(0xE2,127,127)));
    CHECK(actions && actions->count==1 && actions->actions[0].writes[1].value==0xCC0C);
    actions=original.process(decode(msg(0xE2,0,64)));
    CHECK(actions && actions->count==1 && actions->actions[0].writes[1].value==0xC962);

    actions=original.process(decode(msg(0xB2,64,127))); // pedal down
    CHECK(actions && actions->count==0);
    actions=original.process(decode(msg(0x82,60,0))); // key up, still sustained
    CHECK(actions && actions->count==0);
    actions=original.process(decode(msg(0xB2,64,0))); // pedal up
    CHECK(actions && actions->count==1);
    CHECK(actions->actions[0].kind==BoundVoiceActionKind::RequestEnvelopeRelease);
    CHECK(actions->actions[0].physical_voice==3);
    CHECK(original.occupiedVoiceCount()==1); // HW release NOT an immediate steal
    actions=original.process(decode(msg(0xB2,7,20)));
    CHECK(actions && actions->count==0); // no fabricated updates after release
    CHECK(original.clearPhysicalVoice(3));
    CHECK(!original.clearPhysicalVoice(3));
    CHECK(original.occupiedVoiceCount()==0);
    CHECK(!original.clearPhysicalVoice(30));

    // Two separate original ROM layers can occupy physical 3 and 6.
    // The original allocator (choosing those numbers) is still external.
    actions=original.process(decode(msg(0xB2,7,100)));
    CHECK(actions && actions->count==0);
    CHECK(original.bind(note,0,3,terms));
    CHECK(original.bind(note,0,6,terms));
    actions=original.process(decode(msg(0xB2,7,20)));
    CHECK(actions && actions->count==2);
    CHECK(actions->actions[0].physical_voice==3 &&
          actions->actions[1].physical_voice==6);
    CHECK(actions->actions[0].writes[1].value==0xB285 &&
          actions->actions[1].writes[1].value==0xB285);
    actions=original.process(decode(msg(0xB2,120,0)));
    CHECK(actions && actions->count==2);
    CHECK(actions->actions[0].kind==BoundVoiceActionKind::RequestImmediateKill);
    CHECK(actions->actions[1].kind==BoundVoiceActionKind::RequestImmediateKill);
    CHECK(original.occupiedVoiceCount()==0);

    // CC123 holds under sustain; CC121 resets controller but preserves program
    // and issues a release request for a key that was already released.
    CHECK(original.bind(note,0,3,terms));
    CHECK(original.process(decode(msg(0xB2,64,127))));
    actions=original.process(decode(msg(0xB2,123,0)));
    CHECK(actions && actions->count==0);
    actions=original.process(decode(msg(0xB2,121,0)));
    CHECK(actions && actions->count==2); // IFATN reset + sustain release
    CHECK(actions->actions[0].kind==BoundVoiceActionKind::UpdateIfatn);
    CHECK(actions->actions[1].kind==BoundVoiceActionKind::RequestEnvelopeRelease);
    CHECK(original.midi().channel(2).program==0 &&
          original.midi().channel(2).cc7_volume==100 &&
          original.midi().channel(2).cc11_expression==127);
    CHECK(original.clearPhysicalVoice(3));

    // MIDI channel separation: changing channel 2 cannot update a voice on 1.
    auto other=pianoNote(1);
    CHECK(original.bind(other,0,9,terms));
    actions=original.process(decode(msg(0xB2,7,127)));
    CHECK(actions && actions->count==0);
    actions=original.process(decode(msg(0xB1,7,20)));
    CHECK(actions && actions->count==1 && actions->actions[0].physical_voice==9);
    CHECK(actions->actions[0].writes[1].value==0xB285);
    MidiMessage invalid{};invalid.kind=MidiKind::Controller;
    invalid.channel=16;invalid.data1=7;invalid.data2=50;
    CHECK(!original.process(invalid));
    CHECK(original.midi().channel(1).cc7_volume==20);
    CHECK(original.clearPhysicalVoice(9));
    std::cout<<"P002_BOUND_ORIGINAL_ROM_VOICES_PASS original_live_cc=4 pitchbend=3"
             <<" sustain_release=TRUE all_sound_off=TRUE"
             <<" allocator_and_release_registers_EXPLICITLY_UNVERIFIED=TRUE\n";
}
