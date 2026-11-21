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
#include "p165/PresetCatalog.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include <array>
#include <cstdint>
#include <memory>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
namespace p009 {
// Prebuilt on the host lifecycle thread. The audio thread copies its
// bounded POD state at a block boundary and retires the object *without*
// freeing it. This prevents host state restores from racing chip register
// writes and preserves all 16 channels' MIDI RPN/NRPN state.
struct NativeRuntimeRestore {
    std::uint32_t program=0,bank=0;
    float master=1.f;
    std::array<std::uint8_t,16> programs{},banks{};
    std::array<std::uint16_t,16*8> midi{},advanced{};
    std::array<std::uint8_t,16> modes{};
    std::array<float,12> gui{};
    int reverb=4,chorus=2;
    std::array<bool,2> sendManual{};
    std::uint64_t sequence=0;
    NativeRuntimeRestore* next=nullptr;
};
class NativeVst3 final : public Steinberg::Vst::SingleComponentEffect,
                         public Steinberg::Vst::IMidiMapping,
                         public VSTGUI::VST3EditorDelegate {
public:
    NativeVst3() {
        midiState_.fill(0xFFFFu);
        advancedMidiState_.fill(0xFFFFu);
        fxPresetMirrors_[0].store(4u);
        fxPresetMirrors_[1].store(2u);
        for(auto& x:stateMidiMirror_)x.store(0xFFFFu);
        for(auto& x:stateAdvancedMirror_)x.store(0xFFFFu);
        for(auto& x:hostProgramEdits_)x.store(-1);
        for(auto& x:hostBankEdits_)x.store(-1);
    }
    ~NativeVst3() override { cleanStagedAndRetiredNonRealtime(); }
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
    Steinberg::uint32 PLUGIN_API getTailSamples() override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API process(
        Steinberg::Vst::ProcessData&) override;
    VSTGUI::CView* createCustomView(VSTGUI::UTF8StringPtr,
        const VSTGUI::UIAttributes&, const VSTGUI::IUIDescription*,
        VSTGUI::VST3Editor*) override;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString) override;
    Steinberg::tresult PLUGIN_API setParamNormalized(
        Steinberg::Vst::ParamID,Steinberg::Vst::ParamValue) override;
    Steinberg::Vst::ParamValue PLUGIN_API getParamNormalized(
        Steinberg::Vst::ParamID) override;
private:
    std::unique_ptr<Synth> synth_;
    // GUI-only preset index: never edited from a real-time callback.
    AWE125::PresetCatalog guiCatalog_{};
    // DAW setState may run off the GUI thread. Apply new catalog on GUI idle.
    std::atomic<AWE125::PresetCatalog*> pendingGuiCatalog_{nullptr};
    AWE125::PresetCatalog unusedFmCatalog_{};
    std::atomic<std::uint64_t> guiCatalogRevision_{1};
    std::mutex guiAssetMutex_{};
    std::string selectedUserSf2_{};
    // GUI-thread-visible last load error; guarded by guiAssetMutex_.
    std::string lastSf2LoadError_{};
    std::array<std::uint8_t,32> selectedUserSha256_{};
    bool userChoseSf2_=false;
    // UI-side atomic mirrors of real audio-thread control states for safe DAW getState.
    std::array<std::atomic<std::uint16_t>,12> guiControlMirrors_{};
    // A stopped host may request a program/bank/master edit before the
    // next audio callback. Until the processor acknowledges it, project
    // saving and GUI readback must retain the *new* host-side choice.
    // -1 means no pending controller-side change; these atomics never
    // allocate in the render callback.
    std::array<std::atomic<int>,16> hostProgramEdits_{},hostBankEdits_{};
    std::atomic<float> hostMasterEdit_{-1.f};
    std::atomic<Synth*> stagedSynth_{nullptr};
    std::atomic<Synth*> retiredSynth_{nullptr};
    // For a bounded 15-ms SF2 transition the previous REAL EMU8000 chip
    // continues rendering while its output fades to the newly loaded chip.
    // Both synths stay audio-thread-owned. Retire only after the last fade
    // sample, then reclaim the 28 MiB former chip off the realtime thread.
    std::unique_ptr<Synth> fadingSynth_;
    std::array<std::int16_t,2048> fadingPcm_{};
    std::uint32_t fadeTotalSamples_=0;
    std::uint32_t fadeRemainingSamples_=0;
    // Bounded POD host/audio state transfers. getState only reads atomic
    // snapshots, never audio-thread-owned arrays or a mutable Synth.
    std::atomic<NativeRuntimeRestore*> pendingRestore_{nullptr};
    std::atomic<NativeRuntimeRestore*> retiredRestore_{nullptr};
    std::mutex hostRestoreMutex_{};
    NativeRuntimeRestore lastHostRestore_{};
    std::atomic<std::uint64_t> restoreRequestedSeq_{0};
    std::atomic<std::uint64_t> restoreAppliedSeq_{0};
    std::atomic<std::uint64_t> stateMirrorEpoch_{0};
    std::atomic<std::uint32_t> stateProgramMirror_{0},stateBankMirror_{0};
    std::atomic<float> stateMasterMirror_{1.f};
    std::array<std::atomic<std::uint8_t>,16> stateProgramsMirror_{},
        stateBanksMirror_{},stateModesMirror_{};
    std::array<std::atomic<std::uint16_t>,16*8> stateMidiMirror_{},
        stateAdvancedMirror_{};
    void applyRuntimeRestore(const NativeRuntimeRestore&) noexcept;
    void applyPendingRestoreRealtime() noexcept;
    void publishRuntimeSnapshot() noexcept;
    void drainRetiredRestoresNonRealtime() noexcept;
    // GUI repeated bank selection after MIDI CC0: the actual chip must
    // receive a bank/program reassert, not only a boolean "ack".
    std::atomic<std::uint8_t> bankReassertRequested_{0xFFu};
    bool requestUserSf2NonRealtime(const std::string&);
    bool buildInstrumentNonRealtime(const std::filesystem::path&,
                                    std::unique_ptr<Synth>&,std::string&);
    void cleanStagedAndRetiredNonRealtime() noexcept;
    void applyStagedInstrumentRealtime() noexcept;
    std::array<std::int16_t,2048> pcm_{};
    std::array<unsigned char,16*128> redirectedDrumNotes_{};
    double sampleRate_=44100.;
    int program_=0,bank_=0;
    // CH2-16 independent Win95 Creative MIDI preset/bank states.
    // CH1 stays in program_/bank_ to preserve browser behavior.
    std::array<std::uint8_t,16> programsByChannel_{};
    std::array<std::uint8_t,16> banksByChannel_{};
    float master_=1.f;
    // Last P175's 12 original EMU8000 hardware performance modifiers.
    // 0% is exact unmodified P022 PCM; all fields are audio-thread owned.
    std::array<float,12> guiControls_{};
    int reverbPreset_=4,chorusPreset_=2;
    std::array<std::atomic<std::uint8_t>,2> fxPresetMirrors_{};
    // 0% in MANUAL = true hardware send zero. Untouched ORIGINAL mode
    // preserves each patch's original 86Box Creative effect register PCM.
    std::array<bool,2> sendManual_{};
    std::array<std::atomic<bool>,2> sendManualMirrors_{};
    bool fxEdited_=false;
    std::atomic<bool> fxPending_{false};
    // 8 original Creative CC controls per channel; 0xFFFF = untouched.
    // Never send default values unnecessarily (preserve original register PCM).
    std::array<std::uint16_t,16*8> midiState_{};
    // Additional standard MIDI CCs, independently mapped per channel.
    // NRPN is selector-only: upstream Creative driver has no NRPN synthesis.
    std::array<std::uint16_t,16*8> advancedMidiState_{};
    std::array<std::uint8_t,16> selectedParameterMode_{};
    // Committed hardware sample-RAM occupancy, in 16-bit words.
    std::atomic<std::uint64_t> activeRamWords_{0};
    std::atomic<bool> active_{false},authenticRomLoaded_{false};
    void applyMidiController(unsigned channel,unsigned slot,std::uint16_t raw) noexcept;
    void replayMidiState() noexcept;
    void resetMidiControllersForStateRestore() noexcept;
    void applyAdvancedController(unsigned channel,unsigned slot,
                                 std::uint16_t value) noexcept;
    void changeBankProgram() noexcept;
    void changeChannelBankProgram(unsigned channel) noexcept;
    void dispatch(const Steinberg::Vst::Event&) noexcept;
    void render(float*,float*,Steinberg::int32) noexcept;
    bool openPrivateSoundBanksNonRealtime();
};
}
