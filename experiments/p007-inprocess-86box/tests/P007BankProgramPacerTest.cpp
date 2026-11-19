#include "P007BankProgramPacer.hpp"
#include "P007CreativeNrpnPacer.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
// Do NOT let MSVC /DNDEBUG remove assertions or their MIDI tick calls.
// These are executable adversarial checks in Release CI, not debug hints.
#undef assert
#define assert(expr) do { if(!(expr)) { \
    std::fprintf(stderr,"P007AZ_FAIL %s:%d: %s\\n",__FILE__,__LINE__,#expr); \
    return 1; \
} } while(false)

using p007::BankProgramPacer;
struct Msg{std::uint8_t status,d1,d2;};
int main(){
    BankProgramPacer p;
    std::vector<Msg> sent;
    auto submit=[&](std::uint8_t st,std::uint8_t a,std::uint8_t b)->int{
        sent.push_back({st,a,b});return 1;
    };
    p.request(1u,7u,true);
    assert(!p.committed() && p.stage()==0u);
    assert(p.tick(submit));
    assert(p.stage()==1u && !p.committed());
    assert(sent.size()==1u && sent[0].status==0xb0u &&
           sent[0].d1==0u && sent[0].d2==1u);
    p.request(1u,7u,true); // stable parameter callback may NOT reset pacing
    assert(p.stage()==1u);
    assert(p.tick(submit));
    assert(sent.size()==2u && sent[1].status==0xb0u &&
           sent[1].d1==32u && sent[1].d2==0u);
    assert(p.tick(submit));
    assert(sent.size()==3u && sent[2].status==0xc0u &&
           sent[2].d1==7u && sent[2].d2==0u);
    assert(!p.committed()); // mandatory separate settling interval
    assert(!p.tick(submit) && p.committed());
    assert(sent.size()==3u); // NO new packet in settle stage
    assert(!p.tick(submit));
    // Actual selection after host bank0 automation, still original ROM.
    sent.clear();
    p.request(0u,7u,true);
    for(int i=0;i<4;++i)(void)p.tick(submit);
    assert(p.committed() && sent.size()==3u &&
           sent[0].d2==0u && sent[1].d2==0u && sent[2].d1==7u);
    // New SF2 not acknowledged must never masquerade as user bank1.
    sent.clear();
    p.request(1u,1u,false);
    for(int i=0;i<4;++i)(void)p.tick(submit);
    assert(p.committed() && sent.size()==3u &&
           sent[0].d2==0u && sent[1].d2==1u);
    // Failure must retry EXACT packet, not consider it committed.
    p.reset();sent.clear();
    p.request(1u,7u,true);
    assert(!p.tick([&](auto,auto,auto){return 0;}));
    assert(p.stage()==0u && sent.empty() && !p.committed());
    assert(p.tick(submit) && p.stage()==1u);
    // Host program change while selection is in-flight must restart at CC0.
    p.request(1u,8u,true);
    assert(p.stage()==0u && !p.committed());
    sent.clear();
    for(int i=0;i<4;++i)(void)p.tick(submit);
    assert(p.committed() && sent.size()==3u && sent[2].d1==8u);
    // Real Creative ROM standard kit on MIDI channel 10.
    sent.clear();p.request(126u,0u,false);
    for(int i=0;i<4;++i)(void)p.tick(submit);
    assert(p.committed() && sent.size()==3u &&
           sent[0].status==0xb9u && sent[0].d2==0u &&
           sent[2].status==0xc9u && sent[2].d1==0u);
    // Original Creative USER bank1 selected on channel10 for true SF2 kits.
    sent.clear();p.request(127u,5u,true);
    for(int i=0;i<4;++i)(void)p.tick(submit);
    assert(p.committed() && sent.size()==3u &&
           sent[0].status==0xb9u && sent[0].d2==1u &&
           sent[1].status==0xb9u && sent[1].d2==0u &&
           sent[2].status==0xc9u && sent[2].d1==5u);
    // Force resend after guest restart or project recall.
    p.request(1u,8u,true,true);
    assert(p.stage()==0u && !p.committed());
    // Real-time <=20ms gating even with tiny 64-frame ASIO blocks.
    p007::creative_nrpn::FramePace gap;
    gap.configure(44100.);
    assert(gap.due(64u));
    unsigned blocks=0;
    while(!gap.due(64u)) {
        ++blocks;assert(blocks<100u);
    }
    assert(blocks>=13u);
    std::puts("P007AZ_CREATIVE_BANK_PROGRAM_FIFO_16B_PACING_AND_NOTES_GATE_UNIT_PASS"
              " commands=3 max_wire_bytes_per_tick=6 settling_gap=1");
}
