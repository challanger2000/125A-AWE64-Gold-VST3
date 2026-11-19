// P007BA: deterministic guard regression. No original Windows 95/PCM claims.
// Test the exact C-compatible original 16550 receive-side occupancy predicate
// and the actual MIDI bank/program parameter-dedup helper used by the VST3.
#include "../include/P007SerialRxGuard.h"
#include "../vst3/P007MidiParamDedup.hpp"
#include "../vst3/P007BankProgramPacer.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <vector>

static void check(bool v,const char* msg) {
    if(!v){std::fprintf(stderr,"P007BA_FAIL %s\n",msg);std::exit(1);}
}
int main(){
    check(p007_serial_rx_has_room(1,0,0,0)==1,"empty RX fifo");
    check(p007_serial_rx_has_room(1,15,0,0)==1,"15-byte fifo has one byte room");
    check(p007_serial_rx_has_room(1,16,0,0)==0,"full 16-byte RX must BLOCK");
    check(p007_serial_rx_has_room(1,17,0,0)==0,"overfull RX must BLOCK");
    check(p007_serial_rx_has_room(1,3,0,1)==0,"serial RSR staging must BLOCK");
    check(p007_serial_rx_has_room(0,0,1,0)==0,"non-FIFO RDR unread must BLOCK");
    check(p007_serial_rx_has_room(0,0,0,0)==1,"non-FIFO receiver clear");

    // Show the failure that the old push-only tunnel couldn't prevent:
    // >16 bytes arrive while Win95 is not polling and are lost.
    std::vector<std::uint8_t> wire;
    for(unsigned i=0;i<128u;++i) {
        const std::uint8_t seq=static_cast<std::uint8_t>(i);
        const std::uint8_t pitch=static_cast<std::uint8_t>(i&127u);
        const std::uint8_t status=0x90u,velocity=88u;
        wire.insert(wire.end(),{0xa5u,seq,status,pitch,velocity,
            static_cast<std::uint8_t>(0x5au^0xa5u^seq^status^pitch^velocity)});
    }
    std::size_t unguardedDrop=0;
    std::deque<std::uint8_t> originalRx;
    for(const auto byte:wire){
        if(originalRx.size()==16u)++unguardedDrop;
        else originalRx.push_back(byte);
    }
    check(unguardedDrop>0u,"negative control did NOT reproduce overflow");

    // Original RX port drains intermittently under Win95 scheduling.
    // Flow control must NOT dequeue upstream wire bytes when 16550
    // receives are full: no dropped bytes and no reordering of 128
    // authentic 6-byte MIDI tunnel packets in this model.
    std::deque<std::uint8_t> rx;
    std::vector<std::uint8_t> consumed;
    std::size_t next=0,maxFill=0;
    for(unsigned tick=0;tick<30000u && consumed.size()<wire.size();++tick){
        if((tick%47u)<5u && !rx.empty()){
            consumed.push_back(rx.front());
            rx.pop_front();
        }
        if(next<wire.size() &&
           p007_serial_rx_has_room(1,static_cast<unsigned>(rx.size()),0,0)){
            rx.push_back(wire[next++]);
        }
        if(rx.size()>maxFill)maxFill=rx.size();
        check(rx.size()<=16u,"backpressure allowed real 16550 overflow");
    }
    check(next==wire.size()&&consumed==wire,
          "wire bytes lost/reordered under Win95 stalled polling");
    check(maxFill==16u,"negative guard test never exercised full 16550");

    // Duplicate host automation at every callback must not restart
    // the 20 ms Creative CC0->CC32->Program->settle state machine.
    std::atomic<std::uint8_t> bank{1u},program{7u};
    p007::BankProgramPacer pacer;
    unsigned packets=0u;
    for(unsigned block=0;block<500u;++block){
        const bool force=p007::midi_param::updateIfChanged(bank,1u)
                  ||p007::midi_param::updateIfChanged(program,7u);
        check(!force,"identical bank/program automation caused resend");
        pacer.request(bank.load(),program.load(),true,force);
        if(block%21u==0u)
            (void)pacer.tick([&](std::uint8_t,std::uint8_t,std::uint8_t)->int{
                ++packets;return 1;
            });
    }
    check(pacer.committed()&&packets==3u,
          "identical automation left original Creative bank permanently uncommitted");
    check(p007::midi_param::updateIfChanged(bank,0u),"real bank edit lost");
    check(!p007::midi_param::updateIfChanged(bank,0u),"duplicate bank not deduplicated");
    pacer.request(bank.load(),program.load(),true,true);
    check(!pacer.committed()&&pacer.stage()==0u,"real bank edit did not restart");
    std::vector<unsigned> cc;
    for(unsigned n=0;n<4u;++n)
       (void)pacer.tick([&](std::uint8_t status,std::uint8_t a,std::uint8_t b)->int{
           cc.push_back((unsigned(status)<<16u)|(unsigned(a)<<8u)|b);return 1;
       });
    check(pacer.committed()&&cc.size()==3u,"ROM bank change not committed");
    check(cc[0]==0xb00000u && cc[1]==0xb02000u && cc[2]==0xc00700u,
          "original ROM Bank0 CC0/CC32/Program mapping changed");
    std::puts("P007BA_ORIGINAL_16550_FIFO_BACKPRESSURE_128_PACKET_LOSSLESS_UNIT_PASS");
    std::puts("P007BA_DUPLICATE_HOST_BANK_PROGRAM_AUTOMATION_NO_STARVATION_UNIT_PASS");
    std::puts("P007BA_SYNTHETIC_NEGATIVE_CONTROL_NO_REAL_WIN95_PCM_CLAIM");
    return 0;
}
