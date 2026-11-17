/* Compile into the pinned ORIGINAL 86Box-AWE32 source tree as one additional
   C unit. The original snd_emu8k.c, Creative guest and x86 CPU are unchanged.
   These probes must NEVER call into the emulation before core init. */
#include <stdint.h>
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <86box/86box.h>
#include <86box/machine.h>
#include <86box/video.h>
/* Configuration is owned by the original 86Box source. */
extern int gfxcard[];
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

/* P007-B PRIVATE VM PROFILE INITIALIZATION.
   Call once, strictly on the future emulator thread. This only configures
   original 86Box paths and internal device settings via pc_init().
   It does NOT boot Win95, start guest CPU or invoke any UI.
   All paths must be supplied by the in-plugin bundle runtime (not CWD).
*/
static int s_profile_initialized;
static int checked_path(const char *src,char *dst,size_t cap) {
    if(!src || !*src) return 0;
    const size_t len=strlen(src);
    if(len>=cap || len<3u || (src[1]!=':' &&
       !(src[0]=='\\' && src[1]=='\\'))) return 0;
    memcpy(dst,src,len+1u);
    return 1;
}
P007_EXPORT int p007_core_init_profile(
        const char *vm_path,const char *rom_path,
        const char *config_basename,const char *global_config) {
    if(s_profile_initialized) return -2;
    char vm[768]={0},rom[768]={0},global[768]={0},asset[768]={0},cfg[128]={0};
    if(!checked_path(vm_path,vm,sizeof(vm)) ||
       !checked_path(rom_path,rom,sizeof(rom)) ||
       !checked_path(global_config,global,sizeof(global)) ||
       !config_basename || !*config_basename ||
       strlen(config_basename)>=sizeof(cfg) ||
       strchr(config_basename,'/') ||
       strchr(config_basename,'\\') ||
       strchr(config_basename,':')) return -1;
    strcpy(cfg,config_basename);
    /* ROM and assets are both private children of the installed VST3's
       Contents/Resources/PrivateVM. Never resolve guest assets against
       Studio Pro's process working directory or 86Box.exe location. */
    const char *sep=strrchr(rom,'\\');
    if (!sep) sep=strrchr(rom,'/');
    if (!sep) return -1;
    const size_t prefix=(size_t)(sep-rom)+1u;
    if(prefix+sizeof("assets")>sizeof(asset)) return -1;
    memcpy(asset,rom,prefix);
    memcpy(asset+prefix,"assets",sizeof("assets"));
    char program[]="P007_Embedded_Guest";
    char vmpath[]="--vmpath",rompath[]="--rompath",assetopt[]="--assetpath";
    char config[]="--config",globalopt[]="--global";
    char *argv[]={
      program,vmpath,vm,rompath,rom,assetopt,asset,config,cfg,globalopt,global
    };
    const int result=pc_init(11,argv);
    if(result==1) s_profile_initialized=1;
    return result;
}

/* Original 86Box's guest launch sequence, invoked only on the private
   emulator thread. Neither original chip DSP nor x86 timing is replaced. */
static int s_guest_stage;
P007_EXPORT int p007_core_guest_stage(void) { return s_guest_stage; }
P007_EXPORT int p007_core_guest_boot(void) {
    if (!s_profile_initialized) return -1;
    if (s_guest_stage) return -2;
    /* No SDL video/window/audio driver is started in the host DAW. */
    if (SDL_Init(0) != 0) return -3;
    if (!pc_init_roms()) return -4;
    /* 86Box pc_init_modules() can call exit(-1) for an unavailable
       primary machine or video card. Never kill the DAW host. */
    if (machine < 0 || !machine_available(machine)) return -5;
    if (gfxcard[0] < 0 || !video_card_available(gfxcard[0])) return -6;
    if (!pc_init_modules()) return -7;
    pc_reset_hard_init();
    s_guest_stage = 1;
    return 1;
}
P007_EXPORT int p007_core_guest_step(void) {
    if (s_guest_stage != 1) return 0;
    pc_run();
    return 1;
}
P007_EXPORT void p007_core_guest_close(void) {
    if (s_guest_stage != 1) return;
    pc_close(NULL);
    s_guest_stage = 2;
    SDL_Quit();
}
