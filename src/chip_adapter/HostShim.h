#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Call on the same non-audio thread as emu8k_init. No process-thread I/O. */
bool awe64_host_set_rom_path(const char *path);
#ifdef __cplusplus
}
#endif
