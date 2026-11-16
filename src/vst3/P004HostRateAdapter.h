#pragma once
// P004: host-rate conversion ONLY. The unchanged EMU8000 and the CT1745
// always produce genuine 44,100-Hz source samples. Never resample their
// register clock or change original MIDI-to-register algorithms.
//
// The adapter is causal: it never reads a native sample whose time is
// after the next host output frame. A 48-tap Blackman-windowed sinc with
// 24 native frames' group delay is used on non-native host rates.
// All tables are prepared off the audio thread in configure(); next()
// is fixed-storage, allocation-free, lock-free and deterministic.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

namespace awe64::vst3 {
class P004HostRateAdapter final {
public:
    struct Stereo {float left=0.f; float right=0.f;};
    static constexpr std::uint32_t kChipHz=44100u;
    static constexpr std::size_t kTaps=48u;
    static constexpr std::size_t kPhases=1024u;
    static constexpr std::size_t kHistory=64u;
    static constexpr std::uint32_t kGroupDelayNative=24u;

    [[nodiscard]] static constexpr bool supports(std::uint32_t rate) noexcept {
        return rate==44100u || rate==48000u ||
               rate==88200u || rate==96000u;
    }
    // configure() is non-real-time. Use exact supported integer host
    // rates; no silent fallback to wrong clock timing.
    [[nodiscard]] bool configure(std::uint32_t hostHz) noexcept {
        if(!supports(hostHz))return false;
        hostHz_=hostHz;
        if(hostHz_!=kChipHz)prepareCoefficients();
        reset();
        return true;
    }
    void reset() noexcept {
        for(auto& frame:history_)frame={};
        nextSourceFrame_=0u;
        hostFrame_=0u;
    }
    [[nodiscard]] std::uint32_t rate() const noexcept {return hostHz_;}
    [[nodiscard]] std::uint64_t consumedSourceFrames() const noexcept {
        return nextSourceFrame_;
    }
    [[nodiscard]] std::uint64_t producedHostFrames() const noexcept {
        return hostFrame_;
    }
    [[nodiscard]] std::uint32_t latencySamples() const noexcept {
        return hostHz_==kChipHz?0u:
            static_cast<std::uint32_t>(
                (std::uint64_t(kGroupDelayNative)*hostHz_+kChipHz-1u)/kChipHz);
    }
    // The callback returns ONE genuine 44.1-kHz stereo frame. It is
    // invoked only as the host sample-clock actually crosses a native
    // chip frame boundary. No pre-render past a future host MIDI event.
    // Only rates >= 44100 are supported, so at most one chip frame is
    // needed for each output sample.
    template<class NativeFrame>
    [[nodiscard]] Stereo next(NativeFrame&& nativeFrame) noexcept {
        const auto numerator=hostFrame_*std::uint64_t(kChipHz);
        const auto target=numerator/hostHz_;
        while(nextSourceFrame_<=target) {
            history_[nextSourceFrame_%kHistory]=nativeFrame();
            ++nextSourceFrame_;
        }
        const auto remainder=static_cast<std::uint32_t>(numerator%hostHz_);
        const auto phase=std::min<std::size_t>(
            kPhases-1u,(std::uint64_t(remainder)*kPhases+
                         hostHz_/2u)/hostHz_);
        const auto& kernel=coefficients_[phase];
        double left=0.,right=0.;
        for(std::size_t tap=0;tap<kTaps;++tap) {
            if(target<tap)continue; // silent native preroll, not wrapping
            const auto source=target-tap;
            if(source>=nextSourceFrame_ ||
               nextSourceFrame_-source>kHistory)continue;
            const auto& frame=history_[source%kHistory];
            left+=double(frame.left)*kernel[tap];
            right+=double(frame.right)*kernel[tap];
        }
        ++hostFrame_;
        return {static_cast<float>(left),static_cast<float>(right)};
    }
private:
    void prepareCoefficients() noexcept {
        // Preserve 44.1-kHz direct path without ANY interpolation.
        // kGroupDelayNative is intentionally an integer chip-frame
        // delay; report its rounded-up host-frame latency to the DAW.
        constexpr double pi=std::numbers::pi_v<double>;
        constexpr double cutoff=0.94; // below original chip Nyquist
        for(std::size_t phase=0;phase<kPhases;++phase) {
            const double frac=double(phase)/double(kPhases);
            double sum=0.;
            for(std::size_t tap=0;tap<kTaps;++tap) {
                const double x=frac-double(kGroupDelayNative)+double(tap);
                const double arg=pi*cutoff*x;
                const double sinc=(std::abs(arg)<1e-12)?
                    1.:std::sin(arg)/arg;
                const double m=double(tap)/double(kTaps-1u);
                const double blackman=0.42-0.5*std::cos(2.*pi*m)+
                                      0.08*std::cos(4.*pi*m);
                const double coefficient=cutoff*sinc*blackman;
                coefficients_[phase][tap]=
                    static_cast<float>(coefficient);
                sum+=coefficient;
            }
            // Unit DC gain, including fractional chip-frame phases.
            for(auto& coefficient:coefficients_[phase])
                coefficient=static_cast<float>(
                    double(coefficient)/sum);
        }
    }
    std::uint32_t hostHz_=kChipHz;
    std::uint64_t hostFrame_=0u;
    std::uint64_t nextSourceFrame_=0u;
    std::array<Stereo,kHistory> history_{};
    std::array<std::array<float,kTaps>,kPhases> coefficients_{};
};
} // namespace awe64::vst3
