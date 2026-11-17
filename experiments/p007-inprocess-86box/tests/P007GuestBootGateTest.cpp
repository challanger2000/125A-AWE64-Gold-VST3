#include <windows.h>
#include <bit>
#include <cstdio>
#include <cstdlib>
using Function=int (__cdecl*)();
int main(int argc,char** argv) {
    if(argc!=2) return 10;
    HMODULE dll=LoadLibraryA(argv[1]);
    if(!dll) return 11;
    auto boot=std::bit_cast<Function>(GetProcAddress(dll,"p007_core_guest_boot"));
    auto step=std::bit_cast<Function>(GetProcAddress(dll,"p007_core_guest_step"));
    auto stage=std::bit_cast<Function>(GetProcAddress(dll,"p007_core_guest_stage"));
    auto close=std::bit_cast<void (__cdecl*)()>(GetProcAddress(dll,"p007_core_guest_close"));
    if(!boot||!step||!stage||!close) return 12;
    if(stage()!=0||step()!=0||boot()!=-1) return 13;
    close();
    if(stage()!=0) return 14;
    std::puts("P007_NO_PRIVATE_PROFILE_SAFE_HEADLESS_BOOT_GATE_PASS");
    FreeLibrary(dll);
    return 0;
}
