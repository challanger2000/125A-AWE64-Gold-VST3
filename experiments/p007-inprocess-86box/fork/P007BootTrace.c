#include <stdatomic.h>
#include <stdint.h>
#include "P007BootTrace.h"
#ifdef _WIN32
# define P007_EXPORT __declspec(dllexport)
#else
# define P007_EXPORT __attribute__((visibility("default")))
#endif
static _Atomic uint64_t read_calls,read_sectors,write_calls,write_sectors;
static _Atomic uint32_t read_lba,write_lba,read_id,write_id;
void p007_trace_hdd_read(uint8_t id,uint32_t lba,uint32_t count) {
    atomic_store_explicit(&read_lba,lba,memory_order_relaxed);
    atomic_store_explicit(&read_id,id,memory_order_relaxed);
    atomic_fetch_add_explicit(&read_sectors,count,memory_order_relaxed);
    atomic_fetch_add_explicit(&read_calls,1u,memory_order_relaxed);
}
void p007_trace_hdd_write(uint8_t id,uint32_t lba,uint32_t count) {
    atomic_store_explicit(&write_lba,lba,memory_order_relaxed);
    atomic_store_explicit(&write_id,id,memory_order_relaxed);
    atomic_fetch_add_explicit(&write_sectors,count,memory_order_relaxed);
    atomic_fetch_add_explicit(&write_calls,1u,memory_order_relaxed);
}
P007_EXPORT int p007_trace_get(P007BootTrace* out) {
    if(!out)return 0;
    out->read_calls=atomic_load_explicit(&read_calls,memory_order_relaxed);
    out->read_requested_sectors=atomic_load_explicit(&read_sectors,memory_order_relaxed);
    out->write_calls=atomic_load_explicit(&write_calls,memory_order_relaxed);
    out->write_requested_sectors=atomic_load_explicit(&write_sectors,memory_order_relaxed);
    out->last_read_lba=atomic_load_explicit(&read_lba,memory_order_relaxed);
    out->last_write_lba=atomic_load_explicit(&write_lba,memory_order_relaxed);
    out->last_read_disk=atomic_load_explicit(&read_id,memory_order_relaxed);
    out->last_write_disk=atomic_load_explicit(&write_id,memory_order_relaxed);
    return 1;
}
