#pragma once
// P007AG — ORIGINAL Win95 Creative/EMU8000 PCM measurement matrix.
//
// Unlike synthetic-core tests, this classifier is not proof that a CC
// arrived at the private Creative driver: actual PCM must be captured.
// A single General MIDI patch may not expose every filter envelope or
// effect-generator dependency. Separate PASS / INCONCLUSIVE / UNSTABLE
// so we never certify a non-audible setting or falsely call it broken.
#include "P007OriginalControlDifferential.hpp"
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace p007::ten_matrix {
namespace p=p007::original_control_proof;
enum class Type : std::uint8_t { Envelope, Release, Filter, Reverb, Chorus };
enum class Verdict : std::uint8_t { Proven, Inconclusive, UnstableReference };
struct Knob {
 const char* title;
 std::uint32_t vstId;
 double normalized;
 Type type;
};
// Order intentionally mirrors P165: Amp 1/2/3, Filter Env 4/5/6/7,
// resonance 9, Reverb 10, Chorus 11; exclude proven AMP Attack 0 & cutoff 8.
inline constexpr std::array<Knob,10> knobs{{
 {"AMP_DECAY",1101u,.85,Type::Envelope},
 {"AMP_SUSTAIN",1102u,.95,Type::Envelope},
 {"AMP_RELEASE",1103u,1.,Type::Release},
 {"FILTER_ENV_ATTACK",1104u,.90,Type::Filter},
 {"FILTER_ENV_DECAY",1105u,.90,Type::Filter},
 {"FILTER_ENV_SUSTAIN",1106u,.95,Type::Filter},
 {"FILTER_ENV_RELEASE",1107u,.90,Type::Filter},
 {"FILTER_RESONANCE",1109u,1.,Type::Filter},
 {"REVERB_SEND",1110u,1.,Type::Reverb},
 {"CHORUS_SEND",1111u,1.,Type::Chorus}
}};
struct Evidence {
 bool stableBefore=false,stableAfter=false,hasB=false;
 bool onsetChanged=false,sustainChanged=false,tailChanged=false;
 double bOnset=0.,aOnset=0.,restoredOnset=0.;
 double bRoughness=0.,aRoughness=0.;
 double bTail=0.,aTail=0.;
 Verdict verdict=Verdict::UnstableReference;
};
inline bool significant(double baseline,double changed,double floor,
                        double ratio) noexcept {
 if(!std::isfinite(baseline)||!std::isfinite(changed))return false;
 if(std::max(std::abs(baseline),std::abs(changed))<floor)return false;
 return p::relativeChange(baseline,changed)>ratio;
}
inline Evidence evaluate(const p::Take& original,
                         const p::Take& before,
                         const p::Take& modified,
                         const p::Take& restored,
                         Type kind) noexcept {
 Evidence e{};
 e.stableBefore=p::stable(original,before);
 e.stableAfter=p::stable(before,restored);
 e.hasB=p::nonSilent(modified);
 e.aOnset=before.onset.rms();
 e.bOnset=modified.onset.rms();
 e.restoredOnset=restored.onset.rms();
 e.aRoughness=before.onset.roughness();
 e.bRoughness=modified.onset.roughness();
 e.aTail=before.tail.rms();e.bTail=modified.tail.rms();
 e.onsetChanged=significant(e.aOnset,e.bOnset,.00003,.19)||
                significant(e.aRoughness,e.bRoughness,.02,.16);
 e.sustainChanged=significant(before.sustain.rms(),
                             modified.sustain.rms(),.00003,.22)||
                significant(before.sustain.roughness(),
                             modified.sustain.roughness(),.02,.18);
 e.tailChanged=significant(e.aTail,e.bTail,.000015,.34);
 if(!e.stableBefore||!e.stableAfter) {
     e.verdict=Verdict::UnstableReference;
     return e;
 }
 if(!e.hasB) {
     // Could be a valid "silent" modified envelope, or a dropped MIDI
     // NoteOn: without independent positive-CC control, never certify.
     e.verdict=Verdict::Inconclusive;
     return e;
 }
 bool changed=false;
 switch(kind){
   case Type::Envelope: changed=e.onsetChanged||e.sustainChanged;break;
   case Type::Release: changed=e.sustainChanged||e.tailChanged;break;
   case Type::Filter: changed=e.onsetChanged||e.sustainChanged;break;
   case Type::Reverb: changed=e.tailChanged;break;
   case Type::Chorus: changed=e.sustainChanged||e.onsetChanged;break;
 }
 e.verdict=changed?Verdict::Proven:Verdict::Inconclusive;
 return e;
}
inline const char* verdictName(Verdict v) noexcept {
 switch(v) {
  case Verdict::Proven: return "MEASURABLE";
  case Verdict::Inconclusive: return "INCONCLUSIVE_WITH_THIS_ROM_PATCH";
  case Verdict::UnstableReference: return "A_B_A_REFERENCE_UNSTABLE";
 }
 return "UNKNOWN";
}
} // namespace p007::ten_matrix
