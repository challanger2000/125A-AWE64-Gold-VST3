// NO PRIVATE CREATIVE/Win95 FILES: fake child process checks the REAL
// P006 supervisor's --config / --vmpath args, AWE32_WAV and singleton.
#include <windows.h>
#include <array>
#include <cwchar>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <chrono>
namespace fs=std::filesystem;
#define CHECK(x) do {if(!(x)){std::fwprintf(stderr,L"P006_TEST_FAIL:%d %hs\n",__LINE__,#x);return 12;}}while(0)
namespace {
bool writeText(fs::path const& p,char const* s) {
    HANDLE f=CreateFileW(p.c_str(),FILE_APPEND_DATA,
        FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;
    bool ok=WriteFile(f,s,static_cast<DWORD>(strlen(s)),&written,nullptr)
        &&written==strlen(s);
    CloseHandle(f);return ok;
}
bool start(fs::path const& exe,PROCESS_INFORMATION& pi) {
    STARTUPINFOW si{};si.cb=sizeof(si);
    std::wstring cmd=L"\""+exe.wstring()+L"\"";
    return CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,
        FALSE,CREATE_NEW_PROCESS_GROUP,nullptr,nullptr,&si,&pi)!=FALSE;
}
bool waitExit(PROCESS_INFORMATION& pi,DWORD ms,DWORD& code) {
    DWORD s=WaitForSingleObject(pi.hProcess,ms);
    bool ok=s==WAIT_OBJECT_0 && GetExitCodeProcess(pi.hProcess,&code);
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return ok;
}
std::string load(fs::path const& path) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|
        FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return {};
    char buf[2048]{};DWORD got=0;
    const BOOL ok=ReadFile(file,buf,2047,&got,nullptr);
    CloseHandle(file);return ok?std::string(buf,buf+got):std::string();
}
}
int wmain(int argc,wchar_t** argv) {
    wchar_t module[4096]{};
    CHECK(GetModuleFileNameW(nullptr,module,4096)>0);
    const wchar_t* last=wcsrchr(module,L'\\');
    const wchar_t* name=last?last+1:module;
    if(_wcsicmp(name,L"86Box.exe")==0) {
        // This is a fake emulator copy of the SAME test executable.
        CHECK(argc>=5);
        CHECK(wcscmp(argv[1],L"--vmpath")==0);
        CHECK(wcscmp(argv[3],L"--config")==0);
        wchar_t audio[2048]{},marker[2048]{};
        CHECK(GetEnvironmentVariableW(L"AWE32_WAV",audio,2048)>0);
        CHECK(wcsstr(audio,L"original_emu8000_live.wav")!=nullptr);
        CHECK(GetEnvironmentVariableW(L"AWE64_P006_TEST_MARKER",marker,2048)>0);
        CHECK(writeText(marker,"ONE_ORIGINAL_VM_STARTED\n"));
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
        return 0;
    }
    CHECK(argc==2);
    const fs::path supervisor=argv[1];
    CHECK(fs::is_regular_file(supervisor));
    wchar_t temp[2048]{};
    const DWORD n=GetTempPathW(2048,temp);
    CHECK(n>0&&n<2048);
    const auto root=fs::path(temp)/(L"P006 Auto Launch Integration "+std::to_wstring(GetCurrentProcessId()));
    const auto local=root/L"private profile with spaces";
    const auto folder=local/L"125A"/L"P006";
    const auto vm=root/L"private VM with spaces";
    const auto fake=vm/L"86Box.exe";
    const auto config=vm/L"86Box.cfg";
    const auto ini=folder/L"machine.ini";
    const auto marker=root/L"only_one_launch.txt";
    std::error_code ec;
    fs::create_directories(folder,ec);CHECK(!ec);
    fs::create_directories(vm,ec);CHECK(!ec);
    CHECK(CopyFileW(module,fake.c_str(),FALSE));
    CHECK(writeText(config,"TEST_86BOX_CONFIG_ONLY\n"));
    CHECK(SetEnvironmentVariableW(L"LOCALAPPDATA",local.c_str()));
    CHECK(SetEnvironmentVariableW(L"AWE64_P006_TEST_MARKER",marker.c_str()));
    CHECK(WritePrivateProfileStringW(L"P006",L"86BoxExe",fake.c_str(),ini.c_str()));
    CHECK(WritePrivateProfileStringW(L"P006",L"VmPath",vm.c_str(),ini.c_str()));
    CHECK(WritePrivateProfileStringW(L"P006",L"VmConfig",config.c_str(),ini.c_str()));
    CHECK(WritePrivateProfileStringW(nullptr,nullptr,nullptr,ini.c_str()));
    PROCESS_INFORMATION first{},second{};
    CHECK(start(supervisor,first));
    bool appeared=false;
    for(unsigned tries=0;tries<200;++tries) {
        if(fs::exists(marker)){appeared=true;break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    CHECK(appeared);
    CHECK(start(supervisor,second));
    DWORD otherExit=99,firstExit=99;
    CHECK(waitExit(second,3000,otherExit));CHECK(otherExit==0);
    CHECK(waitExit(first,5000,firstExit));CHECK(firstExit==0);
    CHECK(load(marker)=="ONE_ORIGINAL_VM_STARTED\n");
    CHECK(fs::exists(folder/L"P006_LastStart.log"));
    // Adversarial missing config: must refuse to spawn any process.
    fs::remove(config,ec);CHECK(!ec);
    PROCESS_INFORMATION negative{};
    CHECK(start(supervisor,negative));
    DWORD negativeExit=0;
    CHECK(waitExit(negative,3000,negativeExit));CHECK(negativeExit!=0);
    CHECK(load(marker)=="ONE_ORIGINAL_VM_STARTED\n");
    fs::remove_all(root,ec);CHECK(!ec);
    std::puts("P006_PRIVATE_86BOX_AUTOLAUNCH_SINGLETON_AND_ENV_PASS");
    return 0;
}
