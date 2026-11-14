#pragma once
// P003: literal RIFF/SoundFont preset inventory; NOT a voice renderer.
// Preserves original physical PHDR order, bank, MIDI program and names.
// No fabricated missing programs/empty 'slots' and no synthetic defaults.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace awe64::creative {
struct Sf2PresetIdentity {
    std::array<char,21> name{}; // exactly 20 original PHDR bytes + final NUL
    std::uint16_t bank=0;
    std::uint8_t program=0;
    std::uint16_t original_phdr_row=0;
    std::uint16_t first_pbag=0;
    std::uint16_t last_pbag=0;
};
using Sf2PresetInventory=std::vector<Sf2PresetIdentity>;
// Off the audio callback ONLY. Malformed/duplicate/ambiguous SoundFonts
// are refused. Returning zero entries is never a silent 128-preset fill.
[[nodiscard]] std::optional<Sf2PresetInventory>
inspectOriginalSoundFontPresetInventory(
    std::span<const std::uint8_t> riff);
[[nodiscard]] bool hasExactSoundFontPreset(
    std::span<const Sf2PresetIdentity> original,
    std::uint16_t bank,std::uint8_t program) noexcept;
} // namespace awe64::creative
