#include "P005OriginalCreativeHostAdapter.hpp"
#include <array>
#include <cstdio>
using awe64::vst3::P005OriginalCreativeHostAdapter;
using namespace awe64::original_vxd;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"P005_FAIL:%d %s\n",__LINE__,#x);return 1;}}while(false)
namespace {
struct Chip {
    bool good=true;
    std::uint64_t writes=0,reads=0;
    [[nodiscard]] bool ready() const noexcept {return good;}
    void writePort(std::uint16_t,std::uint16_t) noexcept {++writes;}
    std::uint16_t readPort(std::uint16_t) noexcept {++reads;return 0u;}
};
struct VerifiedSourceFixture {
    static bool resolve(void*,MidiChannel const& channel,std::uint8_t key,
                        std::uint8_t velocity,
                        std::array<SourceVoice,max_source_note_layers>& voices,
                        std::uint8_t& count) noexcept {
        if(channel.bank_msb!=0u || channel.program!=7u ||
           key!=60u || velocity!=100u)return false;
        count=1u;
        auto& v=voices[0];
        v.source_verified=true;
        v.descriptor[0x8c]=1u;
        v.note_start_descriptor_verified=true;
        v.note_start_descriptor[0x8c]=1u;
        v.note_start_descriptor[0x76]=0x00u;
        v.note_start_descriptor[0x77]=0x01u;
        v.note_start_descriptor[0x78]=0x20u;
        v.note_start_descriptor[0x7a]=0x00u;
        v.note_start_descriptor[0x7b]=0x00u;
        v.note_start_descriptor[0x7c]=0x21u;
        v.note_start_descriptor[0x6c]=1u;
        return true;
    }
    static bool compile(void*,SourceVoice const&,std::uint8_t,
                        std::uint8_t,OriginalVoiceProgram& plan) noexcept {
        plan.source_verified=true;
        plan.data.fill(0u);
        return true;
    }
};
}
int main() {
    using Adapter=P005OriginalCreativeHostAdapter<Chip>;
    Chip chip{},second{};
    Adapter host{};
    std::array<VerifiedSf2Zone,1> zones{};
    VerifiedSf2Bank bank{zones};
    CHECK(!host.active());
    CHECK(host.noteOn(0,60,100)==MidiResult::MissingSource);
    CHECK(!host.activate(chip,bank,false));
    CHECK(!host.activate(chip,bank,true));
    auto& zone=zones[0];
    zone.original_zone_verified=true;
    zone.voice.source_verified=true;
    zone.voice.note_start_descriptor_verified=true;
    zone.original_state.original_descriptor_verified=true;
    zone.original_state.original_device_state_verified=true;
    zone.original_state.original_sample_placement_verified=true;
    CHECK(!host.activate(chip,bank,false));
    CHECK(host.activate(chip,bank,true));
    CHECK(host.active());
    CHECK(host.sharedRuntime()->attached_to(chip));
    CHECK(!host.sharedRuntime()->attached_to(second));
    CHECK(host.sharedRuntime()->scratch.device==&host.sharedRuntime()->midi.device);
    CHECK(!host.activate(second,bank,true));
    CHECK(host.noteOn(16,60,100)==MidiResult::Invalid);
    CHECK(host.controller(0,0,128)==MidiResult::Invalid);
    CHECK(host.program(0,7)==MidiResult::Applied);
    CHECK(host.sharedRuntime()->midi.channel[0].program==7u);
    CHECK(host.controller(0,0,1)==MidiResult::Applied);
    CHECK(host.sharedRuntime()->midi.channel[0].bank_msb==1u);
    CHECK(host.controller(0,0,0)==MidiResult::Applied);
    const auto before=chip.writes;
    CHECK(host.noteOn(0,60,100)==MidiResult::MissingSource);
    CHECK(chip.writes==before);
    CHECK(host.sharedRuntime()->midi_event(chip,0x90u,60u,100u,
          {nullptr,VerifiedSourceFixture::resolve,
           VerifiedSourceFixture::compile,nullptr})==MidiResult::Applied);
    CHECK(chip.writes>before);
    CHECK(host.sharedRuntime()->scratch.device->voices[0].key_marker==60u);
    host.resetNonRealtime();
    CHECK(!host.active() && host.sharedRuntime()==nullptr);
    CHECK(host.activate(second,bank,true));
    CHECK(host.sourceBound() && host.active());
    CHECK(host.sharedRuntime()->attached_to(second));
    CHECK(host.sharedRuntime()->midi.device.voices[0].key_marker==0x0ffeu);
    // Simulate permanent ORIGINAL Creative source-driver fault. An
    // inactive/faulted port MUST still own host MIDI routing, otherwise
    // the exported VST3 could unexpectedly play its old P001 test voice.
    host.sharedRuntime()->midi.fault=true;
    CHECK(host.sourceBound() && !host.active());
    const auto faultWrites=second.writes;
    CHECK(host.noteOn(0u,60u,100u)==MidiResult::MissingSource);
    CHECK(host.noteOff(0u,60u)==MidiResult::MissingSource);
    CHECK(host.program(0u,7u)==MidiResult::MissingSource);
    CHECK(second.writes==faultWrites);
    host.resetNonRealtime();
    CHECK(!host.sourceBound() && !host.active());
    CHECK(host.sharedRuntime()==nullptr);
    std::puts("P005_LATCHED_SOURCE_FAULT_NEVER_FALLS_BACK_PASS");
    std::puts("P005_ORIGINAL_CREATIVE_SINGLE_CHIP_SINGLE_DEVICE_MIDI_PASS");
    return 0;
}
