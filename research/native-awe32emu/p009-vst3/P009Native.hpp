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
#include <cstring>
#include <type_traits>
#include <memory>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
namespace p009 {
// A GUI/host edit is a *versioned transaction*, not only a scalar value.
// A new same-valued edit is still a NEW accepted host command. Audio must
// never acknowledge that edit by comparing only its numeric value.
// The 64-bit CAS is lock-free on the supported Windows x64 target.
template <typename Value>
class VersionedHostEdit {
    static_assert(sizeof(Value)==4u &&
                  std::is_trivially_copyable<Value>::value,
                  "4-byte atomically versioned host edits only");
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "native x64 lock-free atomic host edit required");
    // Retain LAST accepted host value and generation even after the audio
    // thread acknowledges it. A reader that pinned an *older* A snapshot
    // must still be able to overlay a newer B host edit after B's pending
    // flag has been cleared. Clearing by storing -1 lost this provenance.
    static constexpr std::uint64_t kPending=1ull<<63u;
    std::atomic<std::uint64_t> bits_{};
    static std::uint32_t encode(Value v) noexcept {
        std::uint32_t x=0;
        std::memcpy(&x,&v,sizeof(x));
        return x;
    }
public:
    static Value decode(std::uint64_t token) noexcept {
        const std::uint32_t x=static_cast<std::uint32_t>(token);
        Value value{};
        std::memcpy(&value,&x,sizeof(x));
        return value;
    }
    static std::uint32_t generation(std::uint64_t token) noexcept {
        return static_cast<std::uint32_t>((token>>32u)&0x7FFFFFFFu);
    }
    static bool pending(std::uint64_t token) noexcept {
        return (token&kPending)!=0u;
    }
    VersionedHostEdit() noexcept {
        bits_.store(encode(Value(-1)),std::memory_order_relaxed);
    }
    std::uint64_t snapshot() const noexcept {
        return bits_.load(std::memory_order_acquire);
    }
    Value load(std::memory_order order=std::memory_order_acquire) const noexcept {
        const auto token=bits_.load(order);
        return pending(token)?decode(token):Value(-1);
    }
    void store(Value v,
               std::memory_order=std::memory_order_release) noexcept {
        auto old=bits_.load(std::memory_order_acquire);
        for(;;){
            const auto nextGeneration=
                (generation(old)+1u)&0x7FFFFFFFu;
            const auto updated=kPending |
                (std::uint64_t(nextGeneration)<<32u) |
                static_cast<std::uint64_t>(encode(v));
            if(bits_.compare_exchange_weak(old,updated,
                    std::memory_order_acq_rel,std::memory_order_acquire))
                return;
        }
    }
    bool clearIfUnchanged(std::uint64_t token) noexcept {
        // Do NOT overwrite accepted value/version! Only clear pending.
        // A reader with an old snapshot can still see the newer accepted
        // generation and serialize its value coherently.
        return bits_.compare_exchange_strong(token,token&~kPending,
                 std::memory_order_acq_rel,std::memory_order_acquire);
    }
};
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
    // RPN 0/0 Data Entry that ACTUALLY changed hardware bend range,
    // independent of a later RPN NULL or unsupported NRPN selection.
    std::array<std::uint8_t,16> bendRangeValid{};
    // Generation of the latest host edit already integrated/superseded in
    // this exact immutable audio snapshot (not the current pending flag).
    std::array<std::uint64_t,16> ackProgram{},ackBank{};
    std::uint64_t ackMaster=0u;
    std::array<std::uint64_t,12> ackGui{};
    std::array<std::uint64_t,2> ackFx{},ackSend{};
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
        for(auto& x:hostGuiEdits_)x.store(-1);
        for(auto& x:hostFxEdits_)x.store(-1);
        for(auto& x:hostSendEdits_)x.store(-1);
        stateFxMirrors_[0].store(4u);
        stateFxMirrors_[1].store(2u);
        // The initial saveable chip state has no MIDI controllers set.
        stableSnapshots_[0].data.midi.fill(0xFFFFu);
        stableSnapshots_[0].data.advanced.fill(0xFFFFu);
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
    // Host/non-RT transaction gate: a saved V5 owner SHA/path and the
    // corresponding program/master/FX snapshot must come from ONE accepted
    // project generation, even with independent host save/restore threads.
    // Never acquired by process() or any original-chip callback.
    std::mutex hostSessionMutex_{};
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
    std::array<VersionedHostEdit<int>,16> hostProgramEdits_{},hostBankEdits_{};
    VersionedHostEdit<float> hostMasterEdit_{};
    // Unacknowledged stopped-transport editor changes, never merged with
    // unrelated audio generations. -1 means the RT epoch is authoritative.
    std::array<VersionedHostEdit<int>,12> hostGuiEdits_{};
    std::array<VersionedHostEdit<int>,2> hostFxEdits_{},hostSendEdits_{};
    // Audio-owned acknowledgments copied into every published state slot.
    // A host getter compares its pinned slot's generation with the
    // retained last accepted host edit; no global seqlock or host waiting.
    std::array<std::uint64_t,16> audioAckProgram_{},audioAckBank_{};
    std::uint64_t audioAckMaster_=0u;
    std::array<std::uint64_t,12> audioAckGui_{};
    std::array<std::uint64_t,2> audioAckFx_{},audioAckSend_{};
    std::atomic<Synth*> stagedSynth_{nullptr};
    // Two bounded old-chip slots prevent a pending bank from stalling if
    // it was requested during the previous 15-ms fade. Only the audio
    // thread publishes detached Synths; only host/UI deletes their heaps.
    std::atomic<Synth*> retiredSynth_{nullptr};
    std::atomic<Synth*> retiredSynthSpare_{nullptr};
    // For a bounded 15-ms SF2 transition the previous REAL EMU8000 chip
    // continues rendering while its output fades to the newly loaded chip.
    // Both synths stay audio-thread-owned. Retire only after the last fade
    // sample, then reclaim the 28 MiB former chip off the realtime thread.
    std::unique_ptr<Synth> fadingSynth_;
    std::array<std::int16_t,2048> fadingPcm_{};
    std::uint32_t fadeTotalSamples_=0;
    std::uint32_t fadeRemainingSamples_=0;
    // Audio-thread-only evidence of the outgoing chip actually producing
    // PCM. A silent project restore must NOT attenuate its first new note.
    bool synthHadAudiblePcm_=false;
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
    // Include ALL audible GUI/FX/SEND state in the SAME audio snapshot
    // generation; independent GUI mirrors would tear during automation.
    std::array<std::atomic<std::uint16_t>,12> stateGuiMirrors_{};
    std::array<std::atomic<std::uint8_t>,2> stateFxMirrors_{};
    std::array<std::atomic<bool>,2> stateSendMirrors_{};
    // Immutable read-pinned snapshots. Writer (audio OR stopped lifecycle)
    // only modifies a NONpublished, UNpinned slot. Host saves pin a slot
    // with one CAS, then memcpy a complete generation with no RT locking.
    // Four slots make fast zero-sample automation bursts saveable without
    // waiting for the 512 individual controller/GUI atomic mirror loads.
    struct StateSnapshotSlot {
        std::atomic<int> pins{0}; // -1 = single RT writer, >=0 host readers
        NativeRuntimeRestore data{};
    };
    std::array<StateSnapshotSlot,4> stableSnapshots_{};
    std::atomic<unsigned> publishedSnapshot_{0u};
    void applyRuntimeRestore(const NativeRuntimeRestore&) noexcept;
    void applyPendingRestoreRealtime() noexcept;
    bool publishRuntimeSnapshot() noexcept;
    void drainRetiredRestoresNonRealtime() noexcept;
    // GUI repeated bank selection after MIDI CC0: the actual chip must
    // receive a bank/program reassert, not only a boolean "ack".
    std::atomic<std::uint8_t> bankReassertRequested_{0xFFu};
    bool requestUserSf2NonRealtime(const std::string&);
    bool buildInstrumentNonRealtime(const std::filesystem::path&,
                                    std::unique_ptr<Synth>&,std::string&);
    void cleanStagedAndRetiredNonRealtime() noexcept;
    void drainRetiredSynthsNonRealtime() noexcept;
    bool retireSynthRealtime(std::unique_ptr<Synth>&) noexcept;
    void applyStagedInstrumentRealtime() noexcept;
    std::array<std::int16_t,2048> pcm_{};
    // VST3 noteId is not a chip address: track immutable original routing
    // across bank changes and matching overlapping NoteOn/NoteOff. Releasing
    // an already redirected note based on the NEW bank strands chip voices.
    // All slots and reference counts are preallocated, zero-heap in process.
    struct ActiveRoutedNote {
        Steinberg::int32 noteId=-1;
        std::uint8_t source=0, key=0, route=0;
        bool active=false;
    };
    std::array<ActiveRoutedNote,512> activeRoutedNotes_{};
    std::array<std::uint16_t,Synth::kChannelCount*128> routeHeldCounts_{};
    void clearRoutedNotes() noexcept {
        for(auto& n:activeRoutedNotes_)n.active=false;
        routeHeldCounts_.fill(0u);
    }
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
    std::array<std::uint8_t,16> effectiveBendRangeValid_{};
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
