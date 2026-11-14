#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
namespace awe64::vst3 {
// No disk work in the audio callback. Only call from setActive(false).
// Refuses overwrites, overflow, wrong sample count and oversized data.
[[nodiscard]] bool savePrivateNativeDryWav(
    const std::filesystem::path& destination,
    std::span<const std::int16_t> samples,
    bool overflow) noexcept;
} // namespace awe64::vst3
