#pragma once
// P007AK: independent, test-only spectral A/B/A measurement of REAL PCM.
// No changes to the original 86Box EMU8000 synthesis or Creative driver.
// Deliberately fixed-size, allocation-free, streaming filterbank; evaluated
// after the original VST3 host has produced the audio.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace p007::original_spectral_proof {

inline constexpr unsigned binFrames = 11025u; // 250 ms at native 44.1 kHz
inline constexpr unsigned noteBins = 13u;    // 0 ... 3.25 seconds
inline constexpr unsigned tailBins = 11u;    // 0 ... 2.75 seconds after NoteOff
inline constexpr unsigned bins = noteBins + tailBins;
inline constexpr unsigned bands = 7u;
// Six low-pass crossovers yield seven broad spectral bands. Unlike 1-2 FFT
// peak frequencies, broad bands remain valid for chords, pads and evolving timbres.
inline constexpr std::array<double,6> cutoffs{{150.,350.,750.,1500.,3000.,6000.}};
inline constexpr double pi = 3.1415926535897932384626433832795;

struct Capture {
  double state[2][6]{};
  double bandPower[bins][bands]{};
  double totalPower[bins]{};
  std::uint32_t count[bins]{};
  unsigned noteFrames=0u,releaseFrames=0u;
  void append(float left,float right,bool isRelease) noexcept {
    const unsigned frame=isRelease?releaseFrames++:noteFrames++;
    const unsigned bucket=(isRelease?noteBins:0u)+frame/binFrames;
    if(bucket>=bins || !std::isfinite(left) || !std::isfinite(right)) return;
    const double sample[2]={left,right};
    double energy=0.;
    for(unsigned c=0;c<2;++c) {
      const double x=sample[c];
      double last=0.;
      for(unsigned b=0;b<6;++b) {
        const double alpha=1.-std::exp(-2.*pi*cutoffs[b]/44100.);
        state[c][b]+=alpha*(x-state[c][b]);
        const double band=state[c][b]-last;
        bandPower[bucket][b]+=band*band;
        last=state[c][b];
      }
      const double high=x-last;
      bandPower[bucket][6]+=high*high;
      energy+=x*x;
    }
    totalPower[bucket]+=energy;
    ++count[bucket];
  }
};

enum class Target : std::uint8_t { Attack,Decay,Sustain,Release };
struct Evidence {
  bool proven=false;
  bool referenceUnstable=false;
  unsigned comparable=0,restored=0,consecutive=0;
  double strongestDb=0.,maxReferenceDriftDb=0.;
  unsigned strongestBand=0,strongestBin=0;
};
inline bool audible(const Capture& c,unsigned i) noexcept {
  return c.count[i]>2000u &&
         c.totalPower[i]/(2.*double(c.count[i]))>1.e-10;
}
inline double shapeDb(const Capture& c,unsigned i,unsigned band) noexcept {
  // Relative spectral shape rejects simple volume/fader drift.
  return 10.*std::log10((c.bandPower[i][band]+1.e-20)/
                        (c.totalPower[i]+1.e-20));
}
inline Evidence evaluate(const Capture& a,const Capture& b,
                         const Capture& back,Target target) noexcept {
  Evidence e{};
  unsigned first=0u,last=noteBins;
  switch(target) {
    case Target::Attack: first=0u;last=6u;break; // first 1.5 s, not 0.5-2.25 s
    case Target::Decay: first=1u;last=11u;break;
    case Target::Sustain: first=6u;last=noteBins;break;
    case Target::Release: first=noteBins;last=bins;break;
  }
  // Require the B spectral difference to repeat in one broad band in at
  // least two successive 250 ms windows, with the same sign, and to exceed
  // BOTH an absolute 4 dB threshold and three times the measured A/A drift.
  // A single noisy/transient window is explicitly NOT enough.
  for(unsigned band=0;band<bands;++band) {
    int previousSign=0;
    unsigned previousIndex=bins;
    for(unsigned i=first;i<last;++i) {
      if(!audible(a,i)||!audible(b,i)||!audible(back,i)) {
        previousSign=0;continue;
      }
      ++e.comparable;
      const double aDb=shapeDb(a,i,band);
      const double bDb=shapeDb(b,i,band);
      const double rDb=shapeDb(back,i,band);
      const double referenceDrift=std::abs(aDb-rDb);
      e.maxReferenceDriftDb=std::max(e.maxReferenceDriftDb,referenceDrift);
      if(referenceDrift>3.) {
        e.referenceUnstable=true;
        previousSign=0;
        continue;
      }
      ++e.restored;
      const double difference=bDb-(aDb+rDb)*.5;
      const int sign=(difference>0.)?1:-1;
      const double effect=std::abs(difference);
      if(effect>std::max(4.,3.*referenceDrift+1.)) {
        if(effect>e.strongestDb) {
          e.strongestDb=effect;e.strongestBand=band;e.strongestBin=i;
        }
        if(previousSign==sign && previousIndex+1u==i) {
          ++e.consecutive;
          e.proven=true;
        }
        previousSign=sign;previousIndex=i;
      } else previousSign=0;
    }
  }
  // Coverage and reference return are mandatory. One valid frequency and
  // just two consecutive windows is proof of a *measurable* spectral effect,
  // not necessarily a perceptually obvious difference.
  e.proven=e.proven && e.restored>=2u && !e.referenceUnstable;
  return e;
}
inline const char* verdict(const Evidence& e) noexcept {
  if(e.referenceUnstable)return "UNSTABLE_REFERENCE";
  return e.proven?"MEASURABLE_SPECTRAL_CHANGE":"INCONCLUSIVE";
}
} // namespace p007::original_spectral_proof
