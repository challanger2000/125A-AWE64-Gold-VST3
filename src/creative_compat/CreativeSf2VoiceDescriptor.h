#pragma once
// P002: original Creative SF2 0x94-byte note-start descriptor transition.
// Input is an ALREADY MERGED genuine format-2 voice descriptor, not a bank.
// This does not manufacture template defaults, sample data, or NoteOn timing.
#include "CreativeControllerVolume.h"
#include "CreativeSf2RamSamplePcm.h"
#include "CreativeSbkLayers.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
namespace awe64::creative {
// Original public Creative SFTYPE.H: 59 signed WORD generators then sample
// metadata with 32-bit DWORDs packed to 2-byte alignment. Do not replace
// these coordinates with an unverified host/compiler default struct layout.
#pragma pack(push,2)
struct OriginalCreativeSf2VoiceLayout {
    std::array<std::int16_t,59> generator_words{}; // 0x00..0x75
    std::uint32_t dwStart=0;                     // 0x76
    std::uint32_t dwEnd=0;                       // 0x7a
    std::uint32_t dwStartLoop=0;                 // 0x7e
    std::uint32_t dwEndLoop=0;                   // 0x82
    std::uint32_t dwPitch=0;                     // 0x86
    std::uint8_t originalKey=0;                  // 0x8a
    std::int8_t pitchCorrection=0;               // 0x8b
    std::uint16_t sampleType=0;                  // 0x8c
    std::uint16_t sampleLink=0;                  // 0x8e
    std::uint32_t presetGroupId=0;               // 0x90
};
#pragma pack(pop)
static_assert(std::is_standard_layout_v<OriginalCreativeSf2VoiceLayout>);
static_assert(sizeof(OriginalCreativeSf2VoiceLayout)==0x94u);
static_assert(alignof(OriginalCreativeSf2VoiceLayout)==2u);
static_assert(offsetof(OriginalCreativeSf2VoiceLayout,dwStart)==0x76u);
static_assert(offsetof(OriginalCreativeSf2VoiceLayout,dwPitch)==0x86u);
static_assert(offsetof(OriginalCreativeSf2VoiceLayout,originalKey)==0x8au);
static_assert(offsetof(OriginalCreativeSf2VoiceLayout,sampleType)==0x8cu);
static_assert(offsetof(OriginalCreativeSf2VoiceLayout,presetGroupId)==0x90u);
inline constexpr std::size_t kCreativeSf2VoiceDescriptorBytes=0x94u;
using CreativeSf2VoiceDescriptorBytes =
    std::array<std::uint8_t,kCreativeSf2VoiceDescriptorBytes>;
struct OriginalSf2VoiceStartConditions {
    std::uint16_t creative_bank;
    std::uint8_t midi_key, midi_velocity;
    std::uint8_t channel_pan, channel_chorus, channel_reverb;
    std::uint8_t device_effects_flag;
    OriginalCreativeVolumeTerms volume; // CC7/CC11 and original device +0x10
};
// Returns the original format-2 descriptor after PCOD+0x3BD4 and the
// recovered VXD per-voice arithmetic, or nullopt for unsupported conditions.
// Word offsets are 2*operator for operators 0..58; sample metadata starts at
// 0x76. Everything not explicitly transformed is preserved byte-for-byte.
// Only ROM-mono sampleType 0x8001 has been verified. The original note-start
// clears that qualifier in its outgoing 148-byte record to 0x0001.
// Exact source-executed ORIGINAL SF2 note-start pitch calculation from
// the already verified descriptor dwPitch at +0x86, root/scale/coarse/fine
// generators 58/56/51/52, signed sample pitch correction +0x8B, and
// explicit per-channel tuning terms. Unlike FORMAT-1, the driver uses
// the fixed integer factor 874 and emits the upper WORD of the resulting
// 24-bit coordinate (0xE000 for 44.1 kHz, root==played key).
// Original rootKey=-1 means use shdr.byOrigKey; keys outside 0..127 and
// unsupported negative intermediate are refused rather than guessed.
// 0xFFFFFF is the source-confirmed maximum 24-bit pitch coordinate.
[[nodiscard]] std::optional<std::uint16_t>
candidateOriginalSf2InitialIpFromDescriptor(
    const CreativeSf2VoiceDescriptorBytes& original_voice,
    std::uint8_t played_key,
    std::int16_t verified_channel_tune_1_cents,
    std::int16_t verified_channel_tune_2_cents) noexcept;
// Reuse the established 2-write DATA3 IP register contract. The physical
// EMU8000 voice is already assigned by the caller; this is not a second
// allocator or a generic register scheduler. Voices 30/31 are reserved.
[[nodiscard]] std::optional<std::array<PortWord,2>>
candidateOriginalSf2InitialIpWritesFromDescriptor(
    const CreativeSf2VoiceDescriptorBytes& original_voice,
    std::uint8_t played_key,std::uint8_t physical_voice,
    std::int16_t verified_channel_tune_1_cents,
    std::int16_t verified_channel_tune_2_cents) noexcept;
// The ORIGINAL format-2 builder performs two separate source-normalized
// levels and signed-saturating addition across all 59 WORD fields. The
// format-2 PRECONVERTED template WORDS must come from the original driver;
// they are supplied explicitly because its template at LCOD+0x2D88 is
// not yet pinned in a public/reproducible test vector. Never substitute
// SoundFont-spec defaults or all-zero templates for original-driver facts.
// Output only populates the 0x00..0x75 generator region; 0x76..0x93 is
// left untouched for the existing verified sample-metadata entrypoint.
[[nodiscard]] std::optional<CreativeSf2VoiceDescriptorBytes>
candidateOriginalSf2GeneratorsWithVerifiedTemplates(
    const SbkLayer& selected_layer,
    const std::array<std::int16_t,59>& verified_original_preset_template,
    const std::array<std::int16_t,59>& verified_original_instrument_template) noexcept;
// Source-observed first stage of original SBAWE.VXD PCOD+0x3735
// sample-rate->dwPitch: rate*0x2F8E in LOW 32 bits, then left-normalize
// until bit31 is set. The original uses bits30..24 as a table index and
// bits23..17 as the interpolation fraction (both seven bits).
// The exact original table formats and interpolation/exponent arithmetic
// are now recovered. Coordinates remain useful independently for debugging.
struct OriginalCreativeSf2PitchLookupCoordinates {
    std::uint32_t wrapped_product=0;
    std::uint32_t normalized_word=0;
    std::uint8_t normalization_left_shifts=0;
    std::uint8_t table_index=0;
    std::uint8_t fraction_7bit=0;
};
[[nodiscard]] std::optional<OriginalCreativeSf2PitchLookupCoordinates>
originalCreativeSf2PitchLookupCoordinates(
    std::uint32_t sample_rate_hz) noexcept;
// Original SBAWE.VXD PCOD+0x3735, disassembled directly from the hash-locked
// 77,370-byte driver. Table A: PDAT+0x8E8 (128 unsigned DWORDs); table B:
// PDAT+0xAE8 (128 signed BYTES). The caller supplies these PRIVATE original
// tables after LE-page-map and SHA-256 verification; they are not distributed
// in this repository, nor fabricated from floating-point approximations.
struct OriginalCreativeSf2PitchTables {
    std::array<std::uint32_t,128> base{};
    std::array<std::int8_t,128> slope{};
};
[[nodiscard]] std::optional<std::uint32_t>
originalCreativeSf2SampleRateToDwPitch(
    std::uint32_t sample_rate_hz,
    const OriginalCreativeSf2PitchTables& verified_original_tables) noexcept;
// Build the real shdr sample metadata with the same verified original
// table-driven conversion, rather than requiring a precomputed dwPitch.
[[nodiscard]] std::optional<CreativeSf2VoiceDescriptorBytes>
candidateOriginalSf2VoiceWithOriginalRomPitchTables(
    const CreativeSf2VoiceDescriptorBytes& merged_generator_words,
    const SbkRomAddress& actual_original_sf2_rom_sample,
    const OriginalCreativeSf2PitchTables& verified_original_tables,
    std::uint32_t original_normalized_preset_group_id) noexcept;
// Complete original 1996 format-2 PCOD voice-building stages using ONLY
// original-verified per-driver resources, already selected SF2 zone and ROM
// sample header. Original tables/template are caller-owned/private; neither
// the original binary bytes nor a fake log/pow approximation is embedded.
// The preset template is a separate source stage (zero in the four captured
// original cases), and the authentic instrument template comes from VXD
// LCOD+0x2D88. Both remain explicit to prevent hidden guessed defaults.
struct OriginalCreativeSf2DriverResources {
    std::array<std::int16_t,59> preset_template{};
    std::array<std::int16_t,59> instrument_template{};
    OriginalCreativeSf2PitchTables pitch_tables{};
};
struct OriginalCreativeSf2NoteDescriptors {
    CreativeSf2VoiceDescriptorBytes before_note_start{};
    CreativeSf2VoiceDescriptorBytes after_note_start{};
};
[[nodiscard]] std::optional<OriginalCreativeSf2NoteDescriptors>
candidateOriginalSf2NoteFromVerifiedVxdResources(
    const SbkLayer& selected_layer,
    const SbkRomAddress& original_rom_sample,
    const OriginalCreativeSf2DriverResources& source_verified_resources,
    std::uint32_t original_normalized_preset_group_id,
    const OriginalSf2VoiceStartConditions& explicit_channel_device_state) noexcept;
// Source-mapped RAM-SF2 equivalent of the existing genuine ROM descriptor
// builder. P002 RESEARCH: uses the already independently verified actual
// physical EMU8000 DRAM upload and original Creative VXD PCOD lookup
// resources; generates 148-byte format-2 generator/metadata records
// *from MIDI zone + shdr*, not from recorded note-port clips.
// RAM sampleType remains 1, without ROM-specific +16 attenuation.
// Full voice allocation, timed startup and original/native PCM parity
// remain independent tests, NOT claimed here.
[[nodiscard]] std::optional<OriginalCreativeSf2NoteDescriptors>
candidateOriginalSf2RamNoteFromVerifiedVxdResources(
    const SbkLayer& selected_layer,
    const OriginalSf2RamSamplePcmView& real_sf2_ram_sample,
    const OriginalSf2RamPhysicalLayer& original_physical_ram_address,
    const OriginalCreativeSf2DriverResources& source_verified_resources,
    std::uint32_t original_normalized_preset_group_id,
    const OriginalSf2VoiceStartConditions& explicit_channel_device_state,
    bool independently_verified_original_win95_ram_upload) noexcept;
// Populate the exact ORIGINAL SF2 sample metadata tail of an otherwise
// already-merged 148-byte voice, from the SAME existing ROM shdr resolver.
// The original PCOD+0x3735 sample-rate -> dwPitch has not yet been
// independently implemented here. Its true result and the original
// normalized preset group are therefore EXPLICIT proven caller inputs.
// A missing verified pitch returns nullopt; never use approximate log2.
[[nodiscard]] std::optional<CreativeSf2VoiceDescriptorBytes>
candidateOriginalSf2VoiceWithVerifiedRomSampleMetadata(
    const CreativeSf2VoiceDescriptorBytes& merged_generator_words,
    const SbkRomAddress& actual_original_sf2_rom_sample,
    std::optional<std::uint32_t> verified_original_dw_pitch,
    std::uint32_t original_normalized_preset_group_id) noexcept;
[[nodiscard]] std::optional<CreativeSf2VoiceDescriptorBytes>
candidateOriginalSf2EncodedVoiceDescriptor(
    const CreativeSf2VoiceDescriptorBytes& merged,
    const OriginalSf2VoiceStartConditions& context) noexcept;
// RAM mono uses the same original Creative PCOD converter, but authentic
// RAM samples have no ROM-only +16 level correction, and are prevalidated
// as physical RAM sampleType=0x0001. Reject an unverified source.
[[nodiscard]] std::optional<CreativeSf2VoiceDescriptorBytes>
candidateOriginalSf2EncodedRamVoiceDescriptor(
    const CreativeSf2VoiceDescriptorBytes& merged,
    const OriginalSf2VoiceStartConditions& context,
    bool independently_verified_original_ram_source) noexcept;
}
