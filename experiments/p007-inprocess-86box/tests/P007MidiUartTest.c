#include "P007MidiUart.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
static void feed(const char* s) {
    while(*s)p007_midi_guest_tx_byte((uint8_t)*s++);
}
static void check_packet(uint8_t seq,uint8_t status,uint8_t a,uint8_t b) {
    uint8_t p[6];
    for(int i=0;i<6;++i)assert(p007_midi_uart_take_byte(&p[i])==1);
    assert(p[0]==0xA5 && p[1]==seq && p[2]==status &&
           p[3]==a && p[4]==b);
    assert(p[5]==(uint8_t)(0x5au^p[0]^p[1]^p[2]^p[3]^p[4]));
}
int main(void) {
    uint8_t byte=0;
    p007_midi_reset_quiescent();
    assert(!p007_midi_submit(0x90,60,100));
    assert(!p007_midi_uart_take_byte(&byte));
    p007_midi_uart_enable(1);
    assert(!p007_midi_guest_ready());
    assert(p007_midi_guest_sf2_status()==0);
    assert(!p007_midi_submit(0x90u,60u,101u));
    feed("P007-AWE-SF2-OK-X\n");
    assert(p007_midi_guest_sf2_status()==0);
    feed(P007_GUEST_SF2_OK_TOKEN);
    assert(p007_midi_guest_sf2_status()==1);
    assert(!p007_midi_guest_ready()); /* SF2 API alone is NOT WinMM READY */
    feed(P007_GUEST_SF2_FAIL_TOKEN);
    assert(p007_midi_guest_sf2_status()==-1);
    feed(P007_GUEST_SF2_OK_TOKEN);
    assert(p007_midi_guest_sf2_status()==1);
    feed("P007-AWE-WINMM-READY-X\n");
    assert(!p007_midi_guest_ready());
    feed(P007_GUEST_READY_TOKEN);
    assert(p007_midi_guest_ready());
    assert(p007_midi_submit(0x90u,60u,101u));
    assert(p007_midi_submit(0x80u,60u,0u));
    assert(p007_midi_uart_queued_bytes()==12u);
    check_packet(0,0x90u,60,101);
    check_packet(1,0x80u,60,0);
    assert(p007_midi_uart_queued_bytes()==0u);
    assert(!p007_midi_uart_take_byte(&byte));
    assert(!p007_midi_submit(0x70u,60u,127u));
    assert(!p007_midi_submit(0x90u,128u,100u));
    assert(!p007_midi_submit(0x90u,60u,128u));
    assert(!p007_midi_submit(0xf0u,60u,30u));
    /* Zero partial packets when ring full. */
    p007_midi_reset_quiescent();
    assert(p007_midi_guest_sf2_status()==0);
    p007_midi_uart_enable(1);
    assert(!p007_midi_submit(0x99u,50u,100u));
    feed(P007_GUEST_READY_TOKEN);
    int accepted=0;
    while(p007_midi_submit(0x99u,50u,100u))++accepted;
    assert(accepted==P007_MIDI_QUEUE_BYTES/P007_MIDI_WIRE_BYTES);
    assert(p007_midi_uart_queued_bytes()==(uint32_t)accepted*6u);
    assert(p007_midi_dropped_packets()==1u);
    for(int n=0;n<accepted;++n)check_packet((uint8_t)n,0x99u,50u,100u);
    assert(p007_midi_uart_queued_bytes()==0u);
    /* At the next guest UART clock, the pending byte must be available. */
    assert(p007_midi_submit(0xb0u,123u,0u));
    check_packet((uint8_t)accepted,0xb0u,123u,0u);
    feed(P007_GUEST_STOP_TOKEN);
    assert(!p007_midi_guest_ready());
    assert(!p007_midi_submit(0x90u,60u,80u));
    feed(P007_GUEST_READY_TOKEN);
    assert(p007_midi_guest_ready());
    p007_midi_uart_enable(0);
    assert(!p007_midi_submit(0x90u,60u,80u));
    assert(!p007_midi_uart_take_byte(&byte));
    puts("P007_INTERNAL_MIDI_TO_ORIGINAL_GUEST_UART_PACKET_AND_TIMING_GATE_PASS");
    puts("P007AU_ORIGINAL_CREATIVE_SF2_ACK_REJECT_RETRY_RESET_PASS");
    return 0;
}
