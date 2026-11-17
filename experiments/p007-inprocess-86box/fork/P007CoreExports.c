/* Compile into the pinned ORIGINAL 86Box-AWE32 source tree as one additional
   C unit. The original snd_emu8k.c, Creative guest and x86 CPU are unchanged.
   These probes must NEVER call into the emulation before core init. */
#include <stdint.h>
#include <86box/86box.h>
#include "P007CoreABI.h"
#ifdef _WIN32
#define P007_EXPORT __declspec(dllexport)
#else
#define P007_EXPORT __attribute__((visibility("default")))
#endif
P007_EXPORT uint32_t p007_core_abi_version(void) {
    return P007_ABI_VERSION;
}
P007_EXPORT uintptr_t p007_core_symbol_pc_init(void) {
    return (uintptr_t)(void*)&pc_init;
}
P007_EXPORT uintptr_t p007_core_symbol_pc_run(void) {
    return (uintptr_t)(void*)&pc_run;
}
P007_EXPORT uintptr_t p007_core_symbol_pc_reset(void) {
    return (uintptr_t)(void*)&pc_reset_hard_init;
}
