// P007AL QA of the CLASSIFIER and original-Creative NRPN15 transmitter.
// All generated tones are synthetic; only a separately run private Win95
// 86Box/EMU8000 host may establish original-guest audio evidence.
#include "P007ALCorrectedSpectralProof.hpp"
#include "P007OriginalEnvFilterDepth.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace p007;
static constexpr double pi=3.14159265358979323846;
static void check(bool ok,const char* why) {
    if(!ok){std::fprintf(stderr,"P007AL_QA_FAIL %s\n",why);std::exit(1);}
}
static original_spectral_proof::Capture synth(
    original_spectral_proof::Target target,bool changed,
    double restoreDrift=1.,double tailDecay=1.5) {
    original_spectral_proof::Capture cap{};
    const unsigned noteFrames=original_spectral_proof::noteBins*
                              original_spectral_proof::binFrames;
    const unsigned totalFrames=original_spectral_proof::bins*
                               original_spectral_proof::binFrames;
    for(unsigned n=0;n<totalFrames;++n) {
        const bool released=n>=noteFrames;
        const double t=double(n)/44100.;
        const double tTail=released?double(n-noteFrames)/44100.:0.;
        double high=restoreDrift;
        if(changed) {
            bool inTarget=false;
            switch(target) {
                case original_spectral_proof::Target::Attack:
                    inTarget=!released && t<1.5;break;
                case original_spectral_proof::Target::Decay:
                    inTarget=!released && t>=.25 && t<2.75;break;
                case original_spectral_proof::Target::Sustain:
                    inTarget=!released && t>=1.5;break;
                case original_spectral_proof::Target::Release:
                    inTarget=released;break;
            }
            if(inTarget)high=1.35;
        }
        const double fade=released?std::exp(-tailDecay*tTail):1.;
        const float x=static_cast<float>(fade*(
              .085*std::sin(2.*pi*220.*t)
             +.004*high*std::sin(2.*pi*2440.*t)));
        cap.append(x,x,released);
    }
    return cap;
}
int main(){
    auto tx=original_filter_env_depth::Transmitter{};
    tx.armUnsigned(15u,1800u);
    std::array<original_filter_env_depth::MidiCC,4> packets{};
    for(auto& out:packets) {
        check(tx.active(),"NRPN15 prematurely finished");
        check(tx.tick([&](std::uint8_t controller,std::uint8_t value)noexcept{
            out={controller,value};return true;
        }),"NRPN15 packet transmit failed");
    }
    check(!tx.active(),"NRPN15 unfinished");
    // Creative NRPN8 (Mod/Filter Sustain): original documented offset
    // 8192 + 127 must give the same CC6/CC38 bytes as P007Native GUI.
    tx.armUnsigned(8u,8192u+127u);
    for(auto& out:packets) {
        check(tx.tick([&](std::uint8_t controller,std::uint8_t value)noexcept{
            out={controller,value};return true;
        }),"NRPN8 packet transmit failed");
    }
    check(!tx.active(),"NRPN8 unfinished");
    check(packets[1].controller==98u && packets[1].value==8u &&
          packets[2].value==64u && packets[3].value==127u,
          "Creative filter ENV sustain NRPN8 offset encoding wrong");
    check(packets[0].controller==99u && packets[0].value==127u,
          "Creative NRPN MSB wrong");
    check(packets[1].controller==98u && packets[1].value==15u,
          "Creative Volume Envelope2 Release NRPN wrong");
    check(packets[2].controller==6u && packets[2].value==14u &&
          packets[3].controller==38u && packets[3].value==8u,
          "Creative 1800*4ms 14-bit raw encoding wrong");
    using namespace original_spectral_proof;
    for(const auto target:{Target::Attack,Target::Decay,
                           Target::Sustain,Target::Release}) {
        const auto original=synth(target,false);
        const auto changed=synth(target,true);
        auto back=synth(target,false);
        const auto e=corrected_filter_proof::evaluate(original,changed,back,target);
        check(e.proven,"medium sized repeatable 0.25+dB effect missed");
        if(target==Target::Sustain) {
            // High-band A-return drift must not veto a stable lower band.
            auto noisyHighBand=back;
            for(unsigned i=6u;i<noteBins;++i)
                for(unsigned band=4u;band<bands;++band)
                    noisyHighBand.bandPower[i][band]*=10.;
            const auto local=corrected_filter_proof::evaluate(
                original,changed,noisyHighBand,target);
            check(local.proven,
                  "unrelated drifting high bands veto stable coherent sustain effect");
        }
        check(e.longestRun>=3u,"not three consecutive windows");
        check(!corrected_filter_proof::evaluate(original,original,back,target).proven,
              "unchanged original incorrectly proven");
        const auto veryShortTail=synth(target,false,1.,22.);
        if(target==Target::Release) {
            const auto shortChanged=synth(target,true,1.,22.);
            const auto shortEvidence=corrected_filter_proof::evaluate(
                veryShortTail,shortChanged,veryShortTail,target);
            check(shortEvidence.insufficientReleaseSignal && !shortEvidence.proven,
                  "too-short amplifier release incorrectly passed filter release");
        }
        back=synth(target,false,2.);
        check(!corrected_filter_proof::evaluate(original,changed,back,target).proven,
              "unrestored spectral reference incorrectly passed");
    }
    std::puts("P007AL_NRPN15_CREATIVE_ENCODING_AND_A_B_A_SPECTRAL_CLASSIFIER_PASS");
    std::puts("P007AL_SYNTHETIC_QA_ONLY_NO_ORIGINAL_WIN95_CLAIM");
    return 0;
}
