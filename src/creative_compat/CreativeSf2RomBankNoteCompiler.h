#pragma once
// Source-constrained, complete ROM-mono format2 SF2 note compilation.
// One actual MIDI NoteOn -> real original SF2 bank zone selection ->
// source-resolved 46-byte ROM shdr -> ORIGINAL VXD 59-WORD converter ->
// original 148-byte poststart descriptors -> original Win95 VXD 30-voice
// runtime state -> exact 61/122/.. typed OUTs.
//
// Deliberately requires explicitly verified original VXD template,
// original PCOD pitch tables, actual driver-state snapshot, dry MIDI
// channel terms and physical voice choices. This is NOT an invented
// allocator, raw-SF2 spec DSP, RAM SoundFont support, dynamic host MIDI
// polyphony, or original Win95 digital PCM parity proof.
//
// Safe to call during plugin activation or OFF the realtime thread.
// No heap allocations in this compiler, no chip port writes on failure.
#include "CreativeSf2VxdStateSnapshot.h"
#include "CreativeSf2VoiceDescriptor.h"
#include "CreativeSf2VxdLiveAllocator.h"
#include "CreativeSf2VxdSourceRelease.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace awe64::creative {
struct OriginalSf2CompiledRomBankNote {
    std::array<OriginalSf2VxdSnapshotVoice,
               kOriginalCreativeMaxNoteStartDescriptors> assignments{};
    std::array<CreativeSf2VoiceDescriptorBytes,
               kOriginalCreativeMaxNoteStartDescriptors> before_note_start{};
    std::array<std::uint8_t,kOriginalVxdMinDeviceImage> vxd_state_after_noteon{};
    OriginalSf2FullNoteOnPlan typed_original_outs{};
    std::uint8_t voice_count=0u;
    [[nodiscard]] std::span<const OriginalSf2VxdSnapshotVoice>
    selectedVoices() const noexcept {return {assignments.data(),voice_count};}
};
// All source trust flags are intentionally explicit. Caller cannot pass
// an arbitrary SF2 "close enough" default template or a device master
// volume mislabeled as original VXD state +0x10.
[[nodiscard]] std::optional<OriginalSf2CompiledRomBankNote>
candidateOriginalSf2CompileRomBankNote(
    std::span<const std::uint8_t> original_sf2_bank,
    const MidiMessage& original_note_on,
    const MidiState& original_midi_channel_state,
    std::uint16_t original_resolved_bank,
    std::size_t original_rom_sample_word_count,
    const OriginalCreativeSf2DriverResources& source_vxd_sf2_resources,
    const OriginalSf2VoiceStartConditions& source_device_and_channel_conditions,
    const OriginalSf2VerifiedVxdDeviceSnapshot& original_device_state,
    std::span<const std::uint8_t> independently_verified_physical_voices,
    bool original_rom_sf2_source_and_allocation_verified) noexcept;
struct OriginalSf2ExecutedLiveIdleMonoNote {
    OriginalWin95MonoSelection original_selection{};
    OriginalSf2CompiledRomBankNote original_compilation{};
};
// First complete SF2 bank MIDI NoteOn through ORIGINAL hardware-read
// physical voice choice and unchanged pinned EMU8000 execution. The
// original source mono allocator is permitted to select a voice only
// when it is COMPLETELY IDLE: no unproven busy voice-stealing, paired
// stereo assignment, or high-priority/running-voice overwrite.
// Requires exactly ONE matched original ROM-mono zone; multisample
// layers need a separate original sequential allocation contract.
// Invoke OFF the real-time thread in the diagnostic/research host.
[[nodiscard]] std::optional<OriginalSf2ExecutedLiveIdleMonoNote>
candidateOriginalSf2StartSingleRomBankLayerOnLiveChip(
    awe64::Chip& real_pinned_emu8000_chip,
    std::span<const std::uint8_t> original_sf2_bank,
    const MidiMessage& original_note_on,
    const MidiState& original_midi_channel_state,
    std::uint16_t original_resolved_bank,
    std::size_t original_rom_sample_word_count,
    const OriginalCreativeSf2DriverResources& source_vxd_sf2_resources,
    const OriginalSf2VoiceStartConditions& source_device_and_channel_conditions,
    const OriginalSf2VerifiedVxdDeviceSnapshot& original_device_state,
    bool original_rom_sf2_and_live_state_verified) noexcept;
// Source-verified SEQUENTIAL ROM-SF2 MIDI NoteOn with ORIGINAL live
// hardware allocator and a single mutable Win95 VXD device state.
// The existing single-note source bank compiler/chip writer is reused;
// only after its exact original 61-OUT execution succeeded is its
// source-generated 0x1592-byte AFTER-state copied into the caller state.
// The subsequent NoteOn sees prior busy keys and must find another
// independently original-identified COMPLETELY-IDLE physical voice.
// No arbitrary voice stealing, ROM sample magic, new DSP or fake preset.
// Intended for non-RT P002 research host until full driver provenance.
[[nodiscard]] std::optional<OriginalSf2ExecutedLiveIdleMonoNote>
candidateOriginalSf2StartNextIdleRomBankNoteOnLiveChip(
    awe64::Chip& real_pinned_emu8000_chip,
    std::span<std::uint8_t> source_mutable_vxd_state,
    std::span<const std::uint8_t> original_sf2_bank,
    const MidiMessage& original_note_on,
    const MidiState& original_midi_channel_state,
    std::uint16_t original_resolved_bank,
    std::size_t original_rom_sample_word_count,
    const OriginalCreativeSf2DriverResources& source_vxd_sf2_resources,
    const OriginalSf2VoiceStartConditions& source_device_and_channel_conditions,
    const OriginalSf2VerifiedVxdDeviceSnapshot& source_verified_vxd_context,
    bool independently_verified_source_state_and_bank) noexcept;
// Multiple original ROM SF2 instrument zones under ONE NoteOn.
// Original PCOD matches at most six zones. Each physical slot comes
// from an actual original VXD live COMPLETELY-IDLE selector pass, with
// previous selected slots provisionally marked occupied for the NEXT
// pass. The final multi-voice source plan is fully verified and only
// THEN written to the immutable original EMU8000 chip. No software DSP,
// guessed physical modulo allocation or busy-voice stealing. The
// exact historical microtiming between allocation passes and original
// individual voice OUTs remains an explicit unverified boundary.
struct OriginalSf2ExecutedLiveLayeredBankNote {
    std::array<OriginalWin95MonoSelection,
               kOriginalCreativeMaxNoteStartDescriptors> selections{};
    OriginalSf2CompiledRomBankNote original_compilation{};
};
[[nodiscard]] std::optional<OriginalSf2ExecutedLiveLayeredBankNote>
candidateOriginalSf2StartNextIdleLayeredRomBankNoteOnLiveChip(
    awe64::Chip& real_pinned_emu8000_chip,
    std::span<std::uint8_t> source_mutable_vxd_state,
    std::span<const std::uint8_t> original_sf2_bank,
    const MidiMessage& original_note_on,
    const MidiState& original_midi_channel_state,
    std::uint16_t original_resolved_bank,
    std::size_t original_rom_sample_word_count,
    const OriginalCreativeSf2DriverResources& source_vxd_sf2_resources,
    const OriginalSf2VoiceStartConditions& source_device_and_channel_conditions,
    const OriginalSf2VerifiedVxdDeviceSnapshot& source_verified_vxd_context,
    bool independently_verified_source_state_and_bank) noexcept;
// A source-verified 30-physical-voice bank session's MIDI release ledger.
// Register only real bank-compiled NoteOns already executed by the
// original hardware allocator; then use the exact Creative VXD NoteOff
// and 30-slot CC64 release. No invented sample envelopes or voice stealing.
// This is allocation-free and can be shared by the native bank research
// host and a later source-loaded VST3 instrument. Caller owns chip and
// original VXD memory and must not mutate them through another path.
class OriginalSf2RomBankVoiceOwnership {
public:
    [[nodiscard]] bool recordExecutedNoteOn(
        const OriginalSf2ExecutedLiveIdleMonoNote& executed,
        std::span<const std::uint8_t> post_noteon_vxd_state) noexcept;
    [[nodiscard]] bool recordExecutedNoteOn(
        const OriginalSf2ExecutedLiveLayeredBankNote& executed,
        std::span<const std::uint8_t> post_noteon_vxd_state) noexcept;
    [[nodiscard]] bool recordCompiledNoteOn(
        const OriginalSf2CompiledRomBankNote& compilation,
        std::span<const std::uint8_t> post_noteon_vxd_state) noexcept;
    [[nodiscard]] std::optional<OriginalSf2SourceNoteReleaseResult>
    noteOff(awe64::Chip& chip,
            std::span<std::uint8_t> mutable_vxd_state,
            std::uint8_t midi_channel,
            std::uint8_t midi_key) noexcept;
    [[nodiscard]] std::optional<OriginalSf2SourceCc64Result>
    controller64(awe64::Chip& chip,
                 std::span<std::uint8_t> mutable_vxd_state,
                 std::uint8_t midi_channel,
                 std::uint8_t controller_value) noexcept;
    [[nodiscard]] std::size_t trackedPhysicalVoices() const noexcept;
private:
    std::array<OriginalSf2VxdSnapshotVoice,
               kOriginalCreativeMusicVoices> source_voices_{};
    std::array<bool,kOriginalCreativeMusicVoices> occupied_{};
};
} // namespace awe64::creative
