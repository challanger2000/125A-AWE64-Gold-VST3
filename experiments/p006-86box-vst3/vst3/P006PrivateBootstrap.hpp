#pragma once
// VST3 activation path only, never called by process(). The manager is a
// separate executable; no guest disk is modified inside the DAW process.
#include "P006PrivatePaths.hpp"
#include "P006WindowsWavTail.hpp"
#include <cwchar>
namespace p006 {
inline bool startPrivateVmNonRealtime(OriginalWavTail& wavTail,
                                     P006AudioShared* ring) noexcept {
    if(!ring)return false;
    // Preserve the existing manual override for independent WAV tests.
    wchar_t overridePath[1024]{};
    DWORD n=GetEnvironmentVariableW(L"AWE64_P006_WAV_PATH",
        overridePath,static_cast<DWORD>(sizeof(overridePath)/sizeof(wchar_t)));
    if(n!=0) {
        if(n>=sizeof(overridePath)/sizeof(wchar_t))return false;
        return wavTail.startNonRealtime(ring,overridePath);
    }
    PrivatePaths p{};
    if(!privatePaths(p)||!regularFile(p.machine)){
        // Unconfigured test host / Steinberg Validator: no private launch.
        return true;
    }
    if(!wavTail.startNonRealtime(ring,p.wav))return false;
    // Locate a private companion shipped next to the plugin binary:
    // .../AWE64_Gold_P006.vst3/Contents/x86_64-win/{plugin,supervisor}
    static const wchar_t anchor[]=L"P006HostDllAnchor";
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          reinterpret_cast<LPCWSTR>(anchor),&module)){
        wavTail.stopNonRealtime();return false;
    }
    wchar_t exe[2048]{};
    DWORD len=GetModuleFileNameW(module,exe,
        static_cast<DWORD>(sizeof(exe)/sizeof(wchar_t)));
    if(len==0||len>=sizeof(exe)/sizeof(wchar_t)){
        wavTail.stopNonRealtime();return false;
    }
    wchar_t* slash=wcsrchr(exe,L'\\');
    if(!slash) {wavTail.stopNonRealtime();return false;}
    slash[1]=L'\0';
    if(wcscat_s(exe,L"P006Supervisor.exe")!=0||!regularFile(exe)){
        wavTail.stopNonRealtime();return false;
    }
    STARTUPINFOW startup{};
    startup.cb=sizeof(startup);
    startup.dwFlags=STARTF_USESHOWWINDOW;
    startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION child{};
    wchar_t command[]=L"\"P006Supervisor.exe\"";
    if(!CreateProcessW(exe,command,nullptr,nullptr,FALSE,
          CREATE_NEW_PROCESS_GROUP|CREATE_NO_WINDOW,nullptr,
          nullptr,&startup,&child)){
        wavTail.stopNonRealtime();return false;
    }
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    return true;
}
} // namespace p006
