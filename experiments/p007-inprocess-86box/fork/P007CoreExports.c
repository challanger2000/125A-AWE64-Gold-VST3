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
#include <86box/keyboard.h>
#include <86box/mem.h>
#include "P007BootTrace.h"
#include "P007MidiUart.h"
#include "P007LiveSf2.h"
/* Configuration is owned by the original 86Box source. */
extern int gfxcard[];
/* 86Box original pc_close sets this exit latch. It must be reset before
   starting another original guest in the SAME loaded VST3 module. */
extern volatile int is_quit;
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
    /* Windows production keeps its original absolute DOS/UNC path gate.
       Temporary Linux QA accepts POSIX absolute paths so the PRIVATE
       Win95 guest can be tested offline without uploading it to GitHub. */
#ifdef _WIN32
    const int absolute=(src[1]==':' ||
        (src[0]=='\\' && src[1]=='\\'));
#else
    const int absolute=(src[0]=='/');
#endif
    if(len>=cap || len<3u || !absolute) return 0;
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
    /* A DAW can reuse the same DLL after closing an internal guest.
       Original pc_close sets is_quit; a new pc_init must clear this. */
    is_quit=0;
    /* Match the ORIGINAL SDL frontend order: SDL_Init(0) precedes pc_init.
       86Box platform setup can touch SDL state while parsing the profile. */
    if (SDL_Init(0) != 0) return -3;
    const int result=pc_init(11,argv);
    if(result==1) s_profile_initialized=1;
    else SDL_Quit();
    return result;
}

/* Original 86Box's guest launch sequence, invoked only on the private
   emulator thread. Neither original chip DSP nor x86 timing is replaced.
   The ORIGINAL VGA/video blit thread expects its buffer-in-use event to be
   acknowledged on every blit. With no frontend window and a NULL blit
   callback that event is never signaled, and guest VGA processing can hang.
   Explicit headless video drain supplies the ORIGINAL completion event
   without creating a DAW window or modifying emulated video register state. */
static void p007_headless_blit(int x,int y,int w,int h,int monitor_index) {
    (void)x;(void)y;(void)w;(void)h;
    video_blit_complete_monitor(monitor_index);
}
static int s_guest_stage;
static unsigned s_real_guest_cold_boots;
P007_EXPORT int p007_core_guest_stage(void) { return s_guest_stage; }
P007_EXPORT unsigned p007_core_guest_cold_boot_count(void) {
    return s_real_guest_cold_boots;
}

/* Private Win95 cold-boot aids, exclusively on the emulator worker thread.
   All keystrokes use ORIGINAL 86Box keyboard hardware inside the guest;
   NEVER post host OS/DAW keyboard events or run guest CPU on audio thread.
   The first boot of this particular Win95 image exposes two known prompts:
   - Award BIOS "Press F1 to continue" on fresh CMOS NVR;
   - Win95 NE2000 (disabled/no-network) warning requiring Enter to dismiss.
   These aids are bounded, evidence-gated, and stop at Creative READY. */
static uint32_t s_f1_release_at, s_enter_release_at;
static uint32_t s_win95_start_at, s_last_enter_at;
static unsigned s_f1_sent, s_win95_seen, s_enter_count;
static int p007_bios_text_contains(const char *needle) {
    const size_t len=strlen(needle);
    if(!len || len>80u) return 0;
    for(uint32_t row=0;row<25u;++row) {
        for(uint32_t col=0;col+len<=80u;++col) {
            size_t n=0u;
            while(n<len && mem_readb_phys(0xb8000u+
                ((row*80u+col+(uint32_t)n)*2u))==(uint8_t)needle[n]) ++n;
            if(n==len) return 1;
        }
    }
    return 0;
}
P007_EXPORT int p007_core_guest_boot_assist(uint32_t tick) {
    if(s_guest_stage!=1) return -1;
    if(s_f1_release_at && tick>=s_f1_release_at) {
        keyboard_input(0,0x3bu);
        s_f1_release_at=0u;
    }
    if(s_enter_release_at && tick>=s_enter_release_at) {
        keyboard_input(0,0x1cu);
        s_enter_release_at=0u;
    }
    if(tick%500u != 0u) return 0;
    if(!s_f1_sent && p007_bios_text_contains("Press F1 to continue")) {
        keyboard_input(1,0x3bu);
        s_f1_release_at=tick+100u;
        s_f1_sent=1u;
        return 1;
    }
    if(!s_win95_seen && p007_bios_text_contains("Starting Windows 95")) {
        s_win95_seen=1u;
        s_win95_start_at=tick;
    }
    if(!s_win95_seen || p007_midi_guest_ready() ||
       s_enter_count>=3u || tick < s_win95_start_at+29000u ||
       (s_last_enter_at && tick<s_last_enter_at+12000u)) return 0;
    P007BootTrace hdd={0};
    if(!p007_trace_get(&hdd) || hdd.read_calls<600u) return 0;
    keyboard_input(1,0x1cu);
    s_enter_release_at=tick+100u;
    s_last_enter_at=tick;
    ++s_enter_count;
    return 2;
}

P007_EXPORT int p007_core_guest_boot(void) {
    if (!s_profile_initialized) return -1;
    if (s_guest_stage) return -2;
    /* SDL base is initialized in original order in init_profile().
       No SDL video/window/audio driver is ever started in the host DAW. */
    if (!pc_init_roms()) return -4;
    /* 86Box pc_init_modules() can call exit(-1) for an unavailable
       primary machine or video card. Never kill the DAW host. */
    if (machine < 0 || !machine_available(machine)) return -5;
    if (gfxcard[0] < 0 || !video_card_available(gfxcard[0])) return -6;
    if (!pc_init_modules()) return -7;
    /* Must register before the original BIOS/VGA first blit occurs. */
    video_setblit(p007_headless_blit);
    pc_reset_hard_init();
    /* Private file-transfer hardware; no original CPU/EMU8000 changes. */
    p007_live_soundfont_attach();
    ++s_real_guest_cold_boots; /* actual original x86 cold reset count */
    s_f1_release_at=s_enter_release_at=0u;
    s_win95_start_at=s_last_enter_at=0u;
    s_f1_sent=s_win95_seen=s_enter_count=0u;
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
    p007_live_soundfont_detach();
    pc_close(NULL);
    video_setblit(NULL);
    /* The guest has been torn down; clear all Creative WinMM ACK/serial
       parser and queued MIDI bytes before a fresh Win95 boot. Otherwise
       stale READY+SF2-OK from the old guest wrongly marks the new guest
       ready before the Creative driver even starts. */
    p007_midi_guest_retire();
    SDL_Quit();
    /* Original 86Box was written for one process launch. The adapter must
       re-arm its lifecycle latches after a full device teardown; otherwise
       the next SF2 selection or DAW activation fails init with -2. */
    s_profile_initialized=0;
    s_guest_stage=0;
    is_quit=0;
}
