// Research-only Windows QA host for the original 86Box core DLL.
// NOT part of the final VST3 package, NOT an external 86Box process.
// Requires locally supplied private licensed Win95+Creative resources.
// Executes original 86Box's guest init/boot/CPU-step/audio/MIDI exports.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include "P007PrivatePaths.hpp"
namespace fs=std::filesystem;
using namespace std::chrono_literals;
using InitFn=int (*)(const char*,const char*,const char*,const char*);
using BootFn=int (*)();
using StepFn=int (*)();
using CloseFn=void (*)();
using ReadyFn=int (*)();
using MidiFn=int (*)(std::uint8_t,std::uint8_t,std::uint8_t);
using PullFn=std::uint32_t (*)(float*,std::uint32_t);
using AvailableFn=std::uint32_t (*)();
using DropFn=std::uint64_t (*)();
template<class T> T bind(HMODULE dll,const char* name) {
    FARPROC f=GetProcAddress(dll,name);
    return f?std::bit_cast<T>(f):nullptr;
}
static int selftest() {
    auto root=p007::privatevm::bundleRoot(
        fs::path(L"C:\\P007QA\\AWE64_Gold_P007.vst3\\Contents\\x86_64-win\\125A_AWE_Core.dll"));
    if(root!=fs::path(L"C:\\P007QA\\AWE64_Gold_P007.vst3\\Contents\\Resources\\PrivateVM"))
        return 41;
    std::error_code ec;
    if(p007::privatevm::prepare(fs::path(L"C:\\P007_MISSING_RESOURCE_BUNDLE"),
                               fs::temp_directory_path()/L"P007-no-private-must-not-create",ec))
        return 42;
    std::puts("P007_PRIVATE_BOOT_SMOKE_SELFTEST_PASS");
    return 0;
}
int main(int argc,char** argv) {
    if(argc==2 && std::string(argv[1])=="--selftest")return selftest();
    if(argc<2||argc>3) {
        std::puts("Research QA: P007PrivateBootSmoke.exe <absolute path to 125A_AWE_Core.dll> [max_1ms_slices]");
        std::puts("The program makes a PRIVATE copy of guest HDD in TEMP; never runs 86Box.exe or writes original .vst3.");
        return 2;
    }
    std::uint32_t budget=180000u;
    if(argc==3) {
        const unsigned long v=std::strtoul(argv[2],nullptr,10);
        if(v<1000ul||v>360000ul)return 3;
        budget=static_cast<std::uint32_t>(v);
    }
    std::error_code ec;
    fs::path dll=fs::absolute(fs::path(argv[1]),ec);
    if(ec||!fs::is_regular_file(dll,ec))return 4;
    const fs::path bundled=p007::privatevm::bundleRoot(dll);
    if(!fs::is_regular_file(bundled/L"vm"/L"win95.img",ec) ||
       !fs::is_directory(bundled/L"roms",ec) ||
       !fs::is_directory(bundled/L"assets",ec)) {
        std::puts("P007_PRIVATE_BOOT_SMOKE_NO_PRIVATE_WIN95_OR_ROM_ASSETS");
        return 5;
    }
    const fs::path writable=fs::temp_directory_path()/
        (L"P007-ResearchGuest-"+std::to_wstring(GetCurrentProcessId()));
    if(!p007::privatevm::prepare(bundled,writable,ec)){
        std::printf("P007_PRIVATE_WORKING_COPY_FAILED %s\n",ec.message().c_str());
        return 6;
    }
    // Only research process is subject to timeout. Never terminate the DAW.
    std::atomic<bool> finished{false};
    std::thread watchdog([&]{
        const auto limit=std::chrono::steady_clock::now()+
             std::chrono::milliseconds(budget)+90s;
        while(!finished.load(std::memory_order_acquire)){
            if(std::chrono::steady_clock::now()>=limit){
                std::fputs("P007_PRIVATE_BOOT_WATCHDOG_HUNG_OR_TOO_SLOW\n",stderr);
                std::fflush(stderr);
                TerminateProcess(GetCurrentProcess(),125);
            }
            std::this_thread::sleep_for(500ms);
        }
    });
    const auto done=[&] {
        finished.store(true,std::memory_order_release);
        watchdog.join();
    };
    HMODULE core=LoadLibraryExW(dll.c_str(),nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!core){std::printf("P007_PRIVATE_CORE_LOAD_FAILED code=%lu\n",GetLastError());done();return 7;}
    const InitFn init=bind<InitFn>(core,"p007_core_init_profile");
    const BootFn boot=bind<BootFn>(core,"p007_core_guest_boot");
    const StepFn step=bind<StepFn>(core,"p007_core_guest_step");
    const CloseFn close=bind<CloseFn>(core,"p007_core_guest_close");
    const ReadyFn ready=bind<ReadyFn>(core,"p007_midi_guest_ready");
    const MidiFn midi=bind<MidiFn>(core,"p007_midi_submit");
    const PullFn pull=bind<PullFn>(core,"p007_audio_pull");
    const AvailableFn available=bind<AvailableFn>(core,"p007_audio_available");
    const DropFn dropped=bind<DropFn>(core,"p007_audio_dropped_frames");
    struct DiskTrace {std::uint64_t readCalls,readSectors,writeCalls,writeSectors;
                      std::uint32_t lastReadLba,lastWriteLba,lastReadId,lastWriteId;};
    const auto diskTrace=bind<int (*)(DiskTrace*)>(core,"p007_trace_get");
    if(!init||!boot||!step||!close||!ready||!midi||!pull||!available||!dropped){
        std::puts("P007_PRIVATE_CORE_EXPORTS_MISSING");
        FreeLibrary(core);done();return 8;
    }
    const auto vm=(writable/L"vm").string();
    const auto roms=(bundled/L"roms").string();
    const auto glob=(writable/L"86box.cfg").string();
    const int profile=init(vm.c_str(),roms.c_str(),"P007_AWE64.cfg",glob.c_str());
    std::printf("P007_PRIVATE_PROFILE_RESULT=%d\n",profile);
    std::fflush(stdout);
    if(profile!=1){FreeLibrary(core);done();return 9;}
    const int bootResult=boot();
    std::printf("P007_PRIVATE_HARDWARE_BOOT_RESULT=%d\n",bootResult);
    std::fflush(stdout);
    if(bootResult!=1){FreeLibrary(core);done();return 10;}
    std::array<float,2048u> stereo{};
    std::uint64_t frames=0,nonzero=0;
    float peak=0.f;
    bool noteSent=false,noteOff=false;
    std::uint32_t noteAt=0;
    int rc=11;
    auto next=std::chrono::steady_clock::now();
    for(std::uint32_t tick=0;tick<budget;++tick) {
        if(step()!=1){rc=12;break;}
        while(available()>0u){
            const auto count=pull(stereo.data(),1024u);
            if(count==0u)break;
            frames+=count;
            for(std::uint32_t i=0;i<count*2u;++i){
                const float abs=std::fabs(stereo[i]);
                if(noteSent && abs>0.000001f)++nonzero;
                if(noteSent)peak=std::max(abs,peak);
            }
        }
        if(ready() && !noteSent) {
            noteSent=midi(0x90u,60u,110u)==1;
            noteAt=tick;
            std::printf("P007_REAL_GUEST_CREATIVE_WINMM_READY_AT_SLICE=%u submit=%d\n",
                        tick,noteSent?1:0);
        }
        if(noteSent && !noteOff && tick-noteAt>=5000u) {
            noteOff=midi(0x80u,60u,0u)==1;
        }
        if(noteOff && tick-noteAt>=5500u && nonzero>0u) {rc=0;break;}
        if(tick%10000u==0u) {
            DiskTrace disk{};
            if(diskTrace) (void)diskTrace(&disk);
            std::printf("P007_PRIVATE_BOOT_PROGRESS slices=%u ready=%d"
                        " audio_frames=%llu peak=%.6f hdd_reads=%llu"
                        " last_lba=%u hdd_writes=%llu\n",
                tick,ready(),static_cast<unsigned long long>(frames),
                static_cast<double>(peak),
                static_cast<unsigned long long>(disk.readCalls),disk.lastReadLba,
                static_cast<unsigned long long>(disk.writeCalls));
            std::fflush(stdout);
        }
        next+=1ms;
        std::this_thread::sleep_until(next);
    }
    std::printf("P007_PRIVATE_RESULT code=%d ready=%d midi_noteon=%d noteoff=%d"
                " frames=%llu nonzero_samples_after_note=%llu peak=%.9f dropped=%llu\n",
        rc,ready(),noteSent,noteOff,
        static_cast<unsigned long long>(frames),
        static_cast<unsigned long long>(nonzero),
        static_cast<double>(peak),
        static_cast<unsigned long long>(dropped()));
    std::fflush(stdout);
    // End original emulated hardware in the research process BEFORE unloading.
    close();
    FreeLibrary(core);
    done();
    if(rc==0)std::puts("P007_PRIVATE_REAL_WIN95_CREATIVE_MIDI_TO_PCM_SMOKE_PASS");
    else std::puts("P007_PRIVATE_REAL_WIN95_CREATIVE_MIDI_TO_PCM_NOT_YET_PROVEN");
    fs::remove_all(writable,ec);
    return rc;
}
