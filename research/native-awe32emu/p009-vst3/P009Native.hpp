#pragma once
// P009: isolated NATIVE VST3 experiment, never overwrite P007's product ID.
// Driver: AWE32Emu's reconstructed SBAWE.VXD logic, NOT a Windows95 VM.
// Chip: original 86Box-derived EMU8000 C core, NOT an 86Box process.
// QA status: PROTOTYPE ONLY. Upstream still contains heap allocations on
// MIDI/render callbacks; realtime/parallel-host release acceptance NOT MET.
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "Synth.h"
#include <array>
#include <cstdint>
#include <memory>
namespace p009 {
class NativeVst3 final : public Steinberg::Vst::SingleComponentEffect,
                         public Steinberg::Vst::IMidiMapping {
public:
    NativeVst3() {
        midiState_.fill(0xFFFFu);
        advancedMidiState_.fill(0xFFFFu);
    }
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID,void**) override;
    Steinberg::uint32 PLUGIN_API addRef() override {
        return SingleComponentEffect::addRef();
    }
    Steinberg::uint32 PLUGIN_API release() override {
        return SingleComponentEffect::release();
    }
    Steinberg::tresult PLUGIN_API getMidiControllerAssignment(
        Steinberg::int32 bus, Steinberg::int16 channel,
        Steinberg::Vst::CtrlNumber controller, Steinberg::Vst::ParamID& id) override;
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
    // CH2-16 independent Win95 Creative MIDI preset/bank states.
    // CH1 stays in program_/bank_ to preserve browser behavior.
    std::array<std::uint8_t,16> programsByChannel_{};
    std::array<std::uint8_t,16> banksByChannel_{};
    float master_=1.f;
    // 8 original Creative CC controls per channel; 0xFFFF = untouched.
    // Never send default values unnecessarily (preserve original register PCM).
    std::array<std::uint16_t,16*8> midiState_{};
    // Additional standard MIDI CCs, independently mapped per channel.
    // NRPN is selector-only: upstream Creative driver has no NRPN synthesis.
    std::array<std::uint16_t,16*8> advancedMidiState_{};
    std::array<std::uint8_t,16> selectedParameterMode_{};
    bool active_=false,authenticRomLoaded_=false;
    void applyMidiController(unsigned channel,unsigned slot,std::uint16_t raw) noexcept;
    void replayMidiState() noexcept;
    void applyAdvancedController(unsigned channel,unsigned slot,
                                 std::uint16_t value) noexcept;
    void changeBankProgram() noexcept;
    void dispatch(const Steinberg::Vst::Event&) noexcept;
    void render(float*,float*,Steinberg::int32) noexcept;
    bool openPrivateSoundBanksNonRealtime();
};
}
