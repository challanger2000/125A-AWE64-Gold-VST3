#pragma once
// P007 – an actual native Windows-x64 VST3 with the ORIGINAL 86Box core
// loaded inside THIS DAW process. NO CreateProcess/86Box.exe/P006 bridge.
// The private Win95 headless boot is NOT implemented; this prototype
// must not be described as a playable AWE instrument.
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include <windows.h>
#include <cstdint>
namespace p007 {
class Core final {
    HMODULE module_=nullptr;
public:
    using Pull=std::uint32_t (__cdecl*)(float*,std::uint32_t);
    using Midi=int (__cdecl*)(std::uint8_t,std::uint8_t,std::uint8_t);
    using Enabled=void (__cdecl*)(int);
    Pull pull=nullptr;
    Midi midi=nullptr;
    Enabled midiEnabled=nullptr;
    Core()=default;
    Core(Core const&)=delete;
    Core& operator=(Core const&)=delete;
    ~Core(){closeNonRealtime();}
    bool openNonRealtime() noexcept;
    void closeNonRealtime() noexcept;
    [[nodiscard]] bool present() const noexcept{return module_!=nullptr;}
};
class Plugin final : public Steinberg::Vst::SingleComponentEffect {
    Core core_{};
    bool active_=false;
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new Plugin());
    }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API terminate() override;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData&) override;
};
} // namespace p007
