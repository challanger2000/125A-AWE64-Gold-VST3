#pragma once
// P007 – an actual native Windows-x64 VST3 with the ORIGINAL 86Box core
// loaded inside THIS DAW process. NO CreateProcess/86Box.exe/P006 bridge.
// Original Win95/Creative has passed local Linux-core NoteOn->PCM;
 // production Windows-VST3 still needs independent DAW acceptance.
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "P007OutputRate.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <atomic>
#include <string>
#include <thread>
namespace p007 {
class Core final {
    HMODULE module_=nullptr;
    bool ownsInstance_=false;
    std::thread emulatorThread_{};
    std::atomic<bool> stop_{false};
    std::atomic<int> bootStatus_{0};
    void runPrivateGuest(std::wstring moduleFolder) noexcept;
public:
    using Pull=std::uint32_t (__cdecl*)(float*,std::uint32_t);
    using Trim=std::uint32_t (__cdecl*)(std::uint32_t,std::uint32_t);
    using Midi=int (__cdecl*)(std::uint8_t,std::uint8_t,std::uint8_t);
    using Enabled=void (__cdecl*)(int);
    using InitProfile=int (__cdecl*)(const char*,const char*,const char*,const char*);
    using Boot=int (__cdecl*)();
    using Step=int (__cdecl*)();
    using GuestClose=void (__cdecl*)();
    using BootAssist=int (__cdecl*)(std::uint32_t);
    Pull pull=nullptr;
    Trim trimBacklog=nullptr;
    Midi midi=nullptr;
    Enabled midiEnabled=nullptr;
    InitProfile initProfile=nullptr;
    Boot boot=nullptr;
    Step step=nullptr;
    GuestClose guestClose=nullptr;
    BootAssist bootAssist=nullptr;
    Core()=default;
    Core(Core const&)=delete;
    Core& operator=(Core const&)=delete;
    ~Core(){closeNonRealtime();}
    bool openNonRealtime() noexcept;
    void closeNonRealtime() noexcept;
    [[nodiscard]] bool present() const noexcept{return module_!=nullptr;}
    [[nodiscard]] int bootStatus() const noexcept{return bootStatus_.load(std::memory_order_acquire);}
};
class Plugin final : public Steinberg::Vst::SingleComponentEffect {
    Core core_{};
    OutputRate outputRate_{};
    bool active_=false;
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new Plugin());
    }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API terminate() override;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32) override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup&) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData&) override;
};
} // namespace p007
