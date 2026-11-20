/* P007BE: in-guest SoundFont transfer with the ORIGINAL 86Box CPU and
 * Win95 guest. Dedicated 0xeb00 I/O window (NOT sound synthesis).
 * 0 in8 status: pending=0xa7, other 0/2/3/4
 * 2..5 in8 exact 32-bit file length
 * 6 out8: 1=begin, 2=guest successful, 3=guest failure
 * 8 in32: next 4 bytes (last word zero-padded).
 *
 * All calls, including publication, are on the single emulator worker.
 * The VST3 audio thread never touches the transfer payload or this API.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <86box/io.h>
#include "P007LiveSf2.h"
#include "P007MidiUart.h"
#ifdef _WIN32
#define P007_EXPORT __declspec(dllexport)
#else
#define P007_EXPORT __attribute__((visibility("default")))
#endif
static unsigned char *s_bytes;
static uint32_t s_len,s_position,s_capacity;
static _Atomic int s_state; /* x86 worker writes; owner QA observes */
static int s_attached; /* worker-thread only */
static _Atomic uint32_t s_published,s_completed,s_failed;
static uint8_t p007_live_in8(uint16_t port,void *unused){
    (void)unused;
    switch((unsigned)(port-P007_LIVE_SF2_BASE)) {
    case 0: return s_state==1?P007_LIVE_SF2_READY_MAGIC:(uint8_t)s_state;
    case 2:case 3:case 4:case 5:
        return (uint8_t)(s_len>>(8u*(port-P007_LIVE_SF2_BASE-2u)));
    default: return 0xffu;
    }
}
static uint32_t p007_live_in32(uint16_t port,void *unused){
    (void)unused;
    if(port!=P007_LIVE_SF2_BASE+8u || s_state!=2)return 0xffffffffu;
    uint32_t value=0u;
    for(unsigned i=0;i<4u && s_position<s_len;++i)
        value|=((uint32_t)s_bytes[s_position++])<<(8u*i);
    return value;
}
static void p007_live_out8(uint16_t port,uint8_t cmd,void *unused){
    (void)unused;
    if(port!=P007_LIVE_SF2_BASE+6u)return;
    if(cmd==1u && s_state==1){s_position=0u;s_state=2;}
    else if(cmd==2u && s_state==2 && s_position==s_len) {
        s_state=3;
        atomic_fetch_add_explicit(&s_completed,1u,memory_order_release);
    }
    else if(cmd==3u && s_state==2) {
        s_state=4;
        atomic_fetch_add_explicit(&s_failed,1u,memory_order_release);
    }
}
P007_EXPORT int p007_live_soundfont_publish(const void *data,uint32_t size){
    if(!s_attached || !data || size<96u ||
       size>P007_LIVE_SF2_MAX_BYTES)return -1;
    if(s_state==1 || s_state==2)return 0;
    const uint8_t *p=(const uint8_t*)data;
    const uint32_t riff=(uint32_t)p[4]|((uint32_t)p[5]<<8u)|
                        ((uint32_t)p[6]<<16u)|((uint32_t)p[7]<<24u);
    if(memcmp(p,"RIFF",4)!=0 || memcmp(p+8,"sfbk",4)!=0 ||
       riff!=size-8u)return -2;
    if(s_capacity<size) {
       unsigned char *next=(unsigned char*)realloc(s_bytes,size);
       if(!next)return -3;
       s_bytes=next;s_capacity=size;
    }
    memcpy(s_bytes,data,size);
    s_len=size;s_position=0u;
    p007_midi_guest_reset_sf2_ack();
    atomic_fetch_add_explicit(&s_published,1u,memory_order_release);
    s_state=1;
    return 1;
}
P007_EXPORT int p007_live_soundfont_state(void){return s_state;}
P007_EXPORT uint32_t p007_live_soundfont_publish_count(void){
    return atomic_load_explicit(&s_published,memory_order_acquire);
}
P007_EXPORT uint32_t p007_live_soundfont_complete_count(void){
    return atomic_load_explicit(&s_completed,memory_order_acquire);
}
P007_EXPORT uint32_t p007_live_soundfont_failure_count(void){
    return atomic_load_explicit(&s_failed,memory_order_acquire);
}
void p007_live_soundfont_attach(void){
    if(s_attached)return;
    s_len=s_position=0u;s_state=0;
    atomic_store_explicit(&s_published,0u,memory_order_release);
    atomic_store_explicit(&s_completed,0u,memory_order_release);
    atomic_store_explicit(&s_failed,0u,memory_order_release);
    io_sethandler(P007_LIVE_SF2_BASE,16u,
                  p007_live_in8,0,p007_live_in32,p007_live_out8,0,0,0);
    s_attached=1;
}
void p007_live_soundfont_detach(void){
    if(!s_attached)return;
    io_removehandler(P007_LIVE_SF2_BASE,16u,
                     p007_live_in8,0,p007_live_in32,p007_live_out8,0,0,0);
    s_attached=0;s_len=s_position=0u;s_state=0;
}
