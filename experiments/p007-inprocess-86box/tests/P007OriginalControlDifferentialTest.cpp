#include "P007OriginalControlDifferential.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace p007::original_control_proof;
namespace {
void fill(Window& window,double fundamental,double extra=0.,
          double amplitude=.012) {
 for(unsigned i=0;i<48000u;++i) {
  const double t=double(i)/44100.;
  const float l=static_cast<float>(amplitude*
      (std::sin(2.*3.141592653589793*fundamental*t)+
       extra*std::sin(2.*3.141592653589793*3500.*t)));
  window.append(l,l*.9f);
 }
}
Take normal() {
 Take t;
 fill(t.onset,440.,.2);
 fill(t.sustain,440.,.2);
 fill(t.tail,440.,0.,.00020);
 return t;
}
}
int main() {
 const auto a=normal(),a2=normal(),a3=normal();
 assert(nonSilent(a)&&stable(a,a2));
 assert(!cutoffEffect(a,a,a3));
 assert(!attackEffect(a,a,a3));
 assert(!sendTailEffect(a,a,a3));
 assert(!chorusEffect(a,a,a3));
 auto unstable=a;
 unstable.onset=Window{};
 fill(unstable.onset,440.,.2,.001);
 assert(!stable(a,unstable));
 auto filtered=a;
 filtered.onset=Window{};
 fill(filtered.onset,440.,0.,.005);
 assert(cutoffEffect(a,filtered,a3));
 auto attacked=a;
 attacked.onset=Window{};
 fill(attacked.onset,440.,.2,.001);
 assert(attackEffect(a,attacked,a3));
 auto wet=a;
 wet.tail=Window{};
 fill(wet.tail,440.,.2,.003);
 assert(sendTailEffect(a,wet,a3));
 auto chorus=a;
 chorus.sustain=Window{};
 fill(chorus.sustain,440.,2.);
 assert(chorusEffect(a,chorus,a3));
 Take silence{};
 assert(!nonSilent(silence));
 assert(!stable(silence,silence));
 assert(!cutoffEffect(silence,filtered,a3));
 assert(!attackEffect(silence,attacked,a3));
 std::puts("P007AC_ORIGINAL_WIN95_PCM_ABA_NEGATIVE_AND_POSITIVE_ORACLE_PASS cases=14");
 return 0;
}
