/* P007 internal UART wire. Matches original P006 Win95 COM2 relay's
   0xA5, seq, MIDI status, data1, data2, XOR CRC wire contract.
   Uses original 86Box UART receive_timer for actual 115200-8N1 timing.
   No call to Windows OS serial, no named pipe, no allocation. */
#include <stdint.h>
#include <stdatomic.h>
#include "P007MidiUart.h"
#ifdef _WIN32
# define P007_EXPORT __declspec(dllexport)
#else
# define P007_EXPORT __attribute__((visibility("default")))
#endif
_Static_assert((P007_MIDI_QUEUE_BYTES&(P007_MIDI_QUEUE_BYTES-1u))==0u,
               "MIDI FIFO must be power-of-two");
static uint8_t s_data[P007_MIDI_QUEUE_BYTES];
static _Atomic uint64_t s_written=0u;
static _Atomic uint64_t s_read=0u;
static _Atomic uint64_t s_dropped=0u;
static _Atomic int s_active=0;
static uint8_t s_sequence=0u; /* MIDI producer thread owns it */

P007_EXPORT void p007_midi_uart_enable(int active){
    atomic_store_explicit(&s_active,!!active,memory_order_release);
}
P007_EXPORT int p007_midi_uart_active(void) {
    return atomic_load_explicit(&s_active,memory_order_acquire);
}
P007_EXPORT int p007_midi_submit(uint8_t status,uint8_t data1,uint8_t data2) {
    if(status<0x80u||status>=0xf0u||data1>127u||data2>127u) return 0;
    if(!p007_midi_uart_active())return 0;
    const uint64_t wr=atomic_load_explicit(&s_written,memory_order_relaxed);
    const uint64_t rd=atomic_load_explicit(&s_read,memory_order_acquire);
    if(wr-rd>P007_MIDI_QUEUE_BYTES-P007_MIDI_WIRE_BYTES) {
        atomic_fetch_add_explicit(&s_dropped,1u,memory_order_relaxed);
        return 0;
    }
    const uint8_t seq=s_sequence++;
    const uint8_t packet[P007_MIDI_WIRE_BYTES] = {
        P007_MIDI_PACKET_SYNC,seq,status,data1,data2,
        (uint8_t)(0x5au^P007_MIDI_PACKET_SYNC^seq^status^data1^data2)
    };
    for(uint32_t i=0u;i<P007_MIDI_WIRE_BYTES;++i)
        s_data[(uint32_t)(wr+i)&(P007_MIDI_QUEUE_BYTES-1u)]=packet[i];
    atomic_store_explicit(&s_written,wr+P007_MIDI_WIRE_BYTES,
                          memory_order_release);
    return 1;
}

P007_EXPORT int p007_midi_uart_take_byte(uint8_t *out){
    if(!out || !p007_midi_uart_active())return 0;
    const uint64_t rd=atomic_load_explicit(&s_read,memory_order_relaxed);
    const uint64_t wr=atomic_load_explicit(&s_written,memory_order_acquire);
    if(rd==wr)return 0;
    *out=s_data[(uint32_t)rd&(P007_MIDI_QUEUE_BYTES-1u)];
    atomic_store_explicit(&s_read,rd+1u,memory_order_release);
    return 1;
}
P007_EXPORT uint32_t p007_midi_uart_queued_bytes(void) {
    uint64_t wr=atomic_load_explicit(&s_written,memory_order_acquire);
    uint64_t rd=atomic_load_explicit(&s_read,memory_order_acquire);
    return (uint32_t)(wr-rd);
}
P007_EXPORT uint64_t p007_midi_dropped_packets(void) {
    return atomic_load_explicit(&s_dropped,memory_order_relaxed);
}
P007_EXPORT void p007_midi_reset_quiescent(void) {
    atomic_store_explicit(&s_active,0,memory_order_relaxed);
    atomic_store_explicit(&s_written,0u,memory_order_relaxed);
    atomic_store_explicit(&s_read,0u,memory_order_relaxed);
    atomic_store_explicit(&s_dropped,0u,memory_order_relaxed);
    s_sequence=0;
}
