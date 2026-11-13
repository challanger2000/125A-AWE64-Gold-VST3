#include "CreativeMidi.h"
#include "CreativeApiIds.h"
#include <utility>
#include <initializer_list>
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
namespace {
std::uint32_t msg(unsigned status,unsigned one=0,unsigned two=0) {
    return status|(one<<8)|(two<<16);
}
}
#define CHECK(pred) do { if(!(pred)) { std::cerr<<"P002_MIDI_CONTRACT_FAIL line="<<__LINE__<<" condition="<<#pred<<"\n"; return 1; } } while(0)
int main() {
    static_assert(static_cast<std::uint16_t>(AweMessage::LoadUserBank)==204);
    static_assert(static_cast<std::uint16_t>(AweMessage::ViennaLoadPreset)==2006);
    static_assert(static_cast<std::uint16_t>(AweMessage::ViennaPitchBend)==2012);
    MidiState state;
    unsigned count=0;
    for(unsigned channel=0;channel<16;channel++){
        auto message=decodeWinMmShort(msg(0x90u+channel,60,100));
        CHECK(message && message->kind==MidiKind::NoteOn && message->channel==channel);
        CHECK(message->raw_short==msg(0x90u+channel,60,100));
        ++count;
        message=decodeWinMmShort(msg(0x90u+channel,60,0));
        CHECK(message && message->kind==MidiKind::NoteOff); // MIDI NoteOn velocity=0
        ++count;
        for(const auto [controller,value] : {
                std::pair<unsigned,unsigned>{0,3}, {32,7}, {7,101},
                {11,86},{10,44},{64,127}}) {
            message=decodeWinMmShort(msg(0xb0u+channel,controller,value));
            CHECK(message && message->kind==MidiKind::Controller);
            state.apply(*message); ++count;
        }
        message=decodeWinMmShort(msg(0xc0u+channel,23)); CHECK(message);
        state.apply(*message);++count;
        message=decodeWinMmShort(msg(0xe0u+channel,0x7f,0x7f)); CHECK(message);
        state.apply(*message);++count;
        CHECK(state.channel(channel).program==23);
        CHECK(state.channel(channel).bank_msb==3 && state.channel(channel).bank_lsb==7);
        CHECK(state.channel(channel).pitch_bend14==16383);
        CHECK(state.channel(channel).cc7_volume==101);
        CHECK(state.channel(channel).cc11_expression==86);
        CHECK(state.channel(channel).cc10_pan==44);
        CHECK(state.channel(channel).sustain_cc64==127);
    }
    // Channel messages must not modify the other 15 channels.
    auto reset=decodeWinMmShort(msg(0xb0u+9u,121,0));
    CHECK(reset);
    state.apply(*reset);
    CHECK(state.channel(9).cc7_volume==100);
    CHECK(state.channel(9).pitch_bend14==8192);
    CHECK(state.channel(9).cc11_expression==127);
    CHECK(state.channel(9).sustain_cc64==0);
    CHECK(state.channel(9).cc10_pan==64);
    // Original Win95 matrix sends a separate Program Change after CC121.
    // Reset controllers must NEVER fabricate a Program 0 / Bank 0 switch.
    CHECK(state.channel(9).program==23);
    CHECK(state.channel(9).bank_msb==3);
    CHECK(state.channel(9).bank_lsb==7);
    CHECK(state.channel(8).cc7_volume==101);
    CHECK(state.channel(10).cc7_volume==101);
    // State APIs can also receive direct C++ messages from the native
    // Creative bank compiler, not only validated WinMM short packets.
    // Invalid channel 16 must NEVER alias channel zero or modify bank 0.
    {
        MidiState guarded{};
        MidiMessage invalid{};
        invalid.kind=MidiKind::Controller;
        invalid.channel=16u;invalid.data1=0u;invalid.data2=1u;
        guarded.apply(invalid);
        CHECK(guarded.channel(0).bank_msb==0u);
        invalid.kind=MidiKind::Program;
        invalid.channel=255u;invalid.data1=45u;invalid.data2=0u;
        guarded.apply(invalid);
        CHECK(guarded.channel(15).program==0u);
        invalid.channel=0u;invalid.data1=128u;invalid.data2=0u;
        guarded.apply(invalid);
        CHECK(guarded.channel(0).program==0u);
        invalid.kind=MidiKind::Controller;
        invalid.data1=0u;invalid.data2=128u;
        guarded.apply(invalid);
        CHECK(guarded.channel(0).bank_msb==0u);
        // A two-byte Program Change may contain an unused raw data2.
        invalid.kind=MidiKind::Program;
        invalid.data1=46u;invalid.data2=255u;
        guarded.apply(invalid);
        CHECK(guarded.channel(0).program==46u);
    }
    // Reject malformed/status-byte injection and non-MIDI 0xF* system codes.
    CHECK(!decodeWinMmShort(msg(0xf0,12,15)));
    CHECK(!decodeWinMmShort(msg(0x70,60,100)));
    CHECK(!decodeWinMmShort(msg(0x90,0x80,12)));
    CHECK(!decodeWinMmShort(msg(0xb0,12,0x80)));
    CHECK(!decodeWinMmShort(msg(0xe0,0x7f,0x80)));
    std::cout<<"P002_CREATIVE_PRIMARY_API_MIDI_HOST_CONTRACT_PASS events="<<count
             <<" channels=16 independent_bank_program_cc_pitch=TRUE"
             <<" original_driver_register_mapping=NOT_VERIFIED\n";
}
