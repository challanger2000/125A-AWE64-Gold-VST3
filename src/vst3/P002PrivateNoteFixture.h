#pragma once
// Opt-in PRIVATE ROM SF2 source-diagnostic for the current P001 VST3.
// This is deliberately NOT a production bank loader, voice allocator,
// 16-channel MIDI host, or original-Win95/PCM-parity release.
// No original VXD, SF2, sound samples, or LCOD tables in the binary.
// One SHA-locked x86 note handoff, one program/key/velocity, one NoteOn.
// All parsing happens at setActive(), NEVER on the real-time audio thread.
#include "CreativeSf2VxdStateSnapshot.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace awe64::vst3 {
struct P002PrivateNoteFixture {
    std::uint8_t program=0;
    std::uint8_t channel=0;
    std::uint8_t key=0;
    std::uint8_t velocity=0;
    std::uint8_t voice_count=0;
    std::array<std::uint8_t,awe64::creative::kOriginalVxdMinDeviceImage> original_device_state{};
    std::array<std::uint16_t,15> negative_attenuation{};
    std::array<std::uint16_t,16> positive_attenuation{};
    std::array<awe64::creative::OriginalSf2VxdSnapshotVoice,
               awe64::creative::kOriginalCreativeMaxNoteStartDescriptors> voices{};
    [[nodiscard]] std::span<const awe64::creative::OriginalSf2VxdSnapshotVoice>
    selectedVoices() const noexcept {
        return {voices.data(),voice_count};
    }
    [[nodiscard]] awe64::creative::OriginalSf2VerifiedVxdDeviceSnapshot
    sourceState() const noexcept {
        awe64::creative::OriginalSf2VerifiedVxdDeviceSnapshot state{};
        state.original_state_bytes=original_device_state;
        state.independently_verified_original_vxd_snapshot=true;
        state.original_lcod_31word_table_verified=true;
        state.source_negative_15word_table=negative_attenuation;
        state.source_positive_16word_table=positive_attenuation;
        return state;
    }
};
[[nodiscard]] bool loadP002PrivateNoteFixture(
    const std::filesystem::path& private_file,
    P002PrivateNoteFixture& out,
    std::string& error);
} // namespace awe64::vst3
