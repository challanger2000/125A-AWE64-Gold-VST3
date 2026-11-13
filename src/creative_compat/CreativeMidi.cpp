#include "CreativeMidi.h"

namespace awe64::creative {
std::optional<MidiMessage> decodeWinMmShort(std::uint32_t packed) noexcept {
    const auto status = static_cast<std::uint8_t>(packed & 255u);
    const auto classCode = static_cast<std::uint8_t>(status & 0xf0u);
    if (status < 0x80u || status >= 0xf0u) return std::nullopt;
    const auto d1 = static_cast<std::uint8_t>((packed >> 8) & 255u);
    const auto d2 = static_cast<std::uint8_t>((packed >> 16) & 255u);
    if ((d1 & 0x80u) || ((classCode != 0xc0u && classCode != 0xd0u) && (d2 & 0x80u)))
        return std::nullopt;
    MidiMessage event{};
    event.channel = static_cast<std::uint8_t>(status & 15u);
    event.data1 = d1;
    event.data2 = d2;
    event.raw_short = packed;
    switch (classCode) {
    case 0x80: event.kind = MidiKind::NoteOff; break;
    case 0x90: event.kind = d2==0u ? MidiKind::NoteOff : MidiKind::NoteOn; break;
    case 0xa0: event.kind = MidiKind::PolyPressure; break;
    case 0xb0: event.kind = MidiKind::Controller; break;
    case 0xc0: event.kind = MidiKind::Program; break;
    case 0xd0: event.kind = MidiKind::ChannelPressure; break;
    case 0xe0: event.kind = MidiKind::PitchBend; break;
    default: return std::nullopt;
    }
    return event;
}
void MidiState::apply(const MidiMessage& message) noexcept {
    // decodeWinMmShort() validates wire MIDI, but public callers also
    // construct MidiMessage directly when compiling a Creative bank note.
    // Never alias an out-of-range channel (e.g. 16) onto channel zero.
    // The second byte is unused for Program/ChannelPressure commands.
    if(message.channel >= channels_.size() || message.data1 >= 128u ||
       ((message.kind != MidiKind::Program &&
         message.kind != MidiKind::ChannelPressure) &&
        message.data2 >= 128u))return;
    auto& c=channels_[message.channel];
    switch (message.kind) {
    case MidiKind::Program: c.program=message.data1; break;
    case MidiKind::ChannelPressure: c.channel_pressure=message.data1; break;
    case MidiKind::PitchBend:
        c.pitch_bend14=static_cast<std::uint16_t>(
            message.data1 | (std::uint16_t(message.data2)<<7)); break;
    case MidiKind::Controller:
        switch (message.data1) {
        case 0: c.bank_msb=message.data2; break;
        case 32: c.bank_lsb=message.data2; break;
        case 7: c.cc7_volume=message.data2; break;
        case 11: c.cc11_expression=message.data2; break;
        case 10: c.cc10_pan=message.data2; break;
        case 64: c.sustain_cc64=message.data2; break;
        case 121: {
            // MIDI CC121 Reset All Controllers is NOT GM Program Change or
            // Bank Select. Never reset the selected patch/bank as a side
            // effect of resetting pitchbend/sustain/expression. Distinct
            // Creative-original output/register behavior awaits P002 oracle.
            const auto program = c.program;
            const auto msb = c.bank_msb;
            const auto lsb = c.bank_lsb;
            c = ChannelState{};
            c.program = program;
            c.bank_msb = msb;
            c.bank_lsb = lsb;
            break;
        }
        default: break;
        }
        break;
    default: break; // note/pressure effects await ORIGINAL driver trace oracle
    }
}
} // namespace awe64::creative
