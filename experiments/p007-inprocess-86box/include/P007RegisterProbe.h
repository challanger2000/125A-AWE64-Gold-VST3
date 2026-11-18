#ifndef P007_REGISTER_PROBE_H
#define P007_REGISTER_PROBE_H
/* P007AJ OWNER-PRIVATE TEST CORE ONLY.
 * Passive observation of original EMU8000 emu8k_trace_write/advance hooks.
 * Fixed-size 8192 events per authenticated note, no file I/O, no heap.
 * Does NOT modify EMU8000 CPU, chip registers, original driver, DSP or PCM.
 * Driver-side API called exclusively by QA host while guest CPU runs on
 * its existing emulator thread; all shared state is C11 atomic.
 * NEVER include this module in production Windows VST3 builds by default.
 */
#include <stdint.h>
#if defined(_WIN32)
# define P007AJ_EXPORT __declspec(dllexport)
#else
# define P007AJ_EXPORT __attribute__((visibility("default")))
#endif
enum { P007AJ_CAP=8192u, P007AJ_API_VERSION=1u };
typedef struct p007aj_register_event {
    uint64_t frame; /* original chip 44100Hz-relative since capture START */
    uint16_t port;  /* original emu8k_outw physical I/O port */
    uint16_t value; /* original Win95 driver-provided 16-bit register word */
    uint32_t serial; /* global chronological index from capture START */
} p007aj_register_event;
P007AJ_EXPORT unsigned p007aj_probe_version(void);
P007AJ_EXPORT int p007aj_probe_begin(unsigned tag);
P007AJ_EXPORT unsigned p007aj_probe_stop(void);
P007AJ_EXPORT unsigned p007aj_probe_count(void);
P007AJ_EXPORT unsigned p007aj_probe_dropped(void);
P007AJ_EXPORT unsigned p007aj_probe_copy(p007aj_register_event *dest,
                                         unsigned offset,unsigned capacity);
/* Only called from the already-compiled original emu8k trace hooks. */
void p007aj_probe_write(uint16_t port,uint16_t value);
void p007aj_probe_advance(int samples);
#endif
