// PUBLIC, DETERMINISTIC ONLY. Tests the real P007AQ VST3 audio-thread
// UART queue; synthetic data must never be described as Creative audio.
#include "../vst3/P007MidiDelivery.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
using p007::MidiDelivery;
static void check(bool ok,const char* why){
    if(!ok){std::fprintf(stderr,"P007AQ_FAIL %s\n",why);std::exit(1);}
}
int main(){
    MidiDelivery fifo;
    std::vector<MidiDelivery::Message> accepted{};
    bool rejectFirstNoteOff=true;
    const auto flaky=[&](unsigned char s,unsigned char a,unsigned char b){
        if(s==0x80u && rejectFirstNoteOff){
            rejectFirstNoteOff=false;return 0;
        }
        accepted.push_back({s,a,b});
        return 1;
    };
    check(fifo.submit(0x90u,60u,100u,flaky),"NoteOn submit");
    check(accepted.size()==1u,"first NoteOn not sent");
    check(fifo.submit(0x80u,60u,0u,flaky),"rejected NoteOff not retained");
    check(fifo.pending()==1u,"NoteOff not queued");
    check(fifo.submit(0x90u,62u,100u,flaky),"successor note not queued");
    check(accepted.size()==1u,"later NoteOn overtook missing NoteOff");
    check(fifo.drain(flaky)==2u,"original NoteOff retry not drained");
    check(accepted.size()==3u&&
          accepted[1].status==0x80u&&accepted[1].data1==60u&&
          accepted[2].status==0x90u&&accepted[2].data1==62u,
          "MIDI delivery order changed");
    check(!fifo.pending()&&!fifo.panicPending(),"queue not empty");
    // Robust overflow policy: stale NoteOns must not be transmitted after
    // a missing NoteOff was stranded. Do NOT leave the sustain pedal latched.
    // Escalate to ordered CC64=0, CC120=0, CC123=0 on ALL 16 channels.
    fifo.reset();
    const auto blocked=[](auto,auto,auto){return 0;};
    for(unsigned i=0u;i<MidiDelivery::capacity;++i)
        check(fifo.submit(0x90u,static_cast<unsigned char>(i&127u),72u,blocked),
              "bounded queue failed before exact capacity");
    check(!fifo.submit(0x90u,1u,72u,blocked),"overflow accepted");
    check(fifo.pending()==0u&&fifo.panicPending(),"panic not requested at overflow");
    check(!fifo.submit(0x90u,66u,90u,flaky),"NoteOn accepted during panic");
    auto failedCC=true;
    unsigned deliveredPanic=0u;
    std::array<unsigned,16> covered{};
    const auto panic=[&](unsigned char status,unsigned char ctl,unsigned char value){
        if(failedCC){failedCC=false;return 0;}
        check((status&0xf0u)==0xb0u&&value==0u,
              "panic used nonstandard MIDI CC packet");
        const unsigned channel=status&15u;
        check(channel==deliveredPanic/3u,
              "panic changed channel ordering");
        const unsigned expected=deliveredPanic%3u==0u?64u:
                               (deliveredPanic%3u==1u?120u:123u);
        check(ctl==expected,
              "panic did not release sustain before all sounds/notes off");
        ++deliveredPanic;
        ++covered[channel];
        return 1;
    };
    check(fifo.drain(panic)==0u&&fifo.panicPending(),
          "rejected panic packet lost");
    // Every packet gets the chance to fail independently; it must stay
    // pending, never skip to the next channel or CC when rejected.
    for(unsigned n=0;n<48u;++n) {
        check(fifo.drain(panic)==1u,
              "not every channel got CC64, CC120 and CC123");
        if(n<47u)
            check(fifo.panicPending(),
                  "panic ended before all 16 channels were silenced");
    }
    check(!fifo.panicPending(),"panic never completed");
    check(deliveredPanic==48u,"panic emitted wrong CC count");
    for(unsigned done:covered)check(done==3u,"MIDI channel missed a safety CC");
    check(fifo.submit(0x90u,64u,90u,flaky),"healthy UART not restored");
    fifo.reset();
    check(!fifo.pending()&&!fifo.panicPending(),"reset failed");
    std::puts("P007AT_BOUNDED_ORDERED_UART_RETRY_AND_SUSTAIN_SAFE_OVERFLOW_PANIC_PASS");
    std::puts("P007AQ_SYNTHETIC_UNIT_TEST_NO_WIN95_ORIGINAL_AUDIO_CLAIM");
}
