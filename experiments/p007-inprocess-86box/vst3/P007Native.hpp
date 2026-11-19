#pragma once
// P007 – an actual native Windows-x64 VST3 with the ORIGINAL 86Box core
// loaded inside THIS DAW process. NO CreateProcess/86Box.exe/P006 bridge.
// Original Win95/Creative has passed local Linux-core NoteOn->PCM;
 // production Windows-VST3 still needs independent DAW acceptance.
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "P007OutputRate.hpp"
#include "P007StateCodec.hpp"
#include "P007CreativeNrpn.hpp"
#include "P007CreativeNrpnPacer.hpp"
#include "P007MidiDelivery.hpp"
#include "P007BankProgramPacer.hpp"
#include "p165/PresetCatalog.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "public.sdk/source/vst/vstparameters.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <atomic>
#include <string>
#include <thread>
#include <mutex>
namespace p007 {
inline constexpr Steinberg::Vst::ParamID kP165Program=1000u;
inline constexpr Steinberg::Vst::ParamID kP165Master=1001u;
inline constexpr Steinberg::Vst::ParamID kP165Bank=1002u;
inline constexpr Steinberg::Vst::ParamID kP165ModFirst=1100u;
class Core final {
    HMODULE module_=nullptr;
    bool ownsInstance_=false;
    std::thread emulatorThread_{};
    std::atomic<bool> stop_{false};
    // GUI queues an SF2 switch; only the existing 86Box worker thread
    // stops/starts the original Win95 guest. NEVER reboot in process().
    std::mutex reloadMutex_{};
    std::wstring requestedSf2_{};
    std::atomic<bool> reloadPending_{false};
    std::atomic<bool> awaitingSf2Ack_{false}; // no stale ROM/SF2 READY during hot swap
    std::atomic<bool> liveTransferInFlight_{false}; // old ACK cannot approve newly queued SF2
    std::uint32_t expectedLiveComplete_=0u; // original 86Box worker only; immutable per transfer
    void runPrivateGuestSession(std::wstring moduleFolder,std::wstring selectedSf2) noexcept;
    std::atomic<int> bootStatus_{0};
    std::atomic<int> guestReady_{0};
    std::atomic<int> sf2Result_{0};
    void runPrivateGuest(std::wstring moduleFolder,std::wstring selectedSf2) noexcept;
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
    using GuestReady=int (__cdecl*)();
    using GuestSf2Status=int (__cdecl*)();
    using LivePublish=int (__cdecl*)(const void*,std::uint32_t);
    using LiveState=int (__cdecl*)();
    using LiveCount=std::uint32_t (__cdecl*)();
    Pull pull=nullptr;
    Trim trimBacklog=nullptr;
    Midi midi=nullptr;
    Enabled midiEnabled=nullptr;
    InitProfile initProfile=nullptr;
    Boot boot=nullptr;
    Step step=nullptr;
    GuestClose guestClose=nullptr;
    BootAssist bootAssist=nullptr;
    GuestReady guestReadyFn=nullptr;
    GuestSf2Status guestSf2StatusFn=nullptr;
    LivePublish livePublish=nullptr;
    LiveState liveState=nullptr;
    LiveCount livePublishCount=nullptr;
    LiveCount liveCompleteCount=nullptr;
    LiveCount liveFailCount=nullptr;
    Core()=default;
    Core(Core const&)=delete;
    Core& operator=(Core const&)=delete;
    ~Core(){closeNonRealtime();}
    bool openNonRealtime(const std::wstring& selectedSf2=L"") noexcept;
    void closeNonRealtime() noexcept;
    bool requestSoundFontReload(const std::wstring& path) noexcept;
    [[nodiscard]] bool present() const noexcept{return module_!=nullptr;}
    [[nodiscard]] int bootStatus() const noexcept{return bootStatus_.load(std::memory_order_acquire);}
    // On live SF2 replacement the previous Win95 guest can remain READY
    // for a few worker ticks. Do not publish its *old* Creative ACK or
    // accept MIDI as if it belonged to the newly selected SoundFont.
    [[nodiscard]] bool guestReady() const noexcept {
        return !reloadPending_.load(std::memory_order_acquire) &&
               !awaitingSf2Ack_.load(std::memory_order_acquire) &&
               guestReady_.load(std::memory_order_acquire)!=0;
    }
    [[nodiscard]] bool soundFontReloadPending() const noexcept {
        return reloadPending_.load(std::memory_order_acquire) ||
               awaitingSf2Ack_.load(std::memory_order_acquire);
    }
    [[nodiscard]] int guestSf2Status() const noexcept {
        return soundFontReloadPending()?0:
               sf2Result_.load(std::memory_order_acquire);
    }
};
class Plugin final : public Steinberg::Vst::SingleComponentEffect,
                     public VSTGUI::VST3EditorDelegate {
    Core core_{};
    AWE125::PresetCatalog originalMidiCatalog_{};
    AWE125::PresetCatalog legacyFmCatalog_{};
    OutputRate outputRate_{};
    bool active_=false;
    std::atomic<std::uint8_t> midiProgram_{0u},midiBank_{0u};
    std::atomic<float> masterVolume_{1.f};
    std::array<std::atomic<std::uint16_t>,creative_nrpn::count> creativeMod_{};
    std::atomic<std::uint32_t> creativeModPending_{0u};
    std::atomic<bool> creativeResetRequested_{false}; // CC121 restores SoundFont defaults on original Win95 driver
    bool creativeGuestWasReady_=false; // audio thread only
    creative_nrpn::Pacer creativePacer_{}; // one CC packet per audio callback
    creative_nrpn::FramePace creativeFramePace_{}; // at least 20ms between NRPN short MIDI packets
    std::atomic<bool> programNeedsSending_{true};
    bool sentInitialProgram_=false;
    BankProgramPacer bankProgramPacer_{}; // P007AZ original Creative bank/patch pacing
    creative_nrpn::FramePace bankFramePace_{}; // >=20ms per short MIDI message
    MidiDelivery midiDelivery_{}; // audio-thread only; retries rejected UART note packets
    mutable std::mutex sf2StateMutex_{}; // non-RT state/UI lifecycle only
    std::string selectedSf2Path_{}; // UTF-8; selected for the NEXT activation
    std::string activeSf2Path_{}; // user selection at the last successful activation
    bool confirmedSf2Catalog_=false; // UI thread only; never fake selected presets
    std::atomic<bool> sf2NoMelodicBank_{false}; // real SF2 may be drum-only; do not claim empty user bank
    std::atomic<bool> sf2CatalogResetPending_{false}; // host-state restore -> UI thread only
    std::atomic<std::uint64_t> sf2CatalogRevision_{1u};
    [[nodiscard]] int p165GuestStatus() const noexcept;
    [[nodiscard]] std::string selectedSf2Path() const;
    bool chooseOriginalSf2(const std::string& path);
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new Plugin());
    }
    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown*) override;
    Steinberg::tresult PLUGIN_API terminate() override;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32) override;
    Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup&) override;
    Steinberg::tresult PLUGIN_API setProcessing(Steinberg::TBool) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData&) override;
    Steinberg::tresult PLUGIN_API setParamNormalized(
        Steinberg::Vst::ParamID,Steinberg::Vst::ParamValue) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream*) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream*) override;
    VSTGUI::CView* createCustomView(VSTGUI::UTF8StringPtr,
        const VSTGUI::UIAttributes&,const VSTGUI::IUIDescription*,
        VSTGUI::VST3Editor*) override;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString) override;
};
} // namespace p007
