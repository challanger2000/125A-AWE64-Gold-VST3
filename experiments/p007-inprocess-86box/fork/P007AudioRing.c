/* P007: passive post-original-mixer audio tap, NOT a replacement EMU8000.
   Preserves the exact final 86Box float stream where sound_is_float != 0,
   or converts its original signed 16bit mixer output back to float.
   Single producer / single consumer fixed allocation. */
#include <stdint.h>
#include <stdatomic.h>
#include "P007AudioRing.h"
#ifdef _WIN32
# define P007_EXPORT __declspec(dllexport)
#else
# define P007_EXPORT __attribute__((visibility("default")))
#endif
_Static_assert((P007_AUDIO_FRAMES & (P007_AUDIO_FRAMES-1u)) == 0u,
               "Power-of-two capacity required");

static float s_data[P007_AUDIO_FRAMES * 2u];
static _Atomic uint64_t s_produced = 0u;
static _Atomic uint64_t s_consumed = 0u;
static _Atomic uint64_t s_dropped = 0u;
static _Atomic uint64_t s_trimmed = 0u;

static uint32_t reserve_frames(uint32_t count, uint64_t *start) {
    const uint64_t produced = atomic_load_explicit(&s_produced, memory_order_relaxed);
    const uint64_t consumed = atomic_load_explicit(&s_consumed, memory_order_acquire);
    const uint64_t occupancy = produced-consumed;
    const uint64_t free_count =
        occupancy < P007_AUDIO_FRAMES ? (P007_AUDIO_FRAMES-occupancy) : 0u;
    const uint32_t accepted = count < free_count ? count : (uint32_t)free_count;
    atomic_fetch_add_explicit(&s_dropped,(uint64_t)(count-accepted),
                              memory_order_relaxed);
    *start = produced;
    return accepted;
}

void p007_audio_capture_float(const float *input, uint32_t frames) {
    if(!input || !frames)return;
    uint64_t start;
    uint32_t count=reserve_frames(frames,&start);
    for(uint32_t i=0;i<count;++i) {
        const uint32_t dst=(uint32_t)((start+i)&(P007_AUDIO_FRAMES-1u))*2u;
        s_data[dst]=input[2u*i];
        s_data[dst+1u]=input[2u*i+1u];
    }
    atomic_store_explicit(&s_produced,start+count,memory_order_release);
}

void p007_audio_capture_pcm16(const int16_t *input, uint32_t frames) {
    if(!input || !frames)return;
    uint64_t start;
    uint32_t count=reserve_frames(frames,&start);
    for(uint32_t i=0;i<count;++i) {
        const uint32_t dst=(uint32_t)((start+i)&(P007_AUDIO_FRAMES-1u))*2u;
        s_data[dst]=(float)input[2u*i]*(1.0f/32768.0f);
        s_data[dst+1u]=(float)input[2u*i+1u]*(1.0f/32768.0f);
    }
    atomic_store_explicit(&s_produced,start+count,memory_order_release);
}

P007_EXPORT uint32_t p007_audio_pull(float *output, uint32_t frames) {
    if(!output || !frames)return 0u;
    const uint64_t consumed=atomic_load_explicit(&s_consumed,memory_order_relaxed);
    const uint64_t produced=atomic_load_explicit(&s_produced,memory_order_acquire);
    const uint64_t available=produced-consumed;
    const uint32_t count=(uint64_t)frames<available?frames:(uint32_t)available;
    for(uint32_t i=0;i<count;++i){
        const uint32_t src=(uint32_t)((consumed+i)&(P007_AUDIO_FRAMES-1u))*2u;
        output[2u*i]=s_data[src];
        output[2u*i+1u]=s_data[src+1u];
    }
    atomic_store_explicit(&s_consumed,consumed+count,memory_order_release);
    return count;
}

P007_EXPORT uint32_t p007_audio_available(void) {
    const uint64_t wr=atomic_load_explicit(&s_produced,memory_order_acquire);
    const uint64_t rd=atomic_load_explicit(&s_consumed,memory_order_acquire);
    return (uint32_t)(wr-rd);
}

P007_EXPORT uint32_t p007_audio_trim_backlog(uint32_t max_backlog,uint32_t retain_frames) {
    /* The VST3 process callback is the ONLY consumer. We must never
       overwrite producer-owned indices or mutate the original mixer state.
       The original 86Box producer publishes full frames with release;
       acquiring s_produced ensures all retained data is committed. */
    const uint64_t consumed=atomic_load_explicit(&s_consumed,memory_order_relaxed);
    const uint64_t produced=atomic_load_explicit(&s_produced,memory_order_acquire);
    const uint64_t pending=produced-consumed;
    if(pending<=(uint64_t)max_backlog || pending<=(uint64_t)retain_frames)
        return 0u;
    const uint64_t discarded=pending-(uint64_t)retain_frames;
    atomic_store_explicit(&s_consumed,consumed+discarded,memory_order_release);
    atomic_fetch_add_explicit(&s_trimmed,discarded,memory_order_relaxed);
    return (uint32_t)discarded;
}

P007_EXPORT uint64_t p007_audio_trimmed_frames(void) {
    return atomic_load_explicit(&s_trimmed,memory_order_relaxed);
}

P007_EXPORT uint64_t p007_audio_dropped_frames(void) {
    return atomic_load_explicit(&s_dropped,memory_order_relaxed);
}

P007_EXPORT void p007_audio_reset_quiescent(void) {
    /* NOT safe while producer or consumer executes. */
    atomic_store_explicit(&s_produced,0u,memory_order_relaxed);
    atomic_store_explicit(&s_consumed,0u,memory_order_relaxed);
    atomic_store_explicit(&s_dropped,0u,memory_order_relaxed);
    atomic_store_explicit(&s_trimmed,0u,memory_order_relaxed);
}
