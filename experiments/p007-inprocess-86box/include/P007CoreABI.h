#pragma once
/* Stable read-only introspection ABI for an authentic 86Box-AWE32 DLL.
   This spike tests whether the complete original C emulator can be linked
   and dynamically loaded into a Windows-x64 host process.
   NO guest init, boot or audio is claimed by these probes. */
#include <stdint.h>
#define P007_ABI_VERSION 0x00070001u
#ifdef __cplusplus
extern "C" {
#endif
uint32_t p007_core_abi_version(void);
uintptr_t p007_core_symbol_pc_init(void);
uintptr_t p007_core_symbol_pc_run(void);
uintptr_t p007_core_symbol_pc_reset(void);
#ifdef __cplusplus
}
#endif
