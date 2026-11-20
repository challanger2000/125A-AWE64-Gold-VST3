#pragma once
// P009: isolated NATIVE VST3 experiment, never overwrite P007's product ID.
// Driver: AWE32Emu's reconstructed SBAWE.VXD logic, NOT a Windows95 VM.
// Chip: original 86Box-derived EMU8000 C core, NOT an 86Box process.
// QA status: PROTOTYPE ONLY. Upstream still contains heap allocations on
// MIDI/render callbacks; realtime/parallel-host release acceptance NOT MET.
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/vst/vstparameters.h"
#include <array>
#include <cstdint>
#include <memory>
class Synth;
namespace p009 {
class NativeVst3 final : public Steinberg::Vst::SingleComponentEffect {
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(
            new NativeVst3());
    }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API terminate() override;
    Steinberg::tresult PLUGIN_API setupProcessing(
        Steinberg::Vst::ProcessSetup&) override;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool) override;
    Steinberg::tresult PLUGIN_API setProcessing(Steinberg::TBool) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(
        Steinberg::int32) override;
    Steinberg::uint32 PLUGIN_API getLatencySamples() override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API process(
        Steinberg::Vst::ProcessData&) override;
private:
    std::unique_ptr<Synth> synth_;
    std::array<std::int16_t,2048> pcm_{};
    std::array<unsigned char,16*128> redirectedDrumNotes_{};
    double sampleRate_=44100.;
    int program_=0,bank_=0;
    float master_=1.f;
    bool active_=false,authenticRomLoaded_=false;
    void changeBankProgram() noexcept;
    void dispatch(const Steinberg::Vst::Event&) noexcept;
    void render(float*,float*,Steinberg::int32) noexcept;
    bool openPrivateSoundBanksNonRealtime();
};
}
