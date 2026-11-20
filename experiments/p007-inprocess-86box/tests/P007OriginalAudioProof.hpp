#pragma once
// P007Y: independent numerical proof gate for ORIGINAL guest audio acceptance.
// A nonzero PCM buffer following NoteOn is NOT enough: Win95 startup sounds,
// stale pre-note FIFO samples, or a constant test tone can cause false PASS.
// Capture baseline PCM while the genuine WinMM guest is READY, but before
// MIDI NoteOn. Require a substantially stronger post-NoteOn audio window.
// This gate is pure C++, allocation-free and never modifies EMU8000 DSP.
#include <algorithm>
#include <array>
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
    // P007BN: test-host-only, constant-memory, 44.1-kHz original-guest
    // eight-band IIR fingerprint. Independent frequency regions detect
    // unequal timbres whose RMS/peak/first-difference roughness coincide.
    // This deliberately does NOT alter any chip, DSP or VST3 audio path.
    std::array<double,7> lowL{},lowR{};
    std::array<long double,8> bandEnergy{};
    void append(float l,float r) noexcept {
        // One-pole lowpasses at 125, 250, 500, 1000, 2000, 4000, 8000 Hz.
        // Decays derived from exp(-2*pi*cutoff/44100), not host rate.
        constexpr std::array<double,7> decay{{
            0.98234816937947178,0.96500792588319928,
            0.93124029701739430,0.86720849078904483,
            0.75205056649661273,0.56558005456787619,
            0.31988079812500181
        }};
        double precedingL=0.,precedingR=0.;
        for(std::size_t i=0;i<decay.size();++i) {
            lowL[i]=decay[i]*lowL[i]+(1.-decay[i])*l;
            lowR[i]=decay[i]*lowR[i]+(1.-decay[i])*r;
            const double bandL=lowL[i]-precedingL;
            const double bandR=lowR[i]-precedingR;
            bandEnergy[i]+=static_cast<long double>(bandL)*bandL+
                           static_cast<long double>(bandR)*bandR;
            precedingL=lowL[i];precedingR=lowR[i];
        }
        const double highL=static_cast<double>(l)-precedingL;
        const double highR=static_cast<double>(r)-precedingR;
        bandEnergy[7]+=static_cast<long double>(highL)*highL+
                       static_cast<long double>(highR)*highR;
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

// P007BN: normalized spectral-shape difference in [0,2], from EIGHT
// independent 44.1k-frequency regions. A gain-only change cancels out;
// no per-sample equality or phase matching is required across Win95 takes.
inline double spectralShapeDistance(const Window& x,
                                    const Window& y) noexcept {
    long double totalX=0.,totalY=0.;
    for(std::size_t i=0;i<x.bandEnergy.size();++i) {
        totalX+=x.bandEnergy[i];totalY+=y.bandEnergy[i];
    }
    if(!(totalX>1.e-14L)||!(totalY>1.e-14L)||
       !std::isfinite(static_cast<double>(totalX))||
       !std::isfinite(static_cast<double>(totalY)))return 2.;
    long double difference=0.;
    for(std::size_t i=0;i<x.bandEnergy.size();++i)
        difference+=std::abs(x.bandEnergy[i]/totalX-
                             y.bandEnergy[i]/totalY);
    return static_cast<double>(difference);
}

// P007BT: A/B/A must match A's TIMBRE and overall gain, NOT the single
// largest transient PCM sample peak. Win95 guest scheduling and sample
// playback phase can shift one peak of the same patch by 15%-20%, even
// when RMS, roughness and the eight normalized spectral bands recover A.
// Independent original-Win95 PCM/EMU8000 register and SF2 ACK remain required.
inline bool restoredAfterAlternate(const Window& first,
                                   const Window& alternate,
                                   const Window& restored) noexcept {
    if(!distinguishBanks(first,alternate) ||
       !distinguishBanks(restored,alternate) ||
       first.frames<33075u || restored.frames<33075u ||
       first.rms()<0.00001 || restored.rms()<0.00001)
        return false;
    const double level=restored.rms()/first.rms();
    if(!std::isfinite(level)||level<0.85||level>1.15)return false;
    const double roughFirst=first.roughness();
    const double roughRestored=restored.roughness();
    if(!std::isfinite(roughFirst)||!std::isfinite(roughRestored) ||
       std::abs(roughFirst-roughRestored)>
           std::max(0.02,0.25*roughFirst))
        return false;
    const double different=spectralShapeDistance(first,alternate);
    const double returned=spectralShapeDistance(first,restored);
    if(!std::isfinite(different)||!std::isfinite(returned) ||
       different<0.20)return false;
    // Keep the adversarial P007BN A/B/C fake-bank rejection.
    return returned<=std::max(0.15,0.5*different);
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
