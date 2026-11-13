#pragma once
// AWE64 Gold CT1745 WAVETABLE ONLY. Source: PINNED turican0/86Box-AWE32
// v6.0-AWE32-0.93 snd_sb.c: sb_get_wavetable_buffer_sb16_awe32,
// sb_ct1745_mixer_write/reset, and include/86box/filters.h.
// Independent 86Box original-function oracle lives in the native tests.
//
// This is a *per-VST3-instance* state holder for the unchanged source
// formulas, not a new EQ/mixer algorithm or duplicate EMU8000 DSP.
// EMU8000 chip already applies pan/chorus/reverb/INIT-EQ/-1.1dB.
// This stage must be downstream of Chip::render(). Never clamp to PCM16
// for the FLOAT sink. The old PCM16 chip tap remains separately available.
// No PC Sound Blaster DSP, FM, CD, mic, ISA, mixer window or GUI included.
#include <array>
#include <cstddef>
#include <cstdint>

namespace awe64::card {

struct OriginalGoldFrame final {
    std::int32_t left{};
    std::int32_t right{};
};

class OriginalCt1745WavetableMixer final {
public:
    OriginalCt1745WavetableMixer() noexcept { reset(); }
    void reset() noexcept;
    // Only independently captured original CT1745 register writes may
    // override hardware reset values. This prevents using guessed
    // nonneutral Win95 mixer state before original driver capture.
    [[nodiscard]] bool applyVerifiedOriginalRegister(
        std::uint8_t reg, std::uint8_t value,
        bool sourceRegisterWriteIndependentlyVerified) noexcept;
    [[nodiscard]] std::uint8_t registerValue(std::uint8_t reg) const noexcept {
        return registers_[reg];
    }
    // One pair, taken AFTER unchanged Chip::render(). No allocation,
    // locks, filesystem or I/O. Returned signed int32 is the fork's
    // wavetable handler value BEFORE its float sink conversion.
    [[nodiscard]] OriginalGoldFrame process(
        std::int32_t chipLeft, std::int32_t chipRight) noexcept;

    // Exact original 86Box float wavetable sink: no PCM16 clip.
    static float floatSink(std::int32_t postCt1745) noexcept {
        return static_cast<float>(postCt1745) / 32768.0f;
    }
    // Historical AWE32_WAV *diagnostic* pre-card PCM tap ONLY.
    static float preCardPcm16Tap(std::int32_t preCt1745) noexcept;

private:
    struct OriginalForkBiquad final {
        double x[2][3]{};
        double y[2][3]{};
        [[nodiscard]] double apply(
            unsigned channel, double sample,
            const std::array<double,3>& a,
            const std::array<double,3>& b) noexcept;
    };

    void decode() noexcept;
    std::array<std::uint8_t,256> registers_{};
    std::array<double,2> fm_{};
    std::array<double,2> master_{};
    std::array<double,2> outputGain_{};
    std::array<unsigned,2> bass_{};
    std::array<unsigned,2> treble_{};
    OriginalForkBiquad low_{};
    OriginalForkBiquad lowCut_{};
    OriginalForkBiquad high_{};
    OriginalForkBiquad highCut_{};
};

} // namespace awe64::card
