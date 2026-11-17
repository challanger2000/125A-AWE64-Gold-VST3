#pragma once
// Win32 path contract, shared between private supervisor and native VST3.
// Does NOT contain any Windows95/Creative/ROM data.
#ifndef _WIN32
#error P006 private VM supervisor is Windows-only
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cwchar>
namespace p006 {
struct PrivatePaths {
    wchar_t dir[1024]{};
    wchar_t machine[1024]{};
    wchar_t wav[1024]{};
};
inline bool privatePaths(PrivatePaths& out) noexcept {
    wchar_t local[768]{};
    const DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,
        static_cast<DWORD>(sizeof(local)/sizeof(local[0])));
    if(!n||n>=sizeof(local)/sizeof(local[0]))return false;
    if(swprintf_s(out.dir,L"%ls\\125A\\P006",local)<0)return false;
    if(swprintf_s(out.machine,L"%ls\\machine.ini",out.dir)<0)return false;
    if(swprintf_s(out.wav,L"%ls\\original_emu8000_live.wav",out.dir)<0)return false;
    return true;
}
inline bool isAbsoluteWindowsPath(wchar_t const* p) noexcept {
    if(!p||!p[0])return false;
    // Reject paths with unescaped quotes/newlines before CreateProcess.
    for(wchar_t const* s=p;*s;++s)
        if(*s==L'"'||*s==L'\r'||*s==L'\n')return false;
    return (p[0]&&p[1]==L':'&&
        (p[2]==L'\\'||p[2]==L'/')) ||
        (p[0]==L'\\'&&p[1]==L'\\');
}
inline bool regularFile(wchar_t const* p) noexcept {
    DWORD a=GetFileAttributesW(p);
    return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY);
}
inline bool directory(wchar_t const* p) noexcept {
    DWORD a=GetFileAttributesW(p);
    return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)!=0;
}
} // namespace p006
