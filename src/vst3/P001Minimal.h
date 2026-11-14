#pragma once
// P001 only: one fixed ROM test voice, NOT yet Creative GM, SBK, SF2 or a user GUI.
// The unchanged EMU8000 chip runs at 44100 Hz. No heap/file/lock on process().
#include "Chip.h"
#include "P002PrivateNoteFixture.h"
#include "P002PrivateRomBank.h"
#include "P002PrivateOriginalRamNotes.h"
#include "CreativeSf2VxdSourceRelease.h"
#include "OriginalCt1745WavetableMixer.h"
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace awe64::vst3 {
inline constexpr Steinberg::Vst::ParamID kP002PrivateDiagnosticCc64Param=0x125A0064u;
inline constexpr Steinberg::Vst::ParamID kP002PrivateProgramParam=0x125A00C0u;
// VST3 carries CC input legally through sample-accurate automation,
// never LegacyMIDICCOutEvent. Only future original-source bank NoteOns
// observe these MIDI-state values; existing hardware voices are untouched.
inline constexpr Steinberg::Vst::ParamID kP002PrivateCc7VolumeParam=0x125A0007u;
inline constexpr Steinberg::Vst::ParamID kP002PrivateCc10PanParam=0x125A000Au;
inline constexpr Steinberg::Vst::ParamID kP002PrivateCc11ExpressionParam=0x125A000Bu;
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
    void noteOff(int channel,int pitch) noexcept;
    bool privateSustainCc64(std::uint8_t value) noexcept;
    void render(Steinberg::int32 begin, Steinberg::int32 end,
                float* left, float* right) noexcept;
    void write(uint16_t reg, uint16_t voice, uint16_t port, uint16_t value) noexcept;
    void write32(uint16_t reg, uint16_t voice, uint16_t port, uint32_t value) noexcept;

    awe64::Chip chip_;
    // Original 86Box SB AWE64 Gold CT1745 card stage, per-instance
    // (the pinned EMU8000 chip processing itself is unchanged).
    awe64::card::OriginalCt1745WavetableMixer originalGoldCard_{};
    bool preCardPcm16DiagnosticTap_=false; // explicit private regression only
    P002PrivateNoteFixture p002Fixture_{};
    P002PrivateRomBank p002RomBank_{};
    bool p002BankEnabled_=false; // private, original-source ROM-SF2 bank
    // Actual original Creative captured RAM source: loaded off-RT
    // into unchanged EMU8000 but no inferred RAM NoteOn voice plan.
    bool p002PrivateRamPreloaded_=false;
    // Explicit private *write-only* native register evidence is captured
    // in a fixed-size adapter buffer on process(); saved only when
    // setActive(false) runs outside the audio callback. No automatic
    // alignment or original Creative provenance is claimed.
    std::filesystem::path privateNativeRegisterTracePath_{};
    // Real original-EMU8000 pre-card stereo PCM16. Entire storage is
    // allocated off the audio thread; recording is bounded and fails
    // closed on overflow. Written only from setActive(false).
    std::filesystem::path privateNativeDryWavPath_{};
    std::vector<std::int16_t> privateNativeDryPcm_{};
    std::size_t privateNativeDryFrames_=0u;
    bool privateNativeDryOverflow_=false;
    // Optional SHA-pinned ORIGINAL Win95 source NoteOn/Off port
    // capture, only alongside the separately authenticated RAM preload.
    // Captured at C4(60) and D4(62), velocity 100, User Bank 1.
    P002PrivateOriginalRamNotes p002OriginalRamNotes_{};
    bool p002DiagnosticEnabled_=false; // private explicitly selected source diagnostic
    // Eight independently source-verified note groups. No runtime
    // physical voice stealing/retrigger; each pre-authenticated group
    // can start exactly once in this private diagnostic.
    std::array<bool,kP002MaxSourceNoteGroups> p002Started_{};
    std::array<bool,kP002MaxSourceNoteGroups> p002KeyDown_{};
    std::array<int32_t, 980u*2u> scratch_{};
    int playingKey_ = -1;
    bool active_ = false;
    bool processing_ = false;
};
}
