#include "P007CreativeNrpnPacer.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <vector>
using namespace p007::creative_nrpn;
int main(){
 std::atomic<std::uint32_t> pending{0u};
 std::array<std::atomic<std::uint16_t>,count> values{};
 Pacer pacer;
 std::vector<Control> sent;
 values[8].store(6000u); // 60% filter cutoff -> raw 76
 pending.store(1u<<8);
 auto send=[&](std::uint8_t number,std::uint8_t amount){
   sent.push_back({number,amount});return true;
 };
 for(unsigned i=0;i<4u;++i) {
   const auto before=sent.size();
   assert(pacer.tick(pending,values,send));
   assert(sent.size()==before+1u); // NOT 24 bytes / single audio block
 }
 assert(pacer.stage()==0u);
 assert(sent.size()==4u&&sent[0].number==99u&&sent[0].value==127u);
 assert(sent[1].number==98u&&sent[1].value==21u);
 assert(sent[2].number==6u&&sent[2].value==64u);
 assert(sent[3].number==38u&&sent[3].value==76u);
 assert(!pacer.tick(pending,values,send));
 // Reject once at CC98: exactly the same packet must be retried later.
 sent.clear();pacer.reset();pending.store(1u<<8);
 unsigned rejects=0;
 assert(pacer.tick(pending,values,send));
 assert(pacer.stage()==2u);
 const auto stall=[&](std::uint8_t,std::uint8_t){++rejects;return false;};
 assert(pacer.tick(pending,values,stall));
 assert(pacer.stage()==2u&&rejects==1u);
 assert(pacer.tick(pending,values,send));
 assert(pacer.stage()==3u);
 // User changes knob DURING the 4-part NRPN: no edit may be dropped.
 values[8].store(1000u);
 assert(pacer.tick(pending,values,send));
 assert(pacer.tick(pending,values,send));
 assert(pacer.stage()==0u);
 assert((pending.load()&(1u<<8))!=0u);
 for(unsigned i=0;i<4u;++i)assert(pacer.tick(pending,values,send));
 assert(pacer.stage()==0u && pending.load()==0u);
 assert(sent[sent.size()-1u].number==38u);
 assert(sent[sent.size()-1u].value==13u); // 10% of 127
 // CC121/guest reboot must abandon partial transaction & replay whole.
 pacer.reset();sent.clear();pending.store(1u<<8);
 assert(pacer.tick(pending,values,send));
 assert(pacer.stage()==2u);
 pacer.abort(pending);
 assert(pacer.stage()==0u&&(pending.load()&(1u<<8))!=0u);
 sent.clear();
 for(unsigned i=0;i<4u;++i)assert(pacer.tick(pending,values,send));
 assert(sent.size()==4u && sent[0].number==99u);
 // 0% tone is NEUTRAL (no destructive NRPN) but 0% FX SEND is dry.
 pacer.reset();sent.clear();
 values[8].store(0u);values[10].store(0u);values[11].store(0u);
 pending.store((1u<<8)|(1u<<10)|(1u<<11));
 unsigned callbacks=0u;
 for(;callbacks<20u;++callbacks){
   const auto before=sent.size();
   (void)pacer.tick(pending,values,send);
   assert(sent.size()-before<=1u);
   if(pending.load()==0u && pacer.stage()==0u)break;
 }
 assert(callbacks<20u);
 assert(sent.size()==8u); // 4 reverb CC + 4 chorus CC, no filter NRPN
 assert(sent[3].number==38u&&sent[3].value==0u);
 assert(sent[7].number==38u&&sent[7].value==0u);
 std::puts("P007AE_REAL_16550_FIFO_ONE_MIDI_PACKET_PER_AUDIO_CALLBACK_PASS"
           " rejected_message_retry=YES mid_edit_requeued=YES"
           " CC121_abort_requeues=YES dry_zero_send=YES");
 return 0;
}
