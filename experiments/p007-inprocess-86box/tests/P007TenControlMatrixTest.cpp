#include "P007TenControlMatrix.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace p007::ten_matrix;
namespace {
void sound(p::Window& w,double amplitude,double harmonic=0.2) {
 for(unsigned i=0;i<44100u;++i) {
  const auto t=double(i)/44100.;
  const float x=static_cast<float>(amplitude*
   (std::sin(2.*3.141592653589793*440.*t)+
    harmonic*std::sin(2.*3.141592653589793*3100.*t)));
  w.append(x,.9f*x);
 }
}
p::Take take(double onset=.01,double sustain=.005,double tail=.00020,
             double harmonics=.2) {
 p::Take t;
 sound(t.onset,onset,harmonics);
 sound(t.sustain,sustain,harmonics);
 sound(t.tail,tail,harmonics);
 return t;
}
}
int main() {
 constexpr std::array<unsigned,10> ids{
  1101u,1102u,1103u,1104u,1105u,1106u,1107u,1109u,1110u,1111u};
 for(unsigned i=0;i<knobs.size();++i) {
  assert(knobs[i].vstId==ids[i]);
  assert(knobs[i].normalized>0. && knobs[i].normalized<=1.);
 }
 const auto original=take();
 const auto repeated=take();
 const auto restored=take();
 assert(evaluate(original,repeated,original,restored,
                 Type::Envelope).verdict==Verdict::Inconclusive);
 assert(evaluate(original,repeated,original,restored,
                 Type::Reverb).verdict==Verdict::Inconclusive);
 const auto decay=take(.005,.002,.00020);
 assert(evaluate(original,repeated,decay,restored,
                 Type::Envelope).verdict==Verdict::Proven);
 const auto release=take(.01,.005,.004);
 assert(evaluate(original,repeated,release,restored,
                 Type::Release).verdict==Verdict::Proven);
 const auto filter=take(.009,.004,.00020,0.);
 assert(evaluate(original,repeated,filter,restored,
                 Type::Filter).verdict==Verdict::Proven);
 const auto wet=take(.01,.005,.003);
 assert(evaluate(original,repeated,wet,restored,
                 Type::Reverb).verdict==Verdict::Proven);
 const auto chorus=take(.01,.005,.00020,2.);
 assert(evaluate(original,repeated,chorus,restored,
                 Type::Chorus).verdict==Verdict::Proven);
 const auto silent=take(0.,0.,0.);
 assert(evaluate(original,repeated,silent,restored,
                 Type::Envelope).verdict==Verdict::Inconclusive);
 const auto drift=take(.02,.018,.004);
 assert(evaluate(original,drift,decay,restored,
                 Type::Envelope).verdict==Verdict::UnstableReference);
 assert(evaluate(original,repeated,decay,drift,
                 Type::Envelope).verdict==Verdict::UnstableReference);
 assert(!significant(0.,.0000001,.00003,.19));
 assert(!significant(0.,0.,.00003,.19));
 assert(significant(.010,.004,.00003,.19));
 std::puts("P007AG_TEN_ORIGINAL_NRPN_AUDIO_MATRIX"
           "_ACOUSTIC_NEGATIVES_AND_POSITIVES_PASS");
}
