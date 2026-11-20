#pragma once
// Original Win95 Creative SBAWE.VXD idle-voice chooser (mono mode 0).
// This is the *proven empty-voice branch* at VXD LCOD +0x3EA1..+0x3FE8,
// not its unimplemented live-envelope priority/voice-stealing branch.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
namespace awe64::creative {
constexpr std::size_t kOriginalCreativeMusicVoices=30;
struct OriginalVxdIdleVoiceProbe {
    // Original VXD's internal channel/key marker, not a MIDI channel number.
    std::uint16_t driver_key_marker=0x0ffe;
    // High 16 bits of the EMU8000 voice-target register the original
    // allocator reads before choosing a completely idle physical voice.
    std::uint16_t chip_voice_target_high16=0;
    // Caller proves the original driver's additional eligibility predicate;
    // unavailable data MUST NOT be guessed true by an adapter.
    bool driver_eligible=false;
};
// Original VXD's three passes: 0,3,...; 1,4,...; 2,5,...
// Returns nullopt unless a verified completely idle voice is available.
// Physical voice 30/31 are *never* musical allocation candidates.
// busy is our native controller's occupied mask, not a replacement for
// original VXD state. Exact timestamps/steal decisions are out of scope.
[[nodiscard]] std::optional<std::uint8_t> candidateOriginalWin95IdleMonoVoice(
    const std::array<OriginalVxdIdleVoiceProbe,kOriginalCreativeMusicVoices>& probes,
    const std::array<bool,kOriginalCreativeMusicVoices>& busy,
    std::size_t original_available_voices=kOriginalCreativeMusicVoices) noexcept;
} // namespace awe64::creative
