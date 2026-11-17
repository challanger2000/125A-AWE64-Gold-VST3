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
    if(!version||!init||!run||!reset||
        version()!=P007_ABI_VERSION||
        !init()||!run()||!reset()){
        std::puts("P007_DLL_ORIGINAL_CORE_SYMBOLS_FAIL");
        FreeLibrary(module);
        return 4;
    }
    std::printf("P007_INPROCESS_86BOX_CPU_CORE_DLL_LOAD_PASS abi=%u\n",version());
    FreeLibrary(module);
    return 0;
}
