#pragma once
/* Research-only passive counters at ORIGINAL 86Box HDD image I/O entrypoints.
   No disk content, guest memory or proprietary information is exposed.
   Counters are bounded thread-safe atomics and safe to read after each
   original CPU slice. Counts represent requested, not committed sectors. */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct P007BootTrace {
    uint64_t read_calls,read_requested_sectors;
    uint64_t write_calls,write_requested_sectors;
    uint32_t last_read_lba,last_write_lba;
    uint32_t last_read_disk,last_write_disk;
} P007BootTrace;
void p007_trace_hdd_read(uint8_t id,uint32_t lba,uint32_t sectors);
void p007_trace_hdd_write(uint8_t id,uint32_t lba,uint32_t sectors);
int p007_trace_get(P007BootTrace* out);
#ifdef __cplusplus
}
#endif
