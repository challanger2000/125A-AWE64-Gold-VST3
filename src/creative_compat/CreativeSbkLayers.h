#pragma once
// P002: Creative 1MGM ROM descriptor only. Raw layer inventory, not a voice allocator.
// Preserve every generator word across four scopes; never invent missing values.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace awe64::creative {
struct SbkGeneratorScope {
    // Creative/SF2 generator word IDs 0..63; a missing generator is distinguishable from zero.
    std::array<std::optional<std::uint16_t>,64> raw{};
    [[nodiscard]] std::optional<std::uint16_t> get(unsigned id) const noexcept {
        return id<raw.size()?raw[id]:std::nullopt;
    }
};
struct SbkLayer {
    std::uint16_t bank=0, preset_bag=0, instrument=0, instrument_bag=0;
    std::uint8_t program=0, key=0, velocity=0;
    std::uint16_t rom_sample_selector=0; // NOT an index into Creative shdr
    SbkGeneratorScope preset_global{}, preset_local{}, instrument_global{}, instrument_local{};
    // Instrument generator 8 is demonstrably used for the verified GM0/GM40 cases.
    // No instrument generator: return nullopt, not a guessed chip cutoff.
    [[nodiscard]] std::optional<std::uint16_t> instrumentRawCutoff() const noexcept {
        const auto local=instrument_local.get(8);
        return local?local:instrument_global.get(8);
    }
    [[nodiscard]] std::optional<std::uint16_t> presetRawCutoff() const noexcept {
        const auto local=preset_local.get(8);
        return local?local:preset_global.get(8);
    }
    [[nodiscard]] std::optional<std::uint16_t> presetRawAttenuation() const noexcept {
        const auto local=preset_local.get(48);
        return local?local:preset_global.get(48);
    }
    [[nodiscard]] std::optional<std::uint16_t> instrumentRawAttackVolEnv() const noexcept {
        const auto local=instrument_local.get(34);
        return local?local:instrument_global.get(34);
    }
};
constexpr std::size_t kMaxCreative1MgmLayers=16;
struct SbkLayerSet {
    std::array<SbkLayer,kMaxCreative1MgmLayers> layers{};
    std::uint8_t count=0;
};
struct SbkRomAddress {
    std::uint16_t sample_id=0;
    std::uint32_t start_word=0, end_word=0, loop_start_word=0, loop_end_word=0;
};
// Creative 1MGM packs THREE 16-byte ROM sample-address records into each
// historical 48-byte shdr row, unlike generic SF2 sample headers.
// Word addresses must stay within the supplied original-ROM size.
[[nodiscard]] std::optional<SbkRomAddress> resolveCreative1MgmRomAddress(
    std::span<const std::uint8_t> sbk, std::uint16_t sample_id,
    std::size_t original_rom_word_count) noexcept;
// Run off the audio thread. Empty result = valid bank but no matching layer.
// nullopt = malformed/unsupported bank, invalid MIDI input, or layer overflow.
// This does NOT select physical EMU8000 voices or resolve sample addresses.
[[nodiscard]] std::optional<SbkLayerSet> resolveCreativeSbkLayers(
    std::span<const std::uint8_t> raw, std::uint16_t bank,
    std::uint8_t program, std::uint8_t key, std::uint8_t velocity) noexcept;
}
