// P007AK deterministic public filterbank checks. Synthetic signals validate
// the ANALYZER ONLY; these are NEVER a claim about original Win95/EMU8000 PCM.
#include "P007OriginalSpectralProof.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace p007::original_spectral_proof;
static constexpr double pi=3.14159265358979323846;
static void check(bool ok,const char* why) {
  if(!ok){std::fprintf(stderr,"P007AK_ANALYZER_TEST_FAIL %s\n",why);std::exit(1);}
}
static Capture render(Target target,bool changed,double backDrift=1.) {
  Capture c{};
  const unsigned noteLength=noteBins*binFrames;
  const unsigned tailLength=tailBins*binFrames;
  for(unsigned i=0;i<noteLength+tailLength;++i){
    const bool released=i>=noteLength;
    const double t=double(i)/44100.;
    const double phaseTime=released?double(i-noteLength)/44100.:t;
    double multiplier=backDrift;
    if(changed) {
      switch(target){
        case Target::Attack:
          if(!released && t<1.5) { multiplier=9.; }
          break;
        case Target::Decay:
          if(!released && t>=.25 && t<2.75) { multiplier=9.; }
          break;
        case Target::Sustain:
          if(!released && t>=1.5) { multiplier=9.; }
          break;
        case Target::Release:
          if(released) { multiplier=9.; }
          break;
      }
    }
    const double fade=released?std::exp(-phaseTime*1.6):1.;
    const float x=float(fade*(.085*std::sin(2.*pi*220.*t)+
                                  .004*multiplier*std::sin(2.*pi*2440.*t)));
    c.append(x,x,released);
  }
  return c;
}
int main() {
  for(const auto type:{Target::Attack,Target::Decay,Target::Sustain,Target::Release}) {
    const auto a=render(type,false);
    const auto b=render(type,true);
    const auto unchanged=render(type,false);
    const auto proof=evaluate(a,b,unchanged,type);
    check(proof.proven,"sustained reproducible target-dependent spectral difference was missed");
    check(proof.strongestDb>4.,"no measurable frequency dependent shift");
    check(!evaluate(a,a,unchanged,type).proven,"false positive when A=B=A");
    const auto drifted=render(type,false,5.);
    check(!evaluate(a,b,drifted,type).proven,
          "false positive when original A return did not restore");
  }
  std::puts("P007AK_SYNTHETIC_ANALYZER_A_B_A_4_TARGETS_NO_FALSE_POSITIVES_PASS");
  std::puts("P007AK_ANALYZER_ONLY_NOT_AN_ORIGINAL_CREATIVE_AUDIO_PROOF");
  return 0;
}
