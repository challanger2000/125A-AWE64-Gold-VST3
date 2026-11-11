#include "CreativeMidi.h"
#include "CreativeApiIds.h"
#include <cassert>
#include <cstdint>
#include <iostream>
using namespace awe64::creative;
namespace {
std::uint32_t msg(unsigned status,unsigned one=0,unsigned two=0) {
    return status|(one<<8)|(two<<16);
}
}
int main() {
    static_assert(static_cast<std::uint16_t>(AweMessage::LoadUserBank)==204);
    static_assert(static_cast<std::uint16_t>(AweMessage::ViennaLoadPreset)==2006);
    static_assert(static_cast<std::uint16_t>(AweMessage::ViennaPitchBend)==2012);
    MidiState state;
    unsigned count=0;
    for(unsigned channel=0;channel<16;channel++){
        auto message=decodeWinMmShort(msg(0x90u+channel,60,100));
        assert(message && message->kind==MidiKind::NoteOn && message->channel==channel);
        assert(message->raw_short==msg(0x90u+channel,60,100));
        ++count;
        message=decodeWinMmShort(msg(0x90u+channel,60,0));
        assert(message && message->kind==MidiKind::NoteOff); // MIDI NoteOn velocity=0
        ++count;
        for(const auto [controller,value] : {
                std::pair<unsigned,unsigned>{0,3}, {32,7}, {7,101},
                {11,86},{10,44},{64,127}}) {
            message=decodeWinMmShort(msg(0xb0u+channel,controller,value));
            assert(message && message->kind==MidiKind::Controller);
            state.apply(*message); ++count;
        }
        message=decodeWinMmShort(msg(0xc0u+channel,23)); assert(message);
        state.apply(*message);++count;
        message=decodeWinMmShort(msg(0xe0u+channel,0x7f,0x7f)); assert(message);
        state.apply(*message);++count;
        assert(state.channel(channel).program==23);
        assert(state.channel(channel).bank_msb==3 && state.channel(channel).bank_lsb==7);
        assert(state.channel(channel).pitch_bend14==16383);
        assert(state.channel(channel).cc7_volume==101);
        assert(state.channel(channel).cc11_expression==86);
        assert(state.channel(channel).cc10_pan==44);
        assert(state.channel(channel).sustain_cc64==127);
    }
    // Channel messages must not modify the other 15 channels.
    auto reset=decodeWinMmShort(msg(0xb0u+9u,121,0));
    assert(reset);
    state.apply(*reset);
    assert(state.channel(9).cc7_volume==100);
    assert(state.channel(9).pitch_bend14==8192);
    assert(state.channel(8).cc7_volume==101);
    assert(state.channel(10).cc7_volume==101);
    // Reject malformed/status-byte injection and non-MIDI 0xF* system codes.
    assert(!decodeWinMmShort(msg(0xf0,12,15)));
    assert(!decodeWinMmShort(msg(0x70,60,100)));
    assert(!decodeWinMmShort(msg(0x90,0x80,12)));
    assert(!decodeWinMmShort(msg(0xb0,12,0x80)));
    assert(!decodeWinMmShort(msg(0xe0,0x7f,0x80)));
    std::cout<<"P002_CREATIVE_PRIMARY_API_MIDI_HOST_CONTRACT_PASS events="<<count
             <<" channels=16 independent_bank_program_cc_pitch=TRUE"
             <<" original_driver_register_mapping=NOT_VERIFIED\n";
}
