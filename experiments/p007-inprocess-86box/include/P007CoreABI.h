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
/* 1: initialized in-process; -1: invalid paths; -2: already initialized;
   0: original 86Box pc_init rejected profile. No Win95 boot implied. */
int p007_core_init_profile(const char *vm_path,const char *rom_path,
                          const char *config_basename,const char *global_config);
/* Guest lifecycle is owned by the single emulator thread, never the VST process callback. */
int p007_core_guest_boot(void);  /* 1=hardware initialized; negative=not ready/failure */
int p007_core_guest_step(void);  /* 1=original CPU slice; 0=not booted */
void p007_core_guest_close(void);
int p007_core_guest_stage(void); /* 0=not started, 1=running, 2=closed */
unsigned p007_core_guest_cold_boot_count(void); /* passive x86 hard reset count */
/* Original keyboard/BIOS on guest worker only.
   Returns 1=F1 sent at exact observed BIOS prompt, 2=Enter to dismiss
   observed post-Win95 network modal (bounded); 0=none, -1=not booted. */
int p007_core_guest_boot_assist(uint32_t guest_milliseconds);
#ifdef __cplusplus
}
#endif
