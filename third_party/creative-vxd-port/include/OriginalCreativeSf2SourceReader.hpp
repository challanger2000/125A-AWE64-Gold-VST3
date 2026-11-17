#pragma once
// SF2 structural reader for the original-driver port. Parses ORIGINAL RIFF
// tables without substituting SoundFont defaults for missing Creative state.
// All storage is bounded; no heap, I/O, Windows API or host callbacks.
// This is a source reader, NOT by itself a proven 1996 Creative converter.
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <optional>

namespace awe64::original_vxd {
inline constexpr std::size_t sf2_max_selected_layers=6u;
inline constexpr std::size_t sf2_max_scope_modulators=32u;
struct Sf2Modulator {
    std::uint16_t source=0,destination=0,amount_source=0,transform=0;
    std::int16_t amount=0;
};
struct Sf2ModulatorScope {
    std::array<Sf2Modulator,sf2_max_scope_modulators> entries{};
    std::uint8_t count=0;
};
struct Sf2GeneratorScope {
    std::array<std::uint16_t,59> raw{};
    std::uint64_t present=0u;
    [[nodiscard]] constexpr bool has(unsigned g) const noexcept {
        return g<59u && (present&(std::uint64_t{1}<<g))!=0;
    }
    [[nodiscard]] constexpr std::uint16_t get(unsigned g) const noexcept {
        return g<59u?raw[g]:0u;
    }
};
struct Sf2SampleHeader {
    std::uint32_t start=0,end=0,loop_start=0,loop_end=0,sample_rate=0;
    std::uint8_t original_pitch=0;
    std::int8_t correction=0;
    std::uint16_t sample_link=0,sample_type=0;
};
struct Sf2SelectedLayer {
    std::uint16_t bank=0,preset_row=0,preset_bag=0;
    std::uint16_t instrument=0,instrument_bag=0,sample_id=0;
    std::uint8_t program=0,key=0,velocity=0;
    Sf2GeneratorScope preset_global{},preset_local{};
    Sf2GeneratorScope instrument_global{},instrument_local{};
    Sf2ModulatorScope preset_global_mod{},preset_local_mod{};
    Sf2ModulatorScope instrument_global_mod{},instrument_local_mod{};
    Sf2SampleHeader sample{};
};
enum class Sf2ReadStatus:std::uint8_t {
    Ok,NoMatch,Malformed,Unsupported,TooManyLayers
};
struct Sf2LayerSelection {
    Sf2ReadStatus status=Sf2ReadStatus::Malformed;
    std::array<Sf2SelectedLayer,sf2_max_selected_layers> layers{};
    std::uint8_t count=0;
};
struct Sf2PresetIdentity {
    std::uint16_t bank=0;
    std::uint8_t program=0;
};
// Non-owning source metadata. Does not infer a VXD bank/row or pad programs.
[[nodiscard]] std::optional<Sf2PresetIdentity> source_sf2_preset_identity(
    std::span<const std::uint8_t> file,std::uint16_t preset_row) noexcept;
// Select one actual PHDR row. Distinct source rows with identical
// bank/program bytes must not be merged into one VXD-selected preset.
[[nodiscard]] Sf2LayerSelection select_sf2_raw_layers_by_preset(
    std::span<const std::uint8_t> file,std::uint16_t preset_row,
    std::uint8_t key,std::uint8_t velocity) noexcept;
// Accepts fully formed standard RIFF sfbk format-2, with original offsets
// kept in the result. Zero matching zones is distinct from malformed data.
// Original SF2 modulator records are preserved per scope, never ignored.
// The separate Creative resource compiler refuses unverified active mods.
// An instrument's generator list ends at sampleID, as in original PCOD.
// Exposes the original uncompressed SF2 smpl PCM16-LE stream without
// copying the bank. Must remain alive through an off-audio-thread upload.
[[nodiscard]] std::optional<std::span<const std::uint8_t>>
source_sf2_raw_sample_pool(std::span<const std::uint8_t> file) noexcept;
[[nodiscard]] Sf2LayerSelection select_sf2_raw_layers(
    std::span<const std::uint8_t> file,std::uint16_t bank,
    std::uint8_t program,std::uint8_t key,std::uint8_t velocity) noexcept;
} // namespace awe64::original_vxd
