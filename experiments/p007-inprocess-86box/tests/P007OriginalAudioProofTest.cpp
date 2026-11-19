// P007Y: numerical proof rules for owner-only ORIGINAL Creative audio.
// The synthetic CI tone must NOT pass as original NoteOn->PCM.
#include "P007OriginalAudioProof.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>
using p007::original_audio_proof::Window;
using p007::original_audio_proof::accept;
void require(bool yes,const char* text) {
    if(!yes)throw std::runtime_error(text);
}
void fill(Window& w,float amplitude,unsigned count) {
    for(unsigned i=0;i<count;++i)
        w.append(i%2?amplitude:-amplitude, i%2?-amplitude:amplitude);
}
int main() {
    try {
        Window quiet, note, constantBefore, constantAfter, noiseBefore, noiseAfter;
        fill(quiet,0.f,66150);
        fill(note,.024f,88200);
        require(accept(quiet,note),"genuine silent-to-audible onset rejected");
        require(!accept(quiet,quiet),"pure silence falsely passed");
        fill(constantBefore,.25f,66150);
        fill(constantAfter,.25f,88200);
        require(!accept(constantBefore,constantAfter),
                "persistent pre-note unrelated audio falsely passed");
        fill(noiseBefore,.002f,66150);
        fill(noiseAfter,.003f,88200);
        require(!accept(noiseBefore,noiseAfter),
                "small background change falsely passed");
        Window louder;
        fill(louder,.024f,88200);
        require(accept(noiseBefore,louder),
                "clear original MIDI onset over background rejected");
        Window tooShort;
        fill(tooShort,.024f,9000);
        require(!accept(quiet,tooShort),"brief click falsely passed");
        Window missingBefore;
        require(!accept(missingBefore,note),"missing baseline falsely passed");
        require(!accept(quiet,note,0.),"invalid rate accepted");
        // P007AW: identical normalized audio and small gain drift are NOT
        // bank changes. Same-amplitude but different spectral contents can
        // establish distinction even when RMS and peak coincide.
        Window tone440,tone440Copy,tone440SlightGain,tone1760;
        for(unsigned i=0u;i<88200u;++i) {
            const double time=double(i)/44100.;
            const float a=float(.02*std::sin(2.*3.141592653589793*440.*time));
            const float b=float(.02*std::sin(2.*3.141592653589793*1760.*time));
            tone440.append(a,a);
            tone440Copy.append(a,a);
            tone440SlightGain.append(a*1.02f,a*1.02f);
            tone1760.append(b,b);
        }
        require(!p007::original_audio_proof::distinguishBanks(tone440,tone440Copy),
                "identical SF2/ROM audio produced false bank-change PASS");
        require(!p007::original_audio_proof::distinguishBanks(tone440,tone440SlightGain),
                "2 percent level drift produced false bank-change PASS");
        require(p007::original_audio_proof::distinguishBanks(tone440,tone1760),
                "equal RMS and peak but different timbre wrongly rejected");
        require(!p007::original_audio_proof::distinguishBanks(quiet,note),
                "non-silent source cannot be omitted for SF2/ROM comparison");
        std::puts("P007AW_SPECTRAL_BANK_DIFFERENCE_NEGATIVE_CONTROLS_PASS"
                  " identical_rejected=1 gain_drift_rejected=1"
                  " equal_rms_different_frequency_accepted=1");
        std::puts("P007Y_ORIGINAL_CREATIVE_PCM_BEFORE_AFTER_NOTE_PROOF_RULES_PASS "
                  " cases=8 synthetic-constant-audio-rejected=YES");
        return 0;
    }catch(const std::exception& e) {
        std::fprintf(stderr,"P007Y_ORIGINAL_CREATIVE_PCM_PROOF_RULES_FAIL %s\n",
                     e.what());
        return 1;
    }
}
