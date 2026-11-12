#pragma once
// P002: original Creative SF2 0x94-byte note-start descriptor transition.
// Input is an ALREADY MERGED genuine format-2 voice descriptor, not a bank.
// This does not manufacture template defaults, sample data, or NoteOn timing.
#include "CreativeControllerVolume.h"
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
}
