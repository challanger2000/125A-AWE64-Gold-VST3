#pragma once
// PRIVATE original Creative 86Box PCM differential evidence helper.
// A function's successful NRPN transmission is NOT an audible effect proof.
// Capture note onsets under identical original-guest conditions; verify both
// repeated original-reference stability and A/B/A return to original.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace p007::original_control_proof {
struct Window {
 std::uint64_t stereoFrames=0u;
 std::uint64_t nonzero=0u;
 double sumSquares=0.;
 double sumDerivative=0.;
 double peak=0.;
 double lastLeft=0.,lastRight=0.;
 bool first=true;
 void append(float left,float right) noexcept {
  if(!std::isfinite(left)||!std::isfinite(right))return;
  const double l=left,r=right;
  sumSquares+=l*l+r*r;
  peak=std::max(peak,std::max(std::abs(l),std::abs(r)));
  nonzero+=(left!=0.f?1u:0u)+(right!=0.f?1u:0u);
  if(!first)sumDerivative+=(l-lastLeft)*(l-lastLeft)+
                            (r-lastRight)*(r-lastRight);
  lastLeft=l;lastRight=r;first=false;++stereoFrames;
 }
 [[nodiscard]] double rms() const noexcept {
  return stereoFrames?std::sqrt(sumSquares/(double(stereoFrames)*2.)):0.;
 }
 // Low-pass movement changes this value even when loudness stays similar.
 [[nodiscard]] double roughness() const noexcept {
  return sumSquares>1.e-16?std::sqrt(sumDerivative/sumSquares):0.;
 }
};
struct Take {Window onset,sustain,tail;};
inline bool nonSilent(const Take& x) noexcept {
 return x.onset.stereoFrames>=20000u &&
        x.onset.nonzero>=1024u && x.onset.rms()>=.00001;
}
inline double relativeChange(double a,double b) noexcept {
 if(!std::isfinite(a)||!std::isfinite(b))return 0.;
 return std::abs(a-b)/std::max({std::abs(a),std::abs(b),.000001});
}
inline bool stable(const Take& a,const Take& b) noexcept {
 return nonSilent(a)&&nonSilent(b) &&
        relativeChange(a.onset.rms(),b.onset.rms())<.28 &&
        relativeChange(a.onset.roughness(),b.onset.roughness())<.32;
}
inline bool cutoffEffect(const Take& baseline,const Take& changed,
                         const Take& restored) noexcept {
 return stable(baseline,restored)&&
        (relativeChange(baseline.onset.rms(),changed.onset.rms())>.22 ||
         relativeChange(baseline.onset.roughness(),
                        changed.onset.roughness())>.18);
}
inline bool attackEffect(const Take& baseline,const Take& slow,
                         const Take& restored) noexcept {
 return stable(baseline,restored) &&
        slow.onset.rms()<baseline.onset.rms()*.68;
}
inline bool sendTailEffect(const Take& dry,const Take& wet,
                           const Take& dryAgain) noexcept {
 return stable(dry,dryAgain) &&
        wet.tail.stereoFrames>=20000u &&
        wet.tail.rms()>std::max(dry.tail.rms()*1.45,.000005);
}
inline bool chorusEffect(const Take& dry,const Take& wet,
                         const Take& dryAgain) noexcept {
 return stable(dry,dryAgain) &&
       (relativeChange(dry.sustain.rms(),wet.sustain.rms())>.13 ||
        relativeChange(dry.sustain.roughness(),
                       wet.sustain.roughness())>.16);
}
} // namespace p007::original_control_proof
