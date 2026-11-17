// A real native Windows loader test; no Win95 guest, no 86Box process.
// Signals only in-process DLL link/loader readiness (P007-A).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <bit>
#include <cstdio>
#include <cstring>
#include "P007CoreABI.h"
using VersionFn = std::uint32_t (*)();
using SymbolFn = std::uintptr_t (*)();
using PullFn = std::uint32_t (*)(float*,std::uint32_t);
using AvailableFn = std::uint32_t (*)();
using ProfileFn = int (*)(const char*,const char*,const char*,const char*);
using MidiEnableFn = void (*)(int);
using MidiSubmitFn = int (*)(std::uint8_t,std::uint8_t,std::uint8_t);
using MidiTakeFn = int (*)(std::uint8_t*);
int main(int argc,char** argv) {
    if(argc!=2||!argv[1]||!argv[1][0])return 2;
    // LoadLibrary: same-process DLL mapping, NOT CreateProcess.
    HMODULE module=LoadLibraryA(argv[1]);
    if(!module){
        std::printf("P007_DLL_LOAD_FAILED winerror=%lu\n",GetLastError());
        return 3;
    }
    auto version=std::bit_cast<VersionFn>(
        GetProcAddress(module,"p007_core_abi_version"));
    auto init=std::bit_cast<SymbolFn>(
        GetProcAddress(module,"p007_core_symbol_pc_init"));
    auto run=std::bit_cast<SymbolFn>(
        GetProcAddress(module,"p007_core_symbol_pc_run"));
    auto reset=std::bit_cast<SymbolFn>(
        GetProcAddress(module,"p007_core_symbol_pc_reset"));
    auto audioPull=std::bit_cast<PullFn>(
        GetProcAddress(module,"p007_audio_pull"));
    auto audioAvailable=std::bit_cast<AvailableFn>(
        GetProcAddress(module,"p007_audio_available"));
    float empty[2]{};
    auto profile=std::bit_cast<ProfileFn>(
        GetProcAddress(module,"p007_core_init_profile"));

    auto midiEnable=std::bit_cast<MidiEnableFn>(
        GetProcAddress(module,"p007_midi_uart_enable"));
    auto midiSubmit=std::bit_cast<MidiSubmitFn>(
        GetProcAddress(module,"p007_midi_submit"));
    auto midiTake=std::bit_cast<MidiTakeFn>(
        GetProcAddress(module,"p007_midi_uart_take_byte"));
    if(!midiEnable||!midiSubmit||!midiTake){
        std::puts("P007_INTERNAL_UART_DLL_EXPORTS_MISSING");
        FreeLibrary(module);return 13;
    }
    midiEnable(1);
    if(midiSubmit(0x90u,60u,100u)!=1){
        std::puts("P007_DLL_MIDI_SUBMIT_FAILED");
        FreeLibrary(module);return 14;
    }
    std::uint8_t packet[6]{};
    for(auto& b:packet)if(midiTake(&b)!=1){
        std::puts("P007_DLL_INTERNAL_UART_TICK_FAIL");
        FreeLibrary(module);return 15;
    }
    if(packet[0]!=0xa5u||packet[2]!=0x90u||
       packet[3]!=60u||packet[4]!=100u||
       packet[5]!=static_cast<std::uint8_t>(
         0x5au^packet[0]^packet[1]^packet[2]^packet[3]^packet[4])){
        std::puts("P007_DLL_ORIGINAL_WIN95_UART_WIRE_FORMAT_FAIL");
        FreeLibrary(module);return 16;
    }
    midiEnable(0);
    std::puts("P007_INPROCESS_HOST_MIDI_TO_GUEST_UART_WIRE_PASS");

    if(!profile||
        profile("relative","also-relative","86box.cfg","nonabsolute")!=-1){
        std::puts("P007_PROFILE_MUST_REJECT_UNSAFE_OR_RELATIVE_PATHS");
        FreeLibrary(module);return 5;
    }
    if(!audioPull||!audioAvailable||audioAvailable()!=0||
        audioPull(empty,1u)!=0||
        !version||!init||!run||!reset||
        version()!=P007_ABI_VERSION||
        !init()||!run()||!reset()){
        std::puts("P007_DLL_ORIGINAL_CORE_SYMBOLS_FAIL");
        FreeLibrary(module);
        return 4;
    }
    /* Test original 86Box pc_init in the SAME Windows host process, with
       temporary writable paths; no Win95 / no proprietary ROM / no GUI. */
    char temp[MAX_PATH]{};
    DWORD t=GetTempPathA(MAX_PATH,temp);
    if(!t||t>=MAX_PATH){FreeLibrary(module);return 6;}
    char root[MAX_PATH]{},romdir[MAX_PATH]{},globalCfg[MAX_PATH]{};
    const int rootN=std::snprintf(root,sizeof(root),"%sP007PrivateProfile%lu",
                                 temp,GetCurrentProcessId());
    if(rootN<1||static_cast<unsigned>(rootN)>=sizeof(root) ||
       std::snprintf(romdir,sizeof(romdir),"%s\\roms",root)<1 ||
       std::snprintf(globalCfg,sizeof(globalCfg),"%s\\86box-global.cfg",root)<1){
        FreeLibrary(module);return 7;
    }
    if(!CreateDirectoryA(root,nullptr) &&
       GetLastError()!=ERROR_ALREADY_EXISTS){FreeLibrary(module);return 8;}
    if(!CreateDirectoryA(romdir,nullptr) &&
       GetLastError()!=ERROR_ALREADY_EXISTS){FreeLibrary(module);return 9;}
    if(!SetEnvironmentVariableA("LOCALAPPDATA",root)){
        FreeLibrary(module);return 10;
    }
    const int profileResult=profile(root,romdir,"86box.cfg",globalCfg);
    if(profileResult!=1){
        std::printf("P007_PRIVATE_VM_PROFILE_INIT_FAILED result=%d\n",profileResult);
        FreeLibrary(module);return 11;
    }
    if(profile(root,romdir,"86box.cfg",globalCfg)!=-2){
        std::puts("P007_DUPLICATE_PROFILE_INIT_MUST_FAIL");
        FreeLibrary(module);return 12;
    }
    std::puts("P007_ORIGINAL_86BOX_INPROCESS_PRIVATE_PROFILE_INIT_PASS");

    std::printf("P007_INPROCESS_86BOX_CPU_CORE_DLL_LOAD_PASS abi=%u\n",version());
    FreeLibrary(module);
    return 0;
}
