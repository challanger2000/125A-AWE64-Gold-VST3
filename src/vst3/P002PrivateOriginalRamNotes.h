#pragma once
// P002 OWNER-PRIVATE, SOURCE-RECORDED original Win95 Creative MIDI
// NoteOn/NoteOff port events. No original bytes embedded in the VST3.
// All load/SHA/format parsing during setActive(), never on audio thread.
// The note scheduler calls only the ORIGINAL unmodified EMU8000 writePort
// and render path, preserving observed 0/1/2 chip-sample event timing.
// One exact original source program (User Bank 1, Program 0), channel 0,
// C4/D4 velocity 100. NOT a generic bank synthesizer or proven PCM parity.
#include "Chip.h"
#include "CreativeSampleClockPorts.h"
#include <span>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace awe64::vst3 {
class P002PrivateOriginalRamNotes final {
public:
    void clear() noexcept;
    [[nodiscard]] bool load(const std::filesystem::path& source,
                            std::string& error);
    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] bool noteOn(awe64::Chip& chip,int channel,
                              int pitch,int velocity) noexcept;
    [[nodiscard]] bool noteOff(awe64::Chip& chip,int channel,
                               int pitch) noexcept;
    // Audio rendering may be split at original per-sample port timestamps,
    // but never allocates, reads files, grabs locks or generates registers.
    void render(awe64::Chip& chip,std::int32_t* stereo,
                std::size_t frames) noexcept;
    // Native-only private differential: compare independently generated
    // real-VST3 OUT values/ordering against the SHA-pinned Win95 trace.
    // Return only the measured original sample offsets plus the NATIVE
    // values. Never emit or replay the source trace's own data.
    [[nodiscard]] bool verifiedNativeClockPlan(
        bool note_off,int channel,int pitch,int velocity,
        std::span<const awe64::ChipPortWrite> independently_generated,
        std::span<awe64::creative::SourceClockedPort> output,
        std::size_t& output_count) const noexcept;

private:
    static constexpr std::size_t kMaxSourceEvents=256u;
    struct Event {
        std::uint16_t relative_chip_frame=0u;
        std::uint16_t port=0u,value=0u;
    };
    struct Clip {
        std::array<Event,kMaxSourceEvents> events{};
        std::uint16_t count=0u;
    };
    struct Group {
        Clip on{},off{};
        std::uint32_t elapsed=0u;
        std::uint16_t next_on=0u;
        bool active=false,started=false;
    };
    std::array<Group,2> notes_{};
    bool ready_=false;
    static void executeFrame(awe64::Chip& chip,Group& g) noexcept;
};
} // namespace awe64::vst3
