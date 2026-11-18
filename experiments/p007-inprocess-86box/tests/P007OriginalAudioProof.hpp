#pragma once
// P007Y: independent numerical proof gate for ORIGINAL guest audio acceptance.
// A nonzero PCM buffer following NoteOn is NOT enough: Win95 startup sounds,
// stale pre-note FIFO samples, or a constant test tone can cause false PASS.
// Capture baseline PCM while the genuine WinMM guest is READY, but before
// MIDI NoteOn. Require a substantially stronger post-NoteOn audio window.
// This gate is pure C++, allocation-free and never modifies EMU8000 DSP.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace p007::original_audio_proof {
struct Window {
    std::uint64_t frames=0;
    std::uint64_t nonzero=0;
    long double squareSum=0.;
    double peak=0.;
    void append(float l,float r) noexcept {
        ++frames;
        const double left=std::abs(static_cast<double>(l));
        const double right=std::abs(static_cast<double>(r));
        if(left>0.000001 || right>0.000001)++nonzero;
        peak=std::max(peak,std::max(left,right));
        squareSum+=static_cast<long double>(l)*l+
                   static_cast<long double>(r)*r;
    }
    [[nodiscard]] double rms() const noexcept {
        return frames?std::sqrt(static_cast<double>(
            squareSum/(2.*static_cast<long double>(frames)))):0.;
    }
};
inline bool accept(const Window& before,const Window& after,
                   double sampleRate=44100.) noexcept {
    if(!std::isfinite(sampleRate)||sampleRate<22050.||sampleRate>384000.)
        return false;
    if(before.frames < static_cast<std::uint64_t>(sampleRate*.5) ||
       after.frames < static_cast<std::uint64_t>(sampleRate*.75))
        return false;
    if(after.nonzero < 512u || !std::isfinite(after.peak) ||
       after.peak < 0.00001) return false;
    const double preRms=before.rms(), postRms=after.rms();
    if(!std::isfinite(preRms)||!std::isfinite(postRms))return false;
    // Floor avoids accepting mere dithering in an otherwise silent guest.
    return postRms >= 0.00001 && postRms > preRms*3.;
}
} // namespace p007::original_audio_proof
