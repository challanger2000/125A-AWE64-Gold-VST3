#include "P007Native.hpp"
#include "P007PrivatePaths.hpp"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <cwchar>
namespace p007 {
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
// Address within THIS plugin module, not Studio Pro executable.
static unsigned char anchor=0;
// 86Box uses process-wide globals; do not allow two plugin instances to own it.
static std::atomic<bool> s_originalCoreOwner{false};
std::uint8_t midiVelocity(float x) noexcept {
    if(!std::isfinite(x))return 0u;
    return static_cast<std::uint8_t>(
        std::clamp(x,0.f,1.f)*127.f+0.5f);
}
} // namespace
bool Core::openNonRealtime() noexcept {
    if(module_)return true;
    HMODULE own=nullptr;
    if(!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&anchor),&own))return false;
    wchar_t path[2048]{};
    const DWORD n=GetModuleFileNameW(own,path,2048u);
    if(!n||n>=2048u)return false;
    wchar_t* slash=wcsrchr(path,L'\\');
    if(!slash)return false;
    slash[1]=L'\0';
    if(wcscat_s(path,2048u,L"125A_AWE_Core.dll")!=0)return false;
    // Explicit local module path; NEVER search or spawn 86Box.exe.
    module_=LoadLibraryExW(path,nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!module_)return false;
    pull=reinterpret_cast<Pull>(GetProcAddress(module_,"p007_audio_pull"));
    midi=reinterpret_cast<Midi>(GetProcAddress(module_,"p007_midi_submit"));
    midiEnabled=reinterpret_cast<Enabled>(GetProcAddress(module_,"p007_midi_uart_enable"));
    if(!pull||!midi||!midiEnabled) {
        closeNonRealtime();
        return false;
    }
    initProfile=reinterpret_cast<InitProfile>(GetProcAddress(module_,"p007_core_init_profile"));
    boot=reinterpret_cast<Boot>(GetProcAddress(module_,"p007_core_guest_boot"));
    step=reinterpret_cast<Step>(GetProcAddress(module_,"p007_core_guest_step"));
    guestClose=reinterpret_cast<GuestClose>(GetProcAddress(module_,"p007_core_guest_close"));
    bootAssist=reinterpret_cast<BootAssist>(GetProcAddress(module_,"p007_core_guest_boot_assist"));
    if(!initProfile||!boot||!step||!guestClose||!bootAssist){
        closeNonRealtime();return false;
    }
    bool expected=false;
    if(!s_originalCoreOwner.compare_exchange_strong(expected,true)){
        closeNonRealtime();return false;
    }
    ownsInstance_=true;
    // Neither a BIOS reset nor the start of Win95 proves Creative WinMM ready.
    // Leave MIDI disabled until a *real guest-ready acknowledgment* exists.
    midiEnabled(0);
    try{
        const std::wstring folder(path);
        stop_.store(false,std::memory_order_release);
        emulatorThread_=std::thread([this,folder](){runPrivateGuest(folder);});
    }catch(...){
        closeNonRealtime();return false;
    }
    return true;
}
void Core::closeNonRealtime() noexcept {
    if(midiEnabled)midiEnabled(0);
    stop_.store(true,std::memory_order_release);
    if(emulatorThread_.joinable())emulatorThread_.join();
    pull=nullptr;midi=nullptr;midiEnabled=nullptr;
    initProfile=nullptr;boot=nullptr;step=nullptr;guestClose=nullptr;
    bootAssist=nullptr;
    if(module_){FreeLibrary(module_);module_=nullptr;}
    if(ownsInstance_){
        ownsInstance_=false;
        s_originalCoreOwner.store(false,std::memory_order_release);
    }
}
void Core::runPrivateGuest(std::wstring folder) noexcept {
    namespace fs=std::filesystem;
    try{
        // All resources are private and must be bundled in the one VST3.
        // No executable, external VM, OS device, BAT or named pipe.
        const fs::path root=privatevm::bundleRoot(fs::path(folder));
        wchar_t local[4096]{};
        const DWORD len=GetEnvironmentVariableW(L"LOCALAPPDATA",local,4096u);
        if(!len||len>=4096u){
            bootStatus_.store(-5,std::memory_order_release);return;
        }
        // Guest disk is writable ONLY in per-user profile. Named Windows
        // mutex prevents two DAW processes from writing the same guest HDD.
        struct MutexLease {
            HANDLE handle=nullptr;
            bool acquired=false;
            ~MutexLease(){
                if(acquired)ReleaseMutex(handle);
                if(handle)CloseHandle(handle);
            }
        } lease;
        lease.handle=CreateMutexW(nullptr,FALSE,
            L"Local\\125A_AWE64_Gold_P007_PrivateVM");
        if(!lease.handle){
            bootStatus_.store(-6,std::memory_order_release);return;
        }
        const DWORD wait=WaitForSingleObject(lease.handle,0);
        if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){
            bootStatus_.store(-7,std::memory_order_release);return;
        }
        lease.acquired=true;
        const fs::path work=fs::path(local)/L"125A"/L"AWE64Gold"/L"PrivateVM";
        std::error_code ec;
        if(!privatevm::prepare(root,work,ec)){
            bootStatus_.store(-8,std::memory_order_release);return;
        }
        const fs::path vm=work/L"vm";
        const fs::path roms=root/L"roms";
        const fs::path assets=root/L"assets";
        if(!fs::is_directory(assets,ec)){
            bootStatus_.store(-9,std::memory_order_release);return;
        }
        const fs::path global=work/L"86box.cfg";
        // Original 86Box is an ANSI-path C API; this initial research
        // version supports the paths which its native Win32 CRT can open.
        const auto a=vm.string(),b=roms.string(),g=global.string();
        if(stop_.load(std::memory_order_acquire))return;
        if(initProfile(a.c_str(),b.c_str(),"P007_AWE64.cfg",g.c_str())!=1){
            bootStatus_.store(-1,std::memory_order_release);return;
        }
        if(stop_.load(std::memory_order_acquire))return;
        if(boot()!=1){
            bootStatus_.store(-2,std::memory_order_release);return;
        }
        bootStatus_.store(1,std::memory_order_release);
        auto next=std::chrono::steady_clock::now();
        std::uint32_t guestMilliseconds=0u;
        while(!stop_.load(std::memory_order_acquire)){
            if(step()!=1){bootStatus_.store(-3,std::memory_order_release);break;}
            // Only original emulated keyboard controller receives Win95 boot
            // keys; no real host input events, no audio-thread work.
            (void)bootAssist(++guestMilliseconds);
            next+=std::chrono::milliseconds(1);
            // No guest CPU, disk, timers, or sleep on the DAW audio callback.
            std::this_thread::sleep_until(next);
        }
        guestClose();
        bootStatus_.store(2,std::memory_order_release);
    }catch(...){
        bootStatus_.store(-4,std::memory_order_release);
    }
}
tresult PLUGIN_API Plugin::initialize(FUnknown* context) {
    if(SingleComponentEffect::initialize(context)!=kResultOk)
        return kResultFalse;
    addEventInput(STR16("Original Creative WinMM MIDI"),16);
    addAudioOutput(STR16("Original EMU8000 Stereo"),SpeakerArr::kStereo);
    return kResultOk;
}
tresult PLUGIN_API Plugin::terminate() {
    if(active_)(void)setActive(false);
    return SingleComponentEffect::terminate();
}
tresult PLUGIN_API Plugin::setActive(TBool on) {
    if(on) {
        if(active_)return kResultOk;
        // SDK activation is non-RT. Host cannot activate if internal core
        // is missing from the SAME .vst3 folder.
        if(!core_.openNonRealtime())return kResultFalse;
        active_=true;
    } else if(active_) {
        core_.closeNonRealtime();active_=false;
    }
    return SingleComponentEffect::setActive(on);
}
tresult PLUGIN_API Plugin::canProcessSampleSize(int32 size) {
    return size==kSample32?kResultTrue:kResultFalse;
}
tresult PLUGIN_API Plugin::process(ProcessData& data) {
    // Only constant-time calls to bounded, heapless native core FIFOs;
    // the original PC CPU/timers/Win95 MUST NOT run on this DAW thread.
    if(active_&&core_.midi&&data.inputEvents) {
        const auto n=data.inputEvents->getEventCount();
        for(int32 i=0;i<n;++i){
            Event event{};
            if(data.inputEvents->getEvent(i,event)!=kResultOk)continue;
            if(event.type==Event::kNoteOnEvent){
                auto const& e=event.noteOn;
                if(e.channel>=0&&e.channel<16&&e.pitch>=0&&e.pitch<128)
                    (void)core_.midi(static_cast<std::uint8_t>(0x90|e.channel),
                        static_cast<std::uint8_t>(e.pitch),midiVelocity(e.velocity));
            }else if(event.type==Event::kNoteOffEvent){
                auto const& e=event.noteOff;
                if(e.channel>=0&&e.channel<16&&e.pitch>=0&&e.pitch<128)
                    (void)core_.midi(static_cast<std::uint8_t>(0x80|e.channel),
                        static_cast<std::uint8_t>(e.pitch),midiVelocity(e.velocity));
            }
        }
    }
    if(data.numOutputs<1||!data.outputs||data.numSamples<=0)return kResultOk;
    auto& output=data.outputs[0];
    if(output.numChannels<2||!output.channelBuffers32)return kResultOk;
    float* l=output.channelBuffers32[0],*r=output.channelBuffers32[1];
    if(!l||!r)return kResultOk;
    constexpr std::uint32_t kChunk=512u;
    std::array<float,kChunk*2u> temp{};
    const auto count=static_cast<std::uint32_t>(data.numSamples);
    for(std::uint32_t pos=0;pos<count;) {
        const std::uint32_t n=std::min(kChunk,count-pos);
        const std::uint32_t got=active_&&core_.pull?
            core_.pull(temp.data(),n):0u;
        const std::uint32_t valid=std::min(got,n);
        for(std::uint32_t i=0;i<valid;++i){
            l[pos+i]=temp[2u*i];r[pos+i]=temp[2u*i+1u];
        }
        for(std::uint32_t i=valid;i<n;++i){
            l[pos+i]=0.f;r[pos+i]=0.f;
        }
        pos+=n;
    }
    output.silenceFlags=0;
    return kResultOk;
}
} // namespace p007
