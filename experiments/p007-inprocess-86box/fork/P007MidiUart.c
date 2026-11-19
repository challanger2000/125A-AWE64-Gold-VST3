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
static _Atomic int s_guest_ready=0;
static _Atomic int s_guest_sf2_status=0; /* 0=none, 1=Creative API loaded, -1=rejected */
static uint32_t s_ready_progress=0u, s_stop_progress=0u;
static uint32_t s_sf2_ok_progress=0u,s_sf2_fail_progress=0u; /* emulation thread only */
static uint32_t advance_token(uint32_t state, uint8_t val, const char *token) {
    if (val == (uint8_t)token[state]) return state+1u;
    return val == (uint8_t)token[0] ? 1u : 0u;
}
P007_EXPORT int p007_midi_guest_ready(void) {
    return atomic_load_explicit(&s_guest_ready,memory_order_acquire);
}
P007_EXPORT int p007_midi_guest_sf2_status(void) {
    return atomic_load_explicit(&s_guest_sf2_status,memory_order_acquire);
}
P007_EXPORT void p007_midi_guest_reset_sf2_ack(void) {
    atomic_store_explicit(&s_guest_sf2_status,0,memory_order_release);
    s_sf2_ok_progress=s_sf2_fail_progress=0u;
}
P007_EXPORT void p007_midi_guest_tx_byte(uint8_t byte) {
    const char ready[]=P007_GUEST_READY_TOKEN;
    const char stop[]=P007_GUEST_STOP_TOKEN;
    const char sf2ok[]=P007_GUEST_SF2_OK_TOKEN;
    const char sf2fail[]=P007_GUEST_SF2_FAIL_TOKEN;
    s_sf2_ok_progress=advance_token(s_sf2_ok_progress,byte,sf2ok);
    s_sf2_fail_progress=advance_token(s_sf2_fail_progress,byte,sf2fail);
    if(s_sf2_ok_progress==sizeof(sf2ok)-1u){
        s_sf2_ok_progress=0;
        atomic_store_explicit(&s_guest_sf2_status,1,memory_order_release);
    }
    if(s_sf2_fail_progress==sizeof(sf2fail)-1u){
        s_sf2_fail_progress=0;
        atomic_store_explicit(&s_guest_sf2_status,-1,memory_order_release);
    }
    s_ready_progress=advance_token(s_ready_progress,byte,ready);
    s_stop_progress=advance_token(s_stop_progress,byte,stop);
    if(s_stop_progress==sizeof(stop)-1u) {
        s_stop_progress=0;
        atomic_store_explicit(&s_active,0,memory_order_release);
        atomic_store_explicit(&s_guest_ready,0,memory_order_release);
    }
    if(s_ready_progress==sizeof(ready)-1u) {
        s_ready_progress=0;
        atomic_store_explicit(&s_guest_ready,1,memory_order_release);
        atomic_store_explicit(&s_active,1,memory_order_release);
    }
}
static uint8_t s_sequence=0u; /* MIDI producer thread owns it */

P007_EXPORT void p007_midi_uart_enable(int active){
    atomic_store_explicit(&s_active,!!active,memory_order_release);
}
P007_EXPORT int p007_midi_uart_active(void) {
    return atomic_load_explicit(&s_active,memory_order_acquire);
}
P007_EXPORT int p007_midi_submit(uint8_t status,uint8_t data1,uint8_t data2) {
    if(status<0x80u||status>=0xf0u||data1>127u||data2>127u) return 0;
    if(!p007_midi_uart_active() || !p007_midi_guest_ready())return 0;
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
    s_ready_progress=s_stop_progress=0;
    s_sf2_ok_progress=s_sf2_fail_progress=0u;
    atomic_store_explicit(&s_guest_sf2_status,0,memory_order_release);
    atomic_store_explicit(&s_guest_ready,0,memory_order_release);
}

/* P007BB: safe during a live host's internal Win95/SF2 guest swap.
 * Unlike p007_midi_reset_quiescent(), this does NOT touch s_sequence or
 * reset the concurrently published producer write counter. It ensures
 * the old WinMM READY and original Creative SF2 ACK CANNOT leak across
 * guest sessions; old already-queued MIDI is abandoned before new READY. */
P007_EXPORT void p007_midi_guest_retire(void) {
    atomic_store_explicit(&s_active,0,memory_order_release);
    atomic_store_explicit(&s_guest_ready,0,memory_order_release);
    atomic_store_explicit(&s_guest_sf2_status,0,memory_order_release);
    s_ready_progress=s_stop_progress=0;
    s_sf2_ok_progress=s_sf2_fail_progress=0u;
    const uint64_t wr=atomic_load_explicit(&s_written,memory_order_acquire);
    atomic_store_explicit(&s_read,wr,memory_order_release);
}
