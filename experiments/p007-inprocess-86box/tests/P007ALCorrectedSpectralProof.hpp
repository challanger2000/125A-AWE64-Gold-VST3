#pragma once
// P007AL: corrected ORIGINAL-Creative PCM spectral A/B/A acceptance.
// Uses the untouched P007AK Capture filterbank. Avoids false 4-dB cutoff,
// requires three consecutive quarter-second windows, validates A-restoration,
// and refuses filter-release claims when the independent volume envelope
// already faded below -100 dBFS. No audio is synthesized here.
#include "P007OriginalSpectralProof.hpp"
#include <algorithm>
#include <cmath>

namespace p007::corrected_filter_proof {
using Capture=p007::original_spectral_proof::Capture;
using Target=p007::original_spectral_proof::Target;
inline constexpr unsigned noteBins=p007::original_spectral_proof::noteBins;
inline constexpr unsigned bins=p007::original_spectral_proof::bins;
inline constexpr unsigned bands=p007::original_spectral_proof::bands;
struct Evidence {
    bool proven=false, referenceUnstable=false, insufficientReleaseSignal=false;
    unsigned comparable=0, unstable=0, longestRun=0, usableReleaseBins=0;
    unsigned peakBand=0, peakBin=0;
    double peakDb=0.,maxReferenceDriftDb=0.;
};
inline Evidence evaluate(const Capture& a,const Capture& b,
                         const Capture& back,Target target) noexcept {
    Evidence e{};
    unsigned first=0u,last=noteBins;
    switch(target) {
        case Target::Attack: first=0;last=6;break;
        case Target::Decay: first=1;last=11;break;
        case Target::Sustain: first=6;last=noteBins;break;
        case Target::Release: first=noteBins;last=bins;break;
    }
    // A meaningful filter-release comparison requires the separate volume
    // envelope to leave the source audible in THREE adjacent time windows.
    if(target==Target::Release) {
        unsigned contiguous=0;
        for(unsigned i=first;i<last;++i) {
            bool hasSignal=true;
            for(const auto* take:{&a,&b,&back}) {
                if(take->count[i]<2000u ||
                   take->totalPower[i]/(2.*double(take->count[i]))<=1.e-10)
                    hasSignal=false;
            }
            if(hasSignal) {
                ++e.usableReleaseBins;
                ++contiguous;
                if(contiguous>=3) e.insufficientReleaseSignal=false;
            } else contiguous=0;
        }
        // Reject one or two isolated tails even if mathematically nonzero.
        unsigned run=0;
        bool hasThree=false;
        for(unsigned i=first;i<last;++i) {
            bool ok=true;
            for(const auto* take:{&a,&b,&back}) {
                if(take->count[i]<2000u ||
                   take->totalPower[i]/(2.*double(take->count[i]))<=1.e-10)
                    ok=false;
            }
            run=ok?run+1:0;
            hasThree=hasThree||run>=3;
        }
        e.insufficientReleaseSignal=!hasThree;
    }
    // Reference drift is evaluated WITHIN each independent band. A noisy
    // unrelated band cannot invalidate a coherent A/B/A effect elsewhere.
    // This fixes the P007AM false negative for Sustain: stable band #3
    // across seven consecutive windows, but weaker upper bands drift.
    bool hasStableEffectBand=false;
    for(unsigned band=0;band<bands;++band) {
        unsigned run=0,localLongest=0,localUnstable=0;
        int previousSign=0;
        unsigned previousBin=bins;
        for(unsigned i=first;i<last;++i) {
            const Capture* takes[3]{&a,&b,&back};
            double values[3]{};
            bool valid=true;
            for(unsigned j=0;j<3;++j) {
                const auto& t=*takes[j];
                if(t.count[i]<2000u || t.totalPower[i]<1.e-20) {
                    valid=false;break;
                }
                const double rmsSq=t.totalPower[i]/(2.*double(t.count[i]));
                if(rmsSq<=(i>=noteBins?1.e-14:1.e-10)) {
                    valid=false;break;
                }
                const double share=t.bandPower[i][band]/t.totalPower[i];
                if(!std::isfinite(share)||share<1.e-4) {
                    valid=false;break;
                }
                values[j]=10.*std::log10(share);
            }
            if(!valid) {run=0;previousSign=0;continue;}
            ++e.comparable;
            const double drift=std::abs(values[0]-values[2]);
            e.maxReferenceDriftDb=std::max(e.maxReferenceDriftDb,drift);
            if(drift>.30) {++e.unstable;++localUnstable;run=0;previousSign=0;continue;}
            const double delta=values[1]-(values[0]+values[2])*.5;
            if(std::abs(delta)>e.peakDb) {
                e.peakDb=std::abs(delta);
                e.peakBand=band;
                e.peakBin=i;
            }
            const int sign=delta>=0.?1:-1;
            if(std::abs(delta)>std::max(.25,5.*drift+.05)) {
                run=(previousBin+1==i && sign==previousSign)?run+1:1;
                e.longestRun=std::max(e.longestRun,run);
                localLongest=std::max(localLongest,run);
                previousSign=sign;previousBin=i;
            } else {run=0;previousSign=0;}
        }
        if(localLongest>=3u && localUnstable<=std::max(1u,(last-first)/4u))
            hasStableEffectBand=true;
    }
    e.proven=hasStableEffectBand && !e.insufficientReleaseSignal;
    e.referenceUnstable=!hasStableEffectBand &&
        e.unstable>=4u && 3u*e.unstable>e.comparable;
    return e;
}
inline const char* verdict(const Evidence& e) noexcept {
    if(e.insufficientReleaseSignal) return "INSUFFICIENT_RELEASE_SIGNAL";
    if(e.referenceUnstable) return "UNSTABLE_A_RETURN";
    return e.proven?"MEASURABLE_SPECTRAL_CHANGE":"INCONCLUSIVE";
}
} // namespace p007::corrected_filter_proof
