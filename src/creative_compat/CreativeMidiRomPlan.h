#pragma once
// P002 independent MIDI NoteOn -> ROM-layer descriptors; NOT EMU8000 voice allocation.
#include "CreativeMidi.h"
#include "CreativeSbkLayers.h"
namespace awe64::creative {
struct CreativeRomPlannedLayer {
    SbkLayer metadata{};
    SbkRomAddress address{};
};
struct CreativeRomNotePlan {
    std::uint8_t channel=0,program=0,key=0,velocity=0;
    std::uint16_t original_sbk_bank=0;
    std::array<CreativeRomPlannedLayer,kMaxCreative1MgmLayers> layers{};
    std::uint8_t count=0;
};
// The active original Creative SBK bank must be explicitly resolved by the
// caller; CC0/CC32 and channel-10 default-bank semantics remain oracle work.
// A valid key with zero matching layers is different from a corrupt bank.
[[nodiscard]] std::optional<CreativeRomNotePlan> prepareOriginalCreativeRomNote(
    const MidiMessage& note_on, const MidiState& state,
    std::span<const std::uint8_t> original_sbk, std::uint16_t resolved_bank,
    std::size_t original_rom_word_count) noexcept;
}
