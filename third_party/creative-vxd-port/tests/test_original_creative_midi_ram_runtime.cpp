#include "OriginalCreativeMidiRamRuntime.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64::original_vxd;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"MIDI_RAM_RUNTIME_FAIL:%d %s\n",__LINE__,#x);std::exit(1);}}while(false)
namespace {
struct Mock {
    std::uint64_t writes=0,reads=0;
    bool available=true;
    [[nodiscard]] bool ready() const noexcept {return available;}
    void writePort(std::uint16_t,std::uint16_t) noexcept {++writes;}
    [[nodiscard]] std::uint16_t readPort(std::uint16_t) noexcept {
        ++reads;return 0u;
    }
};
struct VerifiedSourceFixture {
    static bool resolve(void*,MidiChannel const&,std::uint8_t,
                        std::uint8_t,
                        std::array<SourceVoice,max_source_note_layers>& voices,
                        std::uint8_t& count) noexcept {
        count=1u;
        auto& v=voices[0];
        v.source_verified=true;
        v.descriptor[0x8cu]=1u;
        v.note_start_descriptor_verified=true;
        v.note_start_descriptor[0x8cu]=1u;
        // Original RAM sample starts at physical word address 0x200100.
        // Its original mode1 is looping without a retained release end.
        v.note_start_descriptor[0x76u]=0x00u;
        v.note_start_descriptor[0x77u]=0x01u;
        v.note_start_descriptor[0x78u]=0x20u;
        v.note_start_descriptor[0x7au]=0x00u;
        v.note_start_descriptor[0x7bu]=0x00u;
        v.note_start_descriptor[0x7cu]=0x21u;
        v.note_start_descriptor[0x6cu]=1u;
        return true;
    }
    static bool compile(void*,SourceVoice const&,std::uint8_t,std::uint8_t,
                        OriginalVoiceProgram& plan) noexcept {
        plan.source_verified=true;
        plan.data.fill(0u);
        return true;
    }
};
}
int main() {
    OriginalCreativeMidiRamRuntime runtime{};
    Mock chip{},another{};
    CHECK(runtime.bind_chip(chip) && runtime.attached_to(chip));
    CHECK(!runtime.attached_to(another) && !runtime.bind_chip(another));
    CHECK(runtime.scratch.device==&runtime.midi.device);
    // Deliberately use synthetic shared MIDI-voice state; this is an API
    // ownership regression, NOT a source-proven Win95 busy-bank2 snapshot.
    runtime.midi.device.voices[0].key_marker=0x003cu;
    CHECK(runtime.scratch.device->voices[0].key_marker==0x003cu);
    runtime.midi.device.voices[0].key_marker=0x0ffeu;
    OriginalCreativeRamBankCursor cursor{true,false,0x200000u};
    const std::array<std::uint8_t,4> malformed{{0u,1u,2u,3u}};
    const auto refused=runtime.upload_bank(chip,malformed,cursor);
    CHECK(refused.status==OriginalSf2UploadStatus::MalformedBank);
    CHECK(!runtime.midi.fault && !cursor.faulted && chip.writes==0u);
    // A MIDI NoteOn must update the SAME source driver rows later visited
    // by the occupied-voice RAM scratch allocator. No independent copy.
    runtime.midi.device.voices[0].blocked=true;
    const SoundFontSource verified_sf2{nullptr,
        VerifiedSourceFixture::resolve,VerifiedSourceFixture::compile,nullptr};
    CHECK(runtime.midi_event(chip,0x90u,60u,100u,verified_sf2)==MidiResult::Applied);
    CHECK(runtime.scratch.device->voices[0].key_marker==0x003cu);
    CHECK(!runtime.scratch.device->voices[0].blocked);
    CHECK(runtime.scratch.device->mode_count==31u);
    CHECK(runtime.scratch.device->voices[0].sample_end==0u);
    runtime.scratch.active=true;
    CHECK(runtime.midi_event(chip,0x90u,60u,100u,{})==MidiResult::FaultLatched);
    CHECK(runtime.upload_bank(chip,malformed,cursor).status==OriginalSf2UploadStatus::SourceMissing);
    runtime.scratch.active=false;
    CHECK(runtime.midi_event(another,0x90u,60u,100u,{})==MidiResult::MissingSource);
    // Broken physical adapter is terminal only when an original source
    // transfer was actually attempted; no fictitious device rollback.
    chip.available=false;
    CHECK(!runtime.bind_chip(chip));
    CHECK(runtime.upload_bank(chip,malformed,cursor).status==OriginalSf2UploadStatus::MalformedBank);
    CHECK(!runtime.midi.fault);
    std::puts("SOURCE_SHARED_MIDI_RAM_RUNTIME_PASS");
}
