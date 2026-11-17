#pragma once
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "P006WindowsBridge.hpp"
#include "P006WindowsWavTail.hpp"
#include "P006PrivateBootstrap.hpp"
namespace p006 {
class Plugin final : public Steinberg::Vst::SingleComponentEffect {
    AudioMapping audio_{};
    MidiPipe midi_{};
    OriginalWavTail wavTail_{};
    bool active_=false;
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new Plugin());
    }
    Plugin()=default;
    ~Plugin() override=default;
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API terminate() override;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(
        Steinberg::int32 symbolicSampleSize) override;
    Steinberg::tresult PLUGIN_API process(
        Steinberg::Vst::ProcessData& data) override;
};
}
