// P006 – PRIVATE 86Box companion supervisor (Windows x64).
// Launches existing 86Box fork with its ORIGINAL Win95 Creative driver.
// No VXD port, no Creative files/VM/ROM/SBK embedded or copied.
//
// Lives independently of Studio Pro: if DAW quits, the guest disk is
// NOT killed abruptly. The named process mutex prevents a second VM
// launch for the same private project while 86Box remains alive.
#include "P006PrivatePaths.hpp"
#include <windows.h>
#include <array>
#include <cwchar>
#include <cstdio>
namespace {
constexpr wchar_t kMutex[]=L"Local\\125A_P006_ORIGINAL_VM_SINGLETON";
void status(wchar_t const* message,DWORD error=0) noexcept {
    p006::PrivatePaths p{};
    if(!p006::privatePaths(p))return;
    wchar_t path[1024]{};
    if(swprintf_s(path,L"%ls\\P006_LastStart.log",p.dir)<0)return;
    HANDLE f=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,
        CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE)return;
    wchar_t text[512]{};
    swprintf_s(text,L"P006 %ls; winerror=%lu\r\n",message,error);
    // ASCII log messages only; avoid assuming an encoding for paths.
    char bytes[512]{};unsigned n=0;
    while(text[n]&&n<sizeof(bytes)-1) {
        bytes[n]=text[n]<128?static_cast<char>(text[n]):'?';
        ++n;
    }
    DWORD written=0;
    (void)WriteFile(f,bytes,n,&written,nullptr);
    CloseHandle(f);
}
bool readPrivate(wchar_t const* ini,wchar_t const* key,
                 wchar_t* out,DWORD cap) noexcept {
    const DWORD n=GetPrivateProfileStringW(
        L"P006",key,L"",out,cap,ini);
    return n>0&&n<cap-1&&p006::isAbsoluteWindowsPath(out);
}
bool appendQuoted(wchar_t* dest,size_t cap,wchar_t const* piece) noexcept {
    const size_t len=wcslen(dest),n=wcslen(piece);
    if(len>cap||n+4u>cap-len)return false;
    dest[len]=L' ';
    dest[len+1]=L'"';
    for(size_t i=0;i<n;++i)dest[len+2+i]=piece[i];
    dest[len+2+n]=L'"';dest[len+3+n]=L'\0';
    return true;
}
}
int wmain() {
    p006::PrivatePaths p{};
    if(!p006::privatePaths(p))return 3;
    if(!p006::regularFile(p.machine)){
        status(L"MISSING_PRIVATE_MACHINE_INI");return 4;
    }
    HANDLE mutex=CreateMutexW(nullptr,FALSE,kMutex);
    if(!mutex)return 5;
    if(GetLastError()==ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        // Existing supervisor owns the VM; do not touch its WAV file.
        return 0;
    }
    // A valid P006 config must point to the user's OWN files.
    std::array<wchar_t,2048> exe{},vm{},config{},rom{};
    if(!readPrivate(p.machine,L"86BoxExe",exe.data(),
            static_cast<DWORD>(exe.size())) ||
       !readPrivate(p.machine,L"VmPath",vm.data(),
            static_cast<DWORD>(vm.size())) ||
       !readPrivate(p.machine,L"VmConfig",config.data(),
            static_cast<DWORD>(config.size())) ||
       !p006::regularFile(exe.data()) ||
       !p006::directory(vm.data()) ||
       !p006::regularFile(config.data())) {
        status(L"PRIVATE_VM_PATH_INVALID");
        CloseHandle(mutex);return 6;
    }
    bool hasRom=readPrivate(p.machine,L"RomPath",rom.data(),
        static_cast<DWORD>(rom.size()));
    if(hasRom&&!p006::directory(rom.data())){
        status(L"PRIVATE_ROM_DIRECTORY_INVALID");
        CloseHandle(mutex);return 7;
    }
    // Never delete the user's Win95 disk image. Only rotate our own WAV.
    if(!DeleteFileW(p.wav)) {
        const DWORD e=GetLastError();
        if(e!=ERROR_FILE_NOT_FOUND){
            status(L"WAV_BUSY_OR_UNREMOVABLE",e);
            CloseHandle(mutex);return 8;
        }
    }
    if(!SetEnvironmentVariableW(L"AWE32_WAV",p.wav)){
        status(L"SET_AWE32_WAV_FAILED",GetLastError());
        CloseHandle(mutex);return 9;
    }
    // Source-verified 86Box arguments: --vmpath / --config / --rompath.
    // Quote paths with spaces; forbid embedded quotes via readPrivate().
    std::array<wchar_t,8192> command{};
    if(swprintf_s(command.data(),command.size(),L"\"%ls\"",
          exe.data())<0 ||
       !appendQuoted(command.data(),command.size(),L"--vmpath") ||
       !appendQuoted(command.data(),command.size(),vm.data()) ||
       !appendQuoted(command.data(),command.size(),L"--config") ||
       !appendQuoted(command.data(),command.size(),config.data()) ||
       (hasRom&&(!appendQuoted(command.data(),command.size(),
             L"--rompath") ||
             !appendQuoted(command.data(),command.size(),rom.data())))){
        status(L"COMMAND_TOO_LONG");
        CloseHandle(mutex);return 10;
    }
    STARTUPINFOW startup{};
    startup.cb=sizeof(startup);
    startup.dwFlags=STARTF_USESHOWWINDOW;
    startup.wShowWindow=SW_SHOWMINNOACTIVE;
    PROCESS_INFORMATION child{};
    // The emulator process remains independent of the DAW lifetime.
    if(!CreateProcessW(exe.data(),command.data(),nullptr,nullptr,
        FALSE,CREATE_NEW_PROCESS_GROUP,nullptr,
        vm.data(),&startup,&child)){
        status(L"ORIGINAL_86BOX_START_FAILED",GetLastError());
        CloseHandle(mutex);return 11;
    }
    CloseHandle(child.hThread);
    status(L"ORIGINAL_86BOX_STARTED");
    (void)WaitForSingleObject(child.hProcess,INFINITE);
    CloseHandle(child.hProcess);
    CloseHandle(mutex);
    return 0;
}
