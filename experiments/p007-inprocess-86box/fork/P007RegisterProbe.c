#include <P007RegisterProbe.h>
#include <stdatomic.h>
#include <stdint.h>
/* Fixed capacity, dormant by default; only QA toggles. */
typedef struct {
    _Atomic uint64_t frame;
    _Atomic uint32_t portvalue;
} Slot;
static Slot s_events[P007AJ_CAP];
static _Atomic unsigned s_active=0u,s_writers=0u;
static _Atomic unsigned s_count=0u,s_dropped=0u;
static _Atomic uint64_t s_frame=0u;
P007AJ_EXPORT unsigned p007aj_probe_version(void){return P007AJ_API_VERSION;}
P007AJ_EXPORT int p007aj_probe_begin(unsigned tag) {
    (void)tag; /* tag is logged by the independent QA executable */
    if(atomic_load_explicit(&s_active,memory_order_acquire))return 0;
    if(atomic_load_explicit(&s_writers,memory_order_acquire))return 0;
    atomic_store_explicit(&s_count,0u,memory_order_relaxed);
    atomic_store_explicit(&s_dropped,0u,memory_order_relaxed);
    atomic_store_explicit(&s_frame,0u,memory_order_relaxed);
    atomic_store_explicit(&s_active,1u,memory_order_release);
    return 1;
}
P007AJ_EXPORT unsigned p007aj_probe_stop(void) {
    atomic_store_explicit(&s_active,0u,memory_order_release);
    /* Do NOT block the DAW audio callback; this function is called from
       private QA main thread only. Bounded wait only for in-flight writer;
       if still racing, exported event slots remain independently atomic. */
    for(unsigned retry=0u;retry<200000u;++retry){
        if(!atomic_load_explicit(&s_writers,memory_order_acquire))break;
    }
    return atomic_load_explicit(&s_count,memory_order_acquire);
}
P007AJ_EXPORT unsigned p007aj_probe_count(void) {
    const unsigned n=atomic_load_explicit(&s_count,memory_order_acquire);
    return n>P007AJ_CAP?P007AJ_CAP:n;
}
P007AJ_EXPORT unsigned p007aj_probe_dropped(void){
    return atomic_load_explicit(&s_dropped,memory_order_acquire);
}
P007AJ_EXPORT unsigned p007aj_probe_copy(p007aj_register_event* dest,
                                               unsigned offset,
                                               unsigned capacity) {
    if(!dest||atomic_load_explicit(&s_active,memory_order_acquire))return 0u;
    const unsigned n=p007aj_probe_count();
    if(offset>=n)return 0u;
    unsigned result=0u;
    while(result<capacity && offset+result<n) {
        const unsigned idx=offset+result;
        const uint32_t pair=atomic_load_explicit(
                          &s_events[idx].portvalue,memory_order_acquire);
        dest[result].frame=atomic_load_explicit(
                          &s_events[idx].frame,memory_order_acquire);
        dest[result].port=(uint16_t)(pair>>16);
        dest[result].value=(uint16_t)(pair&65535u);
        dest[result].serial=idx;
        ++result;
    }
    return result;
}
void p007aj_probe_advance(int samples) {
    if(samples>0 && atomic_load_explicit(&s_active,memory_order_relaxed))
        atomic_fetch_add_explicit(&s_frame,(uint64_t)samples,
                                  memory_order_relaxed);
}
void p007aj_probe_write(uint16_t port,uint16_t value) {
    if(!atomic_load_explicit(&s_active,memory_order_relaxed))return;
    atomic_fetch_add_explicit(&s_writers,1u,memory_order_acq_rel);
    if(!atomic_load_explicit(&s_active,memory_order_acquire)){
        atomic_fetch_sub_explicit(&s_writers,1u,memory_order_release);
        return;
    }
    const unsigned idx=atomic_fetch_add_explicit(&s_count,1u,
                                                 memory_order_acq_rel);
    if(idx<P007AJ_CAP) {
        /* Fully atomic slot stores; no data race even if stop/copy overlaps
           a guest-thread write, and no blocking or allocation in chip code. */
        atomic_store_explicit(&s_events[idx].frame,
               atomic_load_explicit(&s_frame,memory_order_relaxed),
               memory_order_release);
        atomic_store_explicit(&s_events[idx].portvalue,
               ((uint32_t)port<<16u)|value,memory_order_release);
    }else {
        atomic_fetch_add_explicit(&s_dropped,1u,memory_order_relaxed);
    }
    atomic_fetch_sub_explicit(&s_writers,1u,memory_order_release);
}
