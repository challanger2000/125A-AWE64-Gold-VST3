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
    long double deltaSquareSum=0.;
    double peak=0.;
    float lastL=0.f,lastR=0.f;
    bool haveLast=false;
    void append(float l,float r) noexcept {
        if(haveLast) {
            const double dl=static_cast<double>(l)-lastL;
            const double dr=static_cast<double>(r)-lastR;
            deltaSquareSum+=dl*dl+dr*dr;
        }
        lastL=l; lastR=r; haveLast=true;
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
    // Gain-invariant temporal spectral proxy, retaining actual signed PCM
    // differences. Two patches can have almost identical RMS and peak.
    [[nodiscard]] double roughness() const noexcept {
        return squareSum>1.e-14L && frames>1u
            ?std::sqrt(static_cast<double>(deltaSquareSum/squareSum)):0.;
    }
};
// P007AW: a deliberately conservative *additional* frequency-content gate.
// A new spectrum must differ substantially; loudness equality is not proof
// of identical waveforms. No test may assert original-guest provenance from
// this numerical comparator alone.
inline bool distinguishBanks(const Window& sf2,const Window& rom) noexcept {
    if(sf2.frames<33075u||rom.frames<33075u||
       sf2.rms()<0.00001||rom.rms()<0.00001||
       !std::isfinite(sf2.roughness())||!std::isfinite(rom.roughness()))
        return false;
    const double ratio=sf2.rms()/rom.rms();
    const bool levelDifferent=ratio>=1.15||ratio<=1./1.15;
    const bool peakDifferent=std::abs(sf2.peak-rom.peak)>
        std::max(0.001,rom.peak*.12);
    const double roughDelta=std::abs(sf2.roughness()-rom.roughness());
    const bool spectrumDifferent=roughDelta>
        std::max(0.015,rom.roughness()*.20);
    return levelDifferent||peakDifferent||spectrumDifferent;
}

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
