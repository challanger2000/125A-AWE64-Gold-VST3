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
        std::puts("P007Y_ORIGINAL_CREATIVE_PCM_BEFORE_AFTER_NOTE_PROOF_RULES_PASS "
                  "cases=8 synthetic-constant-audio-rejected=YES");
        return 0;
    }catch(const std::exception& e) {
        std::fprintf(stderr,"P007Y_ORIGINAL_CREATIVE_PCM_PROOF_RULES_FAIL %s\n",
                     e.what());
        return 1;
    }
}
