#pragma once
// P002: read original Win95 SBAWE.VXD synthesizer state image, rather than
// accepting three caller-invented pitch state operands per NoteOn.
//
// Genuine original format-2 mono channel/voice layout (LCOD+0x6072 and
// source NoteOn LCOD+0x5B4D/0x5D7B):
//   voice = device_state + 0x42 + 0x20*physical (30 music voices)
//   channel = device_state + 0x442 + 0x24*midi_channel (16 channels)
//   channel +0x0a/+0x0b: two original LFO1 channel BYTES,
//   channel +0x0e signed WORD; physical voice +0x0e unsigned WORD;
//   channel +0x10/+0x12 signed WORD pitch cents; +0x14 DWORD.
// Original x86 LCOD+0x574F writes the computed NoteOn initial IP into
// voice+0x0e before LCOD+0x5B54 later reads it. Source +0x5CEB uses
// channel+0x0a/+0x0b; +0x56DE uses channel+0x10/+0x12.
// Uses original ROM id at device+0x158e ("1MGM") and original dry
// mode word at device+0x12. The synth state must be an independently
// verified original snapshot: it is NOT synthesized by this adapter.
//
// VXD allocation/CC lifecycle and chip-reads are not reconstructed here.
// Original negative/positive attenuation tables come from private source
// SHA-locked VXD resource extractor; no copyrighted tables embedded.
#include "CreativeSf2OriginalTargets.h"
#include "CreativeSf2FullNoteOnChip.h"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
namespace awe64::creative {
inline constexpr std::size_t kOriginalCreativeMidiChannels=16u;
inline constexpr std::size_t kOriginalVxdVoiceRowStart=0x42u;
inline constexpr std::size_t kOriginalVxdVoiceRowStride=0x20u;
inline constexpr std::size_t kOriginalVxdChannelRowStart=0x442u;
inline constexpr std::size_t kOriginalVxdChannelRowStride=0x24u;
inline constexpr std::size_t kOriginalVxdRomIdOffset=0x158eu;
inline constexpr std::size_t kOriginalVxdMinDeviceImage=0x1592u;
struct OriginalSf2VerifiedVxdDeviceSnapshot {
    std::span<const std::uint8_t> original_state_bytes{};
    bool independently_verified_original_vxd_snapshot=false;
    bool original_lcod_31word_table_verified=false;
    std::array<std::uint16_t,16> source_positive_16word_table{};
    std::array<std::uint16_t,15> source_negative_15word_table{};
};
struct OriginalSf2VxdSnapshotVoice {
    OriginalSf2FullNoteOnVoice assigned_voice{};
    std::uint8_t original_midi_channel=0;
};
struct OriginalSf2ResolvedSnapshotNote {
    std::array<OriginalSf2FullNoteOnVoice,
               kOriginalCreativeMaxNoteStartDescriptors> voices{};
    std::size_t count=0;
};
// Apply only the independently identified ORIGINAL voice-state write
// (LCOD+0x574F) to a mutable VXD device-state image. ALL voices are
// validated first; any invalid later layer leaves the image untouched.
// This is the pre-register first phase; allocation, MIDI state lifecycle
// and actual Win95 OS initialization remain separate work.
// Original LCOD+0x3DF8 writes the allocated voice marker, and
// LCOD+0x5A9A/+0x5AA3 copies the already ENCODED per-descriptor
// generator30/mod-release and generator38/volume-release BYTES into
// physical voice+0x15/+0x16. All voices are preflighted before state
// modification. This function does not invent release rates or sample
// END bookkeeping; input descriptor must be source proven.
[[nodiscard]] bool candidateOriginalSf2StoreNoteOnSourceReleaseState(
    std::span<std::uint8_t> original_device_state,
    bool independently_verified_original_device_state,
    std::span<const OriginalSf2VxdSnapshotVoice> assigned) noexcept;
[[nodiscard]] bool candidateOriginalSf2WriteInitialIpToVxdState(
    std::span<std::uint8_t> original_device_state,
    bool independently_verified_original_device_state,
    std::span<const OriginalSf2VxdSnapshotVoice> assigned) noexcept;
[[nodiscard]] std::optional<OriginalSf2VxdFinalTargetInputs>
candidateOriginalSf2TargetStateFromVxdImage(
    const OriginalSf2VerifiedVxdDeviceSnapshot& original,
    std::uint8_t midi_channel,
    std::uint8_t physical_voice) noexcept;
[[nodiscard]] std::optional<OriginalSf2ResolvedSnapshotNote>
candidateOriginalSf2ResolveVxdSnapshotVoices(
    std::span<const OriginalSf2VxdSnapshotVoice> assigned,
    const OriginalSf2VerifiedVxdDeviceSnapshot& snapshot) noexcept;
[[nodiscard]] std::optional<OriginalSf2FullNoteOnPlan>
candidateOriginalSf2FullNoteOnFromVxdSnapshot(
    std::span<const OriginalSf2VxdSnapshotVoice> assigned,
    const OriginalSf2VerifiedVxdDeviceSnapshot& original) noexcept;
// Real pinned turican0 EMU8000, no added DSP or P001 source changes.
[[nodiscard]] bool applyOriginalCreativeSf2FullNoteOnFromVxdSnapshot(
    awe64::Chip& chip,
    std::span<const OriginalSf2VxdSnapshotVoice> assigned,
    const OriginalSf2VerifiedVxdDeviceSnapshot& original) noexcept;
} // namespace awe64::creative
