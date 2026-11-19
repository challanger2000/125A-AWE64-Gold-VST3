// P007AS SYNTHETIC ONLY: adversarial QA for the filter-Sustain spectral
// classifier on mildly drifting reference tones. Does NOT create/ship audio,
// emulate Creative hardware, or replace owner-private original PCM evidence.
#include "P007ALCorrectedSpectralProof.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace p007;
using original_spectral_proof::Capture;
using original_spectral_proof::Target;
namespace {
void check(bool ok,const char* what) {
    if(!ok){std::fprintf(stderr,"P007AS_SYNTHETIC_FAIL %s\n",what);std::exit(1);}
}
void setBand3(Capture& c,unsigned bin,double offsetDb) {
    const double total=c.totalPower[bin];
    const double frac=.10*std::pow(10.,offsetDb/10.);
    c.bandPower[bin][3u]=total*frac;
    for(unsigned band=0;band<7u;++band)
        if(band!=3u)c.bandPower[bin][band]=total*(1.-frac)/6.;
}
struct Captures{Capture a{},b{},back{};};
Captures fixture() {
    Captures z{};
    for(Capture* c:{&z.a,&z.b,&z.back}) {
        for(unsigned bin=0;bin<original_spectral_proof::bins;++bin) {
            c->count[bin]=original_spectral_proof::binFrames;
            c->totalPower[bin]=220.5;
            setBand3(*c,bin,0.);
        }
    }
    // Deliberately not the user-provided CSV: independent plausible synthetic
    // magnitudes near 0.8..0.95 dB, mild nonstationarity + two noisy tails.
    constexpr std::array<double,7> returnDriftDb{
        -.16,-.19,-.256,-.233,-.28,-.40,-.36
    };
    for(unsigned i=6u;i<13u;++i) {
        setBand3(z.b,i,-.95);
        setBand3(z.back,i,returnDriftDb[i-6u]);
    }
    return z;
}
}
int main(){
    auto f=fixture();
    const auto result=corrected_filter_proof::evaluate(
        f.a,f.b,f.back,Target::Sustain);
    check(result.proven,"coherent ~0.8dB, 1s Sustain effect wrongly missed");
    check(result.longestRun>=4u,"Sustain passed without four adjacent 250ms windows");

    // Deliberately force every common false-positive shape to fail.
    check(!corrected_filter_proof::evaluate(
            f.a,f.a,f.back,Target::Sustain).proven,
          "no-change A/A/return drift falsely passed");
    check(!corrected_filter_proof::evaluate(
            f.a,f.b,f.b,Target::Sustain).proven,
          "failed A/return restoration falsely passed");

    const auto unchanged=fixture();
    for(unsigned affected:{1u,2u,3u}) {
        auto only=unchanged;
        for(unsigned i=6u+affected;i<13u;++i)setBand3(only.b,i,0.);
        check(!corrected_filter_proof::evaluate(
                only.a,only.b,only.back,Target::Sustain).proven,
              "fewer than four affected windows falsely passed");
    }

    auto alternating=fixture();
    for(unsigned i=6u;i<13u;++i)
        setBand3(alternating.b,i,(i&1u)?+.95:-.95);
    check(!corrected_filter_proof::evaluate(
            alternating.a,alternating.b,alternating.back,Target::Sustain).proven,
          "alternating-sign Sustain effect falsely passed");

    auto tiny=fixture();
    for(unsigned i=6u;i<13u;++i)setBand3(tiny.b,i,-.20);
    check(!corrected_filter_proof::evaluate(
            tiny.a,tiny.b,tiny.back,Target::Sustain).proven,
          "tiny change below measured A/return drift falsely passed");

    auto drifting=fixture();
    for(unsigned i=6u;i<13u;++i)setBand3(drifting.back,i,-.85);
    check(!corrected_filter_proof::evaluate(
            drifting.a,drifting.b,drifting.back,Target::Sustain).proven,
          "unreliable, unreturned original spectral reference falsely passed");

    auto unstable=fixture();
    for(unsigned i=9u;i<13u;++i)setBand3(unstable.back,i,-.85);
    check(!corrected_filter_proof::evaluate(
            unstable.a,unstable.b,unstable.back,Target::Sustain).proven,
          "only three stable bins plus four unstable bins falsely passed");

    // This change is Sustain-only; original strong-proof thresholds must
    // remain untouched for Attack, Decay and Release.
    for(auto target:{Target::Attack,Target::Decay,Target::Release}) {
        const auto a=corrected_filter_proof::evaluate(
            f.a,f.a,f.a,target);
        check(!a.proven,"unchanged non-Sustain control falsely passed");
    }
    std::puts("P007AS_SYNTHETIC_FOUR_WINDOW_SUSTAIN_BAND3_PASS");
    std::puts("P007AS_ADVERSARIAL_NON_EFFECT_DRIFT_NONRESTORE_SHORT_EFFECT_PASS");
    std::puts("P007AS_NOT_ORIGINAL_CREATIVE_AUDIO_PROOF");
}
