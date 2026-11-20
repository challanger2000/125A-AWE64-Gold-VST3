#pragma once
// P007BP: track host MIDI NoteOn destination across bank changes.
// A MIDI NoteOff must reach the CHANNEL on which its NoteOn was sent.
// When a host legitimately retriggers one pitch into BOTH destinations,
// release both once. This only routes MIDI to the ORIGINAL Creative guest;
// it is not a substitute synthesizer and never changes EMU8000 audio.
#include <array>
#include <cstdint>

namespace p007 {
class MidiNoteRoute final {
    // Bit 0 = original melodic host channel, bit 1 = Creative drum ch10.
    std::array<std::uint8_t,16u*128u> active_{};
    static constexpr unsigned key(unsigned channel,unsigned pitch) noexcept {
        return channel*128u+pitch;
    }
public:
    struct Release {
        std::uint8_t first=0u,second=0u;
        std::uint8_t count=0u;
    };
    void reset() noexcept {active_.fill(0u);}
    // Precondition: channel <16 and pitch <128; caller validates VST3 input.
    [[nodiscard]] std::uint8_t noteOn(unsigned channel,unsigned pitch,
                                     bool drums) noexcept {
        active_[key(channel,pitch)]|=
            static_cast<std::uint8_t>(drums?2u:1u);
        return static_cast<std::uint8_t>(drums?9u:channel);
    }
    [[nodiscard]] Release noteOff(unsigned channel,unsigned pitch,
                                  bool drumsNow) noexcept {
        const auto index=key(channel,pitch);
        const std::uint8_t flags=active_[index];
        active_[index]=0u;
        // An untracked NoteOff may still need to release a real guest voice
        // (e.g. when resuming a project): retain the old best-effort path.
        if(flags==0u)
            return {static_cast<std::uint8_t>(drumsNow?9u:channel),0u,1u};
        // Incoming channel 10 and routed drum channel 10 coincide; do not
        // duplicate release packets and overwhelm the 16550 UART.
        if(flags==3u && channel!=9u)
            return {static_cast<std::uint8_t>(channel),9u,2u};
        return {static_cast<std::uint8_t>(flags&1u?channel:9u),0u,1u};
    }
};
} // namespace p007
