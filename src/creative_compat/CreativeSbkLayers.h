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
enum class OriginalCreativeBankFormat : std::uint8_t {
    Format1Sbk=1,
    Format2Sf2=2
};
struct SbkLayer {
    OriginalCreativeBankFormat source_format=OriginalCreativeBankFormat::Format1Sbk;
    std::uint16_t bank=0, preset_bag=0, instrument=0, instrument_bag=0;
    std::uint8_t program=0, key=0, velocity=0;
    std::uint16_t rom_sample_selector=0; // NOT an index into Creative shdr
    SbkGeneratorScope preset_global{}, preset_local{}, instrument_global{}, instrument_local{};
    // Original 1MGM PCOD generator8 dispatcher directly doubles the
    // instrument raw value; also verified by actual GM0/GM40 chip traces.
    // No instrument generator: return nullopt, not a guessed chip cutoff.
    [[nodiscard]] std::optional<std::uint16_t> instrumentRawCutoff() const noexcept {
        const auto local=instrument_local.get(8);
        return local?local:instrument_global.get(8);
    }
    // Restricted original 1MGM voice-cutoff candidate. The owner's
    // original GM0 C4 zone gen8=89 -> chip cutoff178 and GM40 C4
    // gen8=105 -> cutoff210; no per-program branching. This rule has
    // NOT been shown equivalent for additive preset gen8 offsets,
    // drum bank128, or missing input. In those cases fail closed.
    [[nodiscard]] std::optional<std::uint8_t>
    candidateOriginal1MgmInstrumentCutoff() const noexcept {
        if(source_format!=OriginalCreativeBankFormat::Format1Sbk ||
           bank>=128 || program>127 || key>127 || velocity==0 ||
           velocity>127 || preset_global.get(8) || preset_local.get(8))
            return std::nullopt;
        const auto raw=instrumentRawCutoff();
        if(!raw || *raw>127)return std::nullopt;
        return static_cast<std::uint8_t>(*raw*2u);
    }
    [[nodiscard]] std::optional<std::uint16_t> presetRawCutoff() const noexcept {
        const auto local=preset_local.get(8);
        return local?local:preset_global.get(8);
    }
    // Source-verified original Creative SF1 (ifil=1) Bank Builder:
    // operator48 instVol is a signed 16-bit WORD at descriptor +0x60.
    // The format-1 load converter PCOD+0x37BA yields
    // trunc((127 - signed(raw))*75/10). The LAST explicit scope wins:
    // instrument-local > instrument-global > preset-local > preset-global.
    // This is NOT the separately set DEVICE-STATE attenuation at +0x10.
    // No missing-generator default is synthesized from an unknown template.
    [[nodiscard]] std::optional<std::int16_t>
    candidateOriginalSf1InstVolFromGenerator48() const noexcept {
        if(source_format!=OriginalCreativeBankFormat::Format1Sbk)
            return std::nullopt;
        const auto raw=instrument_local.get(48).has_value()
            ? instrument_local.get(48)
            : instrument_global.get(48).has_value()
            ? instrument_global.get(48)
            : preset_local.get(48).has_value()
            ? preset_local.get(48) : preset_global.get(48);
        if(!raw)return std::nullopt;
        const auto signedRaw=static_cast<std::int16_t>(*raw);
        const auto original=(127-static_cast<std::int32_t>(signedRaw))*75/10;
        // The original writes only the resulting low WORD. Stay within
        // verified signed-descriptor range until wrap cases are checked.
        if(original<-32768 || original>32767)return std::nullopt;
        return static_cast<std::int16_t>(original);
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
// Actual Creative FORMAT-2/SF2 PCOD+0x3985 load arithmetic. Its preset
// operator offsets use PCOD+0x3AF7 (some are C(x)-C(0)); neither is
// interchangeable with the old original 48-byte-shdr FORMAT-1/SBK.
[[nodiscard]] std::optional<std::int16_t>
candidateOriginalSf2InstrumentLoadGenerator(
    std::uint16_t generator_id,std::int16_t original_raw) noexcept;
[[nodiscard]] std::optional<std::int16_t>
candidateOriginalSf2PresetDeltaGenerator(
    std::uint16_t generator_id,std::int16_t original_raw) noexcept;
// Bounded FORMAT-2 voice generator merge: original source-supported
// cutoff (8), volume attack timecents (34), and instVol (48) only.
// Within each level, local overrides global. Preset deltas and instrument
// values use their respective PCOD converters, then signed-saturating add.
// Missing scope values require explicitly verified PRECONVERTED descriptor
// template defaults. Unknown operator semantics fail closed; this is not
// a claim of general SF2 generator/voice-descriptor parity.
[[nodiscard]] std::optional<std::int16_t>
candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
    const SbkLayer& layer,std::uint16_t generator_id,
    std::optional<std::int16_t> verified_preset_template = {},
    std::optional<std::int16_t> verified_instrument_template = {}) noexcept;
// Existing generator48/instVol entrypoint retained for compatibility.
// It delegates to the SAME verified format-2 merge implementation.
[[nodiscard]] std::optional<std::int16_t>
candidateOriginalSf2InstVolFromExplicitScopes(
    const SbkLayer& layer,
    std::optional<std::int16_t> verified_preset_template = {},
    std::optional<std::int16_t> verified_instrument_template = {}) noexcept;
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
