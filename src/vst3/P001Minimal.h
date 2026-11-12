#pragma once
// P001 only: one fixed ROM test voice, NOT yet Creative GM, SBK, SF2 or a user GUI.
// The unchanged EMU8000 chip runs at 44100 Hz. No heap/file/lock on process().
#include "Chip.h"
#include "P002PrivateNoteFixture.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace awe64::vst3 {
class Minimal final : public Steinberg::Vst::SingleComponentEffect {
public:
    Minimal() = default;
    ~Minimal() override = default;
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new Minimal);
    }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API terminate() override;
    Steinberg::tresult PLUGIN_API setBusArrangements(
        Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numInputs,
        Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOutputs) override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) override;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API setProcessing(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* stream) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* stream) override;

    OBJ_METHODS(Minimal, SingleComponentEffect)
    REFCOUNT_METHODS(SingleComponentEffect)
private:
    void noteOn(int channel, int pitch, float velocity) noexcept;
    void noteOff(int pitch) noexcept;
    void render(Steinberg::int32 begin, Steinberg::int32 end,
                float* left, float* right) noexcept;
    void write(uint16_t reg, uint16_t voice, uint16_t port, uint16_t value) noexcept;
    void write32(uint16_t reg, uint16_t voice, uint16_t port, uint32_t value) noexcept;

    awe64::Chip chip_;
    P002PrivateNoteFixture p002Fixture_{};
    bool p002DiagnosticEnabled_=false; // private explicitly selected source diagnostic
    std::array<int32_t, 980u*2u> scratch_{};
    int playingKey_ = -1;
    bool active_ = false;
    bool processing_ = false;
};
}
