#pragma once
// P002 Creative host-MIDI staging, strictly BEFORE any EMU8000 register mapping.
// It makes no acoustic claims: P002 register writes must be derived from
// running original Creative Win95 traces and compared separately.
#include <array>
#include <cstdint>
#include <optional>

namespace awe64::creative {
enum class MidiKind : std::uint8_t {
    NoteOff, NoteOn, PolyPressure, Controller,
    Program, ChannelPressure, PitchBend
};
struct MidiMessage {
    MidiKind kind{};
    std::uint8_t channel = 0;
    std::uint8_t data1 = 0;
    std::uint8_t data2 = 0;
    std::uint32_t raw_short = 0; // original WinMM short message, for trace audit
};
[[nodiscard]] std::optional<MidiMessage> decodeWinMmShort(std::uint32_t packed) noexcept;

struct ChannelState {
    std::uint8_t bank_msb = 0, bank_lsb = 0, program = 0;
    std::uint8_t cc7_volume = 100, cc11_expression = 127, cc10_pan = 64;
    std::uint8_t sustain_cc64 = 0, channel_pressure = 0;
    std::uint16_t pitch_bend14 = 8192;
};
class MidiState {
public:
    MidiState() noexcept = default;
    // Host logical state only; no voice steal, sample choice or DSP writes.
    void apply(const MidiMessage& event) noexcept;
    [[nodiscard]] const ChannelState& channel(std::uint8_t index) const noexcept {
        return channels_[index & 15u];
    }
private:
    std::array<ChannelState,16> channels_{};
};
} // namespace awe64::creative
