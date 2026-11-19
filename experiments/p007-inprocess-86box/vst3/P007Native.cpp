#include "P007Native.hpp"
#include "P007HostMidiOffset.hpp"
#include "P007PrivatePaths.hpp"
#include "P007Sf2FatVolume.hpp"
#include "P007Sf2GuestProfile.hpp"
#include "p165/PresetBrowserView.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/base/ibstream.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include <cstring>
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <cwchar>
#include <vector>
namespace p007 {
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
// Address within THIS plugin module, not Studio Pro executable.
static unsigned char anchor=0;
// 86Box uses process-wide globals; do not allow two plugin instances to own it.
static std::atomic<bool> s_originalCoreOwner{false};
std::uint8_t midiVelocity(float x) noexcept {
    if(!std::isfinite(x))return 0u;
    return static_cast<std::uint8_t>(
        std::clamp(x,0.f,1.f)*127.f+0.5f);
}
} // namespace
bool Core::openNonRealtime(const std::wstring& selectedSf2) noexcept {
    if(module_)return true;
    HMODULE own=nullptr;
    if(!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&anchor),&own))return false;
    wchar_t path[2048]{};
    const DWORD n=GetModuleFileNameW(own,path,2048u);
    if(!n||n>=2048u)return false;
    wchar_t* slash=wcsrchr(path,L'\\');
    if(!slash)return false;
    slash[1]=L'\0';
    if(wcscat_s(path,2048u,L"125A_AWE_Core.dll")!=0)return false;
    // Explicit local module path; NEVER search or spawn 86Box.exe.
    module_=LoadLibraryExW(path,nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!module_)return false;
    pull=reinterpret_cast<Pull>(GetProcAddress(module_,"p007_audio_pull"));
    trimBacklog=reinterpret_cast<Trim>(GetProcAddress(module_,"p007_audio_trim_backlog"));
    midi=reinterpret_cast<Midi>(GetProcAddress(module_,"p007_midi_submit"));
    midiEnabled=reinterpret_cast<Enabled>(GetProcAddress(module_,"p007_midi_uart_enable"));
    if(!pull||!trimBacklog||!midi||!midiEnabled) {
        closeNonRealtime();
        return false;
    }
    initProfile=reinterpret_cast<InitProfile>(GetProcAddress(module_,"p007_core_init_profile"));
    boot=reinterpret_cast<Boot>(GetProcAddress(module_,"p007_core_guest_boot"));
    step=reinterpret_cast<Step>(GetProcAddress(module_,"p007_core_guest_step"));
    guestClose=reinterpret_cast<GuestClose>(GetProcAddress(module_,"p007_core_guest_close"));
    bootAssist=reinterpret_cast<BootAssist>(GetProcAddress(module_,"p007_core_guest_boot_assist"));
    guestReadyFn=reinterpret_cast<GuestReady>(GetProcAddress(module_,"p007_midi_guest_ready"));
    guestSf2StatusFn=reinterpret_cast<GuestSf2Status>(GetProcAddress(module_,"p007_midi_guest_sf2_status"));
    if(!initProfile||!boot||!step||!guestClose||!bootAssist||!guestReadyFn||!guestSf2StatusFn){
        closeNonRealtime();return false;
    }
    bool expected=false;
    if(!s_originalCoreOwner.compare_exchange_strong(expected,true)){
        closeNonRealtime();return false;
    }
    ownsInstance_=true;
    bootStatus_.store(0,std::memory_order_release);
    guestReady_.store(0,std::memory_order_release);
    sf2Result_.store(0,std::memory_order_release);
    // Neither a BIOS reset nor the start of Win95 proves Creative WinMM ready.
    // Leave MIDI disabled until a *real guest-ready acknowledgment* exists.
    midiEnabled(0);
    try{
        const std::wstring folder(path);
        stop_.store(false,std::memory_order_release);
        emulatorThread_=std::thread([this,folder,selectedSf2](){runPrivateGuest(folder,selectedSf2);});
    }catch(...){
        closeNonRealtime();return false;
    }
    return true;
}
void Core::closeNonRealtime() noexcept {
    // The 86Box fork has process-global MIDI state. A SECOND VST3 instance
    // that fails to acquire s_originalCoreOwner must NOT disable the
    // existing owner's Creative UART while releasing its DLL handle.
    if(ownsInstance_ && midiEnabled)midiEnabled(0);
    stop_.store(true,std::memory_order_release);
    if(emulatorThread_.joinable())emulatorThread_.join();
    pull=nullptr;trimBacklog=nullptr;midi=nullptr;midiEnabled=nullptr;
    initProfile=nullptr;boot=nullptr;step=nullptr;guestClose=nullptr;
    bootAssist=nullptr;guestReadyFn=nullptr;guestSf2StatusFn=nullptr;
    guestReady_.store(0,std::memory_order_release);
    sf2Result_.store(0,std::memory_order_release);
    if(module_){FreeLibrary(module_);module_=nullptr;}
    if(ownsInstance_){
        ownsInstance_=false;
        s_originalCoreOwner.store(false,std::memory_order_release);
    }
}
void Core::runPrivateGuest(std::wstring folder,std::wstring selectedSf2) noexcept {
    namespace fs=std::filesystem;
    bool guestBooted=false;
    try{
        // All resources are private and must be bundled in the one VST3.
        // No executable, external VM, OS device, BAT or named pipe.
        const fs::path root=privatevm::bundleRoot(fs::path(folder));
        wchar_t local[4096]{};
        const DWORD len=GetEnvironmentVariableW(L"LOCALAPPDATA",local,4096u);
        if(!len||len>=4096u){
            bootStatus_.store(-5,std::memory_order_release);return;
        }
        // Guest disk is writable ONLY in per-user profile. Named Windows
        // mutex prevents two DAW processes from writing the same guest HDD.
        struct MutexLease {
            HANDLE handle=nullptr;
            bool acquired=false;
            ~MutexLease(){
                if(acquired)ReleaseMutex(handle);
                if(handle)CloseHandle(handle);
            }
        } lease;
        lease.handle=CreateMutexW(nullptr,FALSE,
            L"Local\\125A_AWE64_Gold_P007_PrivateVM");
        if(!lease.handle){
            bootStatus_.store(-6,std::memory_order_release);return;
        }
        const DWORD wait=WaitForSingleObject(lease.handle,0);
        if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){
            bootStatus_.store(-7,std::memory_order_release);return;
        }
        lease.acquired=true;
        const fs::path work=fs::path(local)/L"125A"/L"AWE64Gold"/L"PrivateVM";
        std::error_code ec;
        if(!privatevm::prepare(root,work,ec)){
            bootStatus_.store(-8,std::memory_order_release);return;
        }
        const fs::path vm=work/L"vm";
        std::string setupError;
        // Both operations run BEFORE original pc_init opens either disk,
        // strictly on the non-RT private guest thread under the VM mutex.
        // Never patch the licensed Win95 system disk: the user's SoundFont
        // is mounted via a separate generated FAT16 IDE data image.
        if(!selectedSf2.empty()&&!sf2_volume::build(
                vm/L"p007sf2.img",fs::path(selectedSf2),setupError)) {
            bootStatus_.store(-10,std::memory_order_release);return;
        }
        if(!sf2_profile::configure(root/L"vm"/L"P007_AWE64.cfg",
                vm/L"P007_AWE64.cfg",!selectedSf2.empty(),setupError)) {
            bootStatus_.store(-11,std::memory_order_release);return;
        }
        const fs::path roms=root/L"roms";
        const fs::path assets=root/L"assets";
        if(!fs::is_directory(assets,ec)){
            bootStatus_.store(-9,std::memory_order_release);return;
        }
        const fs::path global=work/L"86box.cfg";
        // Original 86Box is an ANSI-path C API; this initial research
        // version supports the paths which its native Win32 CRT can open.
        const auto a=vm.string(),b=roms.string(),g=global.string();
        if(stop_.load(std::memory_order_acquire))return;
        if(initProfile(a.c_str(),b.c_str(),"P007_AWE64.cfg",g.c_str())!=1){
            bootStatus_.store(-1,std::memory_order_release);return;
        }
        if(stop_.load(std::memory_order_acquire))return;
        if(boot()!=1){
            bootStatus_.store(-2,std::memory_order_release);return;
        }
        guestBooted=true;
        bootStatus_.store(1,std::memory_order_release);
        auto next=std::chrono::steady_clock::now();
        std::uint32_t guestMilliseconds=0u;
        while(!stop_.load(std::memory_order_acquire)){
            if(step()!=1){
                // A failed guest step must stay a FAILED state. The prior
                // implementation overwrote -3 with 2 after guestClose(),
                // leaving the status panel falsely saying the guest ended.
                guestReady_.store(0,std::memory_order_release);
                if(midiEnabled)midiEnabled(0);
                bootStatus_.store(-3,std::memory_order_release);
                guestClose();
                return;
            }
            // Only original emulated keyboard controller receives Win95 boot
            // keys; no real host input events, no audio-thread work.
            (void)bootAssist(++guestMilliseconds);
            if(guestSf2StatusFn)
                sf2Result_.store(guestSf2StatusFn(),std::memory_order_release);
            if(guestReadyFn)
                guestReady_.store(guestReadyFn()?1:0,std::memory_order_release);
            next+=std::chrono::milliseconds(1);
            // No guest CPU, disk, timers, or sleep on the DAW audio callback.
            std::this_thread::sleep_until(next);
        }
        guestReady_.store(0,std::memory_order_release);
        if(midiEnabled)midiEnabled(0);
        guestClose();
        bootStatus_.store(2,std::memory_order_release);
    }catch(...){
        guestReady_.store(0,std::memory_order_release);
        if(midiEnabled)midiEnabled(0);
        // Avoid leaking a partially active original guest when a C++
        // filesystem/worker exception is raised after successful boot.
        if(guestBooted && guestClose) {
            try { guestClose(); } catch(...) {}
        }
        bootStatus_.store(-4,std::memory_order_release);
    }
}
tresult PLUGIN_API Plugin::initialize(FUnknown* context) {
    if(SingleComponentEffect::initialize(context)!=kResultOk)
        return kResultFalse;
    addEventInput(STR16("Original Creative WinMM MIDI"),16);
    addAudioOutput(STR16("Original EMU8000 Stereo"),SpeakerArr::kStereo);
    // These P165 controls have REAL paths in the original Creative guest:
    // BankSelect(CC0+CC32)/ProgramChange sent only by the AUDIO consumer;
    // master output is applied after the untouched original hardware PCM.
    // No dummy envelopes/FX sliders until original Win95 control exists.
    parameters.addParameter(new RangeParameter(STR16("Creative MIDI Program"),
        kP165Program,nullptr,0.0,127.0,0.0,127));
    parameters.addParameter(new RangeParameter(STR16("Creative MIDI Bank"),
        kP165Bank,nullptr,0.0,127.0,0.0,127));
    parameters.addParameter(new RangeParameter(STR16("Master Volume"),
        kP165Master,STR16("%"),0.0,100.0,100.0));
    // ORIGINAL Creative Win95 NRPN controls, preserving the old P165 IDs.
    parameters.addParameter(new RangeParameter(STR16("AMP Attack"),1100u,STR16("%"),0.,100.,0.));
    parameters.addParameter(new RangeParameter(STR16("AMP Decay"),1101u,STR16("%"),0.,100.,0.));
    parameters.addParameter(new RangeParameter(STR16("AMP Sustain"),1102u,STR16("%"),0.,100.,0.));
    parameters.addParameter(new RangeParameter(STR16("AMP Release"),1103u,STR16("%"),0.,100.,0.));
    parameters.addParameter(new RangeParameter(STR16("Filter Env Attack"),1104u,STR16("%"),0.,100.,0.));
    parameters.addParameter(new RangeParameter(STR16("Filter Env Decay"),1105u,STR16("%"),0.,100.,0.));
    parameters.addParameter(new RangeParameter(STR16("Filter Env Sustain"),1106u,STR16("%"),0.,100.,0.));
    parameters.addParameter(new RangeParameter(STR16("Filter Env Release"),1107u,STR16("%"),0.,100.,0.));
    parameters.addParameter(new RangeParameter(STR16("Filter Cutoff"),1108u,STR16("%"),0.,100.,0.));
    parameters.addParameter(new RangeParameter(STR16("Filter Resonance"),1109u,STR16("%"),0.,100.,0.));
    parameters.addParameter(new RangeParameter(STR16("Reverb Send"),1110u,STR16("%"),0.,100.,0.));
    parameters.addParameter(new RangeParameter(STR16("Chorus Send"),1111u,STR16("%"),0.,100.,0.));
    // The original P165 editor requires a catalog to show actual program
    // numbers. A text name is NOT an assertion that the Creative guest
    // installed a SoundFont or a program with that name.
    originalMidiCatalog_.SetOriginalCreativeMidiNumbers();
    return kResultOk;
}
tresult PLUGIN_API Plugin::terminate() {
    if(active_)(void)setActive(false);
    return SingleComponentEffect::terminate();
}
tresult PLUGIN_API Plugin::setActive(TBool on) {
    if(on) {
        if(active_)return kResultOk;
        // SDK activation is non-RT. Host cannot activate if internal core
        // is missing from the SAME .vst3 folder.
        // VST3 activation is non-realtime. Selected SF2 files are not
        // touched by process(): only the private guest worker prepares
        // a separate 64-MiB IDE volume before original Win95 boots.
        std::string fontPath=selectedSf2Path();
        std::wstring winPath;
        try {
            if(!fontPath.empty())winPath=std::filesystem::u8path(fontPath).wstring();
        } catch(...) {return kResultFalse;}
        if(!core_.openNonRealtime(winPath))return kResultFalse;
        {
            std::lock_guard<std::mutex> lock(sf2StateMutex_);
            activeSf2Path_=std::move(fontPath);
            confirmedSf2Catalog_=false;
        }
        outputRate_.reset();
        sentInitialProgram_=false;
        programNeedsSending_.store(true,std::memory_order_release);
        creativePacer_.reset();
        creativeFramePace_.reset();
        creativeGuestWasReady_=false;
        midiDelivery_.reset();
        creativeResetRequested_.store(false,std::memory_order_release);
        for(unsigned i=0;i<creative_nrpn::count;++i)
            if(creative_nrpn::shouldTransmit(
                i,creativeMod_[i].load(std::memory_order_acquire)))
                creativeModPending_.fetch_or(1u<<i,std::memory_order_acq_rel);
        active_=true;
    } else if(active_) {
        core_.closeNonRealtime();active_=false;
        outputRate_.reset();
        sentInitialProgram_=false;
        creativePacer_.reset();
        creativeFramePace_.reset();
        creativeGuestWasReady_=false;
        midiDelivery_.reset();
    }
    return SingleComponentEffect::setActive(on);
}
tresult PLUGIN_API Plugin::canProcessSampleSize(int32 size) {
    return size==kSample32?kResultTrue:kResultFalse;
}
tresult PLUGIN_API Plugin::setupProcessing(ProcessSetup& setup) {
    // Original 86Box WT_FREQ is EXACTLY 44100 Hz. The output adapter
    // converts this stream to the DAW rate without modifying the chip DSP.
    // setupProcessing is a non-audio-thread lifecycle call.
    // Some SDK test hosts initialize with a zero/sentinel sample rate.
    // Preserve Steinberg's original lifecycle return semantics. Our
    // adapter already clamps unsupported/invalid values to native 44.1 kHz.
    const auto result=SingleComponentEffect::setupProcessing(setup);
    if(result==kResultOk){
        outputRate_.configure(setup.sampleRate);
        creativeFramePace_.configure(setup.sampleRate);
    }
    return result;
}
// VST3 hosts (including Studio Pro) call setProcessing() on transport
// and processor lifecycle transitions. The Steinberg base-class default
// does not acknowledge this in our combined-instrument implementation.
// Keep the original guest alive across host transport stop/start: its
// Win95 boot must NOT be restarted on each audio processing transition.
tresult PLUGIN_API Plugin::setProcessing(TBool on) {
    // Do not reuse stale resampler history when the DAW resumes. This is
    // constant-time, heap-free and leaves the original EMU8000 untouched.
    if(on) outputRate_.reset();
    return kResultOk;
}
std::string Plugin::selectedSf2Path() const {
    std::lock_guard<std::mutex> lock(sf2StateMutex_);
    return selectedSf2Path_;
}
bool Plugin::chooseOriginalSf2(const std::string& path) {
    // Runs on the VSTGUI thread, never on the audio callback.
    try{
        const auto file=std::filesystem::u8path(path);
        std::error_code ec;
        if(path.empty()||path.size()>state::maxSf2PathBytes ||
           !std::filesystem::is_regular_file(file,ec) ||
           std::filesystem::file_size(file,ec)>sf2_volume::maxSf2Bytes ||
           ec) return false;
        AWE125::PresetCatalog validated;
        std::string error;
        if(!validated.LoadSoundFont(path,error))return false;
        std::lock_guard<std::mutex> lock(sf2StateMutex_);
        selectedSf2Path_=path;
        confirmedSf2Catalog_=false;
        // Do not display the PREVIOUS bank's SF2 preset names while the
        // new bank is only queued and has not been loaded by Creative.
        originalMidiCatalog_.SetOriginalCreativeMidiNumbers();
        sf2CatalogRevision_.fetch_add(1u,std::memory_order_release);
        return true;
    }catch(...){return false;}
}
int Plugin::p165GuestStatus() const noexcept {
    const int stage=core_.bootStatus();
    if(stage<0)return stage;
    if(core_.guestReady())return 2;
    if(stage==1)return 1;
    if(stage==2)return 3;
    return 0;
}
VSTGUI::CView* Plugin::createCustomView(VSTGUI::UTF8StringPtr name,
    const VSTGUI::UIAttributes&,const VSTGUI::IUIDescription*,
    VSTGUI::VST3Editor*) {
    if(name&&std::strcmp(name,"P007P165Panel")==0)
        return new AWE125::PresetBrowserView({0,0,1000,708},this,
            originalMidiCatalog_,legacyFmCatalog_,std::string{},
            [this]() {
                const int s=p165GuestStatus();
                std::string selected,active;
                {
                    std::lock_guard<std::mutex> lock(sf2StateMutex_);
                    selected=selectedSf2Path_;active=activeSf2Path_;
                }
                if(!selected.empty()){
                    if(selected!=active)
                        return std::string("SF2 AUSGEWAEHLT | BEIM NAECHSTEN PLUGIN-START LADEN");
                    const int sf2=core_.guestSf2Status();
                    if(sf2==1 && s==2)
                        return std::string("ORIGINAL CREATIVE: SF2-BANK 1 PER AWEMAN32 GELADEN");
                    if(sf2<0)
                        return std::string("FEHLER: ORIGINAL CREATIVE HAT SF2 ABGELEHNT");
                    if(s==2)
                        return std::string("SF2 NICHT BESTAETIGT | ORIGINAL ROM AKTIV");
                }
                return s==2 ? std::string("ORIGINAL CREATIVE WINMM: ROM BEREIT") :
                       s==1 ? std::string("WINDOWS 95: ORIGINAL-GAST STARTET") :
                       s<0 ? std::string("WINDOWS 95 / CREATIVE: STARTFEHLER") :
                             std::string("ORIGINAL CREATIVE: GAST NOCH NICHT BEREIT");
            },
            [this]() {
                const int s=p165GuestStatus();
                return s==2 ? std::string("ORIGINAL AWE64 | CREATIVE WINMM BEREIT") :
                       s==1 ? std::string("WIN95: BOOTET") :
                       s<0 ? std::string("WIN95: GASTFEHLER") :
                             std::string("ROM IM BUNDLE | WIN95 NICHT BEREIT");
            },
            [](const std::string&){return false;}, // original ROM immutable
            [this](const std::string& path){return chooseOriginalSf2(path);},
            [this](){return selectedSf2Path();},
            [this]() {
                if(core_.guestReady() && core_.guestSf2Status()==1) {
                    std::string selected;
                    {
                        std::lock_guard<std::mutex> lock(sf2StateMutex_);
                        if(confirmedSf2Catalog_ || selectedSf2Path_!=activeSf2Path_)
                            return sf2CatalogRevision_.load(std::memory_order_acquire);
                        selected=selectedSf2Path_;
                    }
                    AWE125::PresetCatalog candidate;
                    std::string error;
                    if(candidate.LoadSoundFont(selected,error)) {
                        std::lock_guard<std::mutex> lock(sf2StateMutex_);
                        if(!confirmedSf2Catalog_ && selectedSf2Path_==selected){
                            originalMidiCatalog_=std::move(candidate);
                            confirmedSf2Catalog_=true;
                            sf2CatalogRevision_.fetch_add(1u,std::memory_order_release);
                        }
                    }
                }
                return sf2CatalogRevision_.load(std::memory_order_acquire);
            },
            [](std::uint16_t bank){return bank<=127u;});
    return nullptr;
}
IPlugView* PLUGIN_API Plugin::createView(FIDString name) {
    if(name&&std::strcmp(name,ViewType::kEditor)==0) {
        // The pinned original P165 view description has NO keyboard.
        auto* editor=new VSTGUI::VST3Editor(this,"view","p007_p165.uidesc");
        editor->setDelegate(this);
        return editor;
    }
    return nullptr;
}
// P007-V DAW project state. Small, versioned, checked data (bank, program,
// post-EMU8000 master) only; never embed the private Win95 HDD or ROMs.
tresult PLUGIN_API Plugin::getState(IBStream* stream) {
    if(!stream)return kInvalidArgument;
    state::Values values{
        midiBank_.load(std::memory_order_acquire),
        midiProgram_.load(std::memory_order_acquire),
        state::masterToBasisPoints(masterVolume_.load(std::memory_order_acquire))
    };
    for(unsigned i=0;i<creative_nrpn::count;++i)
        values.modulation[i]=creativeMod_[i].load(std::memory_order_acquire);
    const auto path=selectedSf2Path();
    const auto v2=state::encode(values);
    std::vector<std::uint8_t> bytes;
    if(path.empty()) bytes.assign(v2.begin(),v2.end());
    else bytes=state::encodeWithSf2(values,path);
    if(bytes.empty())return kResultFalse;
    int32 written=0;
    const auto result=stream->write(const_cast<std::uint8_t*>(bytes.data()),
            static_cast<int32>(bytes.size()),&written);
    return result==kResultOk&&written==static_cast<int32>(bytes.size())
        ?kResultOk:kResultFalse;
}
tresult PLUGIN_API Plugin::setState(IBStream* stream) {
    if(!stream)return kInvalidArgument;
    state::LegacyImage header{};
    int32 received=0;
    if(stream->read(header.data(),static_cast<int32>(header.size()),
                    &received)!=kResultOk ||
       received!=static_cast<int32>(header.size()))return kResultFalse;
    state::Values values{};
    std::string selectedPath; // empty for legacy or original ROM-only states
    if(header[4]==1u) {
        if(!state::decode(header,values))return kResultFalse;
    }else if(header[4]==2u) {
        state::Image image{};
        std::copy(header.begin(),header.end(),image.begin());
        received=0;
        constexpr int32 tail=static_cast<int32>(
                state::Image{}.size()-state::LegacyImage{}.size());
        if(stream->read(image.data()+header.size(),tail,&received)!=kResultOk ||
           received!=tail||!state::decode(image,values))return kResultFalse;
    }else if(header[4]==3u) {
        // Read the bounded variable-length UTF-8 path with exact counts
        // and checksum BEFORE updating any live plugin parameter.
        std::array<std::uint8_t,46u> prefix{};
        std::copy(header.begin(),header.end(),prefix.begin());
        received=0;
        constexpr int32 more=46-16;
        if(stream->read(prefix.data()+16u,more,&received)!=kResultOk ||
           received!=more)return kResultFalse;
        const unsigned n=unsigned(prefix[44])+(unsigned(prefix[45])<<8u);
        if(n==0u||n>state::maxSf2PathBytes)return kResultFalse;
        try{
            std::vector<std::uint8_t> image(prefix.begin(),prefix.end());
            image.resize(50u+n);
            received=0;
            const int32 remain=static_cast<int32>(n+4u);
            if(stream->read(image.data()+46u,remain,&received)!=kResultOk ||
               received!=remain ||
               !state::decodeWithSf2(image,values,selectedPath))
                return kResultFalse;
        }catch(...){return kResultFalse;}
    }else return kResultFalse;
    {
        std::lock_guard<std::mutex> lock(sf2StateMutex_);
        selectedSf2Path_=std::move(selectedPath);
        confirmedSf2Catalog_=false;
    }
    // In a combined IComponent+IEditController, restore BOTH the original
    // Creative MIDI engine parameters and the visible P165 host automation.
    (void)setParamNormalized(kP165Bank,double(values.bank)/127.);
    (void)setParamNormalized(kP165Program,double(values.program)/127.);
    (void)setParamNormalized(kP165Master,double(values.master)/10000.);
    for(unsigned i=0;i<creative_nrpn::count;++i)
        (void)setParamNormalized(kP165ModFirst+i,
                     double(values.modulation[i])/10000.);
    programNeedsSending_.store(true,std::memory_order_release);
    return kResultOk;
}
tresult PLUGIN_API Plugin::setComponentState(IBStream* stream) {
    return setState(stream);
}
tresult PLUGIN_API Plugin::setParamNormalized(ParamID tag,ParamValue value) {
    if(!std::isfinite(value))return kInvalidArgument;
    const tresult result=SingleComponentEffect::setParamNormalized(tag,value);
    if(result!=kResultOk)return result;
    const double v=std::clamp(value,0.,1.);
    switch(tag) {
        case kP165Bank:
            midiBank_.store(static_cast<std::uint8_t>(std::lround(v*127.)),
                            std::memory_order_release);
            programNeedsSending_.store(true,std::memory_order_release);
            break;
        case kP165Program:
            midiProgram_.store(static_cast<std::uint8_t>(std::lround(v*127.)),
                               std::memory_order_release);
            programNeedsSending_.store(true,std::memory_order_release);
            break;
        case kP165Master:
            masterVolume_.store(static_cast<float>(v),std::memory_order_release);
            break;
        default:
            if(tag>=kP165ModFirst && tag<kP165ModFirst+creative_nrpn::count) {
                const unsigned index=tag-kP165ModFirst;
                const auto next=state::normalizedToBasisPoints(v);
                const auto old=creativeMod_[index].exchange(
                                      next,std::memory_order_acq_rel);
                if(creative_nrpn::needsSoundFontReset(index,old,next))
                    creativeResetRequested_.store(true,std::memory_order_release);
                creativeModPending_.fetch_or(1u<<index,std::memory_order_acq_rel);
            }
            break;
    }
    return result;
}
tresult PLUGIN_API Plugin::process(ProcessData& data) {
    // Original P165 editor -> host automation -> DAW audio thread -> same
    // bounded SPSC UART packet producer as host MIDI notes. GUI must NEVER
    // write the UART directly from its separate UI thread.
    bool programChanged=false,bankChanged=false;
    if(data.inputParameterChanges){
        const int32 n=data.inputParameterChanges->getParameterCount();
        for(int32 i=0;i<n;++i){
            auto* queue=data.inputParameterChanges->getParameterData(i);
            if(!queue||queue->getPointCount()<1)continue;
            int32 offset=0;ParamValue value=0.;
            if(queue->getPoint(queue->getPointCount()-1,offset,value)!=kResultOk)
                continue;
            // Host automation may provide NaN/INF despite valid VST3
            // parameter ranges. std::clamp(NaN) is still NaN; converting
            // it with std::lround is undefined. Ignore invalid points.
            if(!std::isfinite(value))continue;
            value=std::clamp(value,0.,1.);
            switch(queue->getParameterId()){
                case kP165Program:
                    midiProgram_=static_cast<std::uint8_t>(std::lround(value*127.));
                    programChanged=true;break;
                case kP165Bank:
                    midiBank_=static_cast<std::uint8_t>(std::lround(value*127.));
                    bankChanged=true;break;
                case kP165Master:
                    masterVolume_=static_cast<float>(value);break;
                default:
                    if(queue->getParameterId()>=kP165ModFirst &&
                       queue->getParameterId()<kP165ModFirst+creative_nrpn::count) {
                        const unsigned index=queue->getParameterId()-kP165ModFirst;
                        const auto next=state::normalizedToBasisPoints(value);
                        const auto old=creativeMod_[index].exchange(
                                                  next,std::memory_order_acq_rel);
                        if(creative_nrpn::needsSoundFontReset(index,old,next))
                            creativeResetRequested_.store(true,std::memory_order_release);
                        creativeModPending_.fetch_or(1u<<index,std::memory_order_acq_rel);
                    }
                    break;
            }
        }
    }
    const bool ready=active_&&core_.guestReady()&&core_.midi;
    if(!ready) {
        // A stopped/restarting guest must not retain resampler priming
        // frames from a previous original-Creative session.
        outputRate_.reset();
        midiDelivery_.reset();
        sentInitialProgram_=false;
        creativePacer_.abort(creativeModPending_);
        creativeGuestWasReady_=false;
    }else if(!creativeGuestWasReady_) {
        midiDelivery_.reset();
        creativePacer_.reset();
        creativeFramePace_.reset();
        // Original guest restarted: reassert modified generators and
        // always enforce 0% truly DRY for original Creative FX sends.
        // For tone controls, 0% does NOT send NRPN=0: that would replace
        // the source SoundFont cutoff/envelope with a destructive value.
        creativeGuestWasReady_=true;
        creativeResetRequested_.store(false,std::memory_order_release);
        for(unsigned i=0;i<creative_nrpn::count;++i)
            if(creative_nrpn::shouldTransmit(
                i,creativeMod_[i].load(std::memory_order_acquire)))
                creativeModPending_.fetch_or(1u<<i,std::memory_order_acq_rel);
    }
    // First retry a previously rejected NoteOff BEFORE allowing further
    // program/NRPN writes or newer host notes to overtake it. The original
    // guest UART returns 1 only after its bounded FIFO accepted a packet.
    if(ready) (void)midiDelivery_.drain(
        [this](std::uint8_t status,std::uint8_t d1,std::uint8_t d2) noexcept {
            return core_.midi(status,d1,d2);
        });
    const bool midiBlocked=ready&&
        (midiDelivery_.pending()!=0u||midiDelivery_.panicPending());
    const bool resend=programNeedsSending_.exchange(false,std::memory_order_acq_rel);
    // Preserve new host automation too: bank/program changes received
    // DURING a MIDI retry must not disappear with this one callback.
    if(midiBlocked && (resend||programChanged||bankChanged))
        programNeedsSending_.store(true,std::memory_order_release);
    bool justSentSelection=false;
    if(ready&&!midiBlocked){
        // The original 86Box UART is a bounded FIFO. A failed submit means
        // Creative did NOT receive the selected bank/program: do not falsely
        // consider it delivered. Retry the complete ordered selection on the
        // next audio callback (still the ONLY MIDI producer thread).
        const bool sendBank=!sentInitialProgram_||bankChanged||resend;
        const bool sendProgram=sendBank||programChanged;
        justSentSelection=sendProgram;
        bool accepted=true;
        if(sendBank) {
            // Original Win95 Creative driver receives real MIDI BankSelect.
            // Evaluate both calls even when the first packet was rejected.
            accepted=(core_.midi(0xb0u,0u,0u)==1)&&accepted; // CC0 bank MSB
            accepted=(core_.midi(0xb0u,32u,
                       midiBank_.load(std::memory_order_acquire))==1)&&accepted;
        }
        if(sendProgram){
            accepted=(core_.midi(0xc0u,
                       midiProgram_.load(std::memory_order_acquire),0u)==1)&&accepted;
        }
        sentInitialProgram_=accepted;
        // Program/bank changes can reset Creative generator offsets. Reapply
        // all active user offsets AND zero FX sends after valid selection.
        if(accepted&&sendProgram) {
            for(unsigned i=0;i<creative_nrpn::count;++i)
                if(creative_nrpn::shouldTransmit(
                   i,creativeMod_[i].load(std::memory_order_acquire)))
                    creativeModPending_.fetch_or(1u<<i,std::memory_order_acq_rel);
        }
    }
    if(ready&&!midiBlocked&&sentInitialProgram_&&!justSentSelection) {
        // CC121 is specified by Creative to restore the original SoundFont
        // settings. This is essential when an ADSR/filter knob moves BACK
        // to 0% (neutral). Unlike FX sends, a raw NRPN value of zero on
        // those generators would change the preset, not restore it.
        // After a reset, replay other active generator offsets and both
        // dry/wet FX sends. Never run original guest CPU on the audio thread.
        const bool reset=creativeResetRequested_.exchange(
                                false,std::memory_order_acq_rel);
        if(reset) {
            creativePacer_.abort(creativeModPending_);
            if(core_.midi(0xb0u,121u,0u)!=1)
                creativeResetRequested_.store(true,std::memory_order_release);
            else for(unsigned i=0;i<creative_nrpn::count;++i)
                if(creative_nrpn::shouldTransmit(
                       i,creativeMod_[i].load(std::memory_order_acquire)))
                    creativeModPending_.fetch_or(1u<<i,std::memory_order_acq_rel);
        }else{
            // 16550 UART is a 16-byte FIFO; 4x6-byte NRPN messages in
            // ONE callback form a 24-byte burst and can overrun it.
            // Send at most ONE 6-byte short MIDI packet per callback.
            if(creativeFramePace_.due(static_cast<std::uint32_t>(
                                        std::max<Steinberg::int32>(0,data.numSamples))))
                (void)creativePacer_.tick(creativeModPending_,creativeMod_,
                    [this](std::uint8_t controller,std::uint8_t amount) noexcept {
                        return core_.midi(0xb0u,controller,amount)==1;
                    });
        }
    }
    // All guest UART calls occur on this one DAW audio-consumer thread.
    // Original 86Box CPU and Win95 timers remain on the worker thread.
    // A VST3 event's sampleOffset means its position IN THIS HOST BLOCK.
    // The old code sent every NoteOn/Off before rendering sample 0, so
    // even an event at offset=900 was dispatched as if at offset=0.
    // Render the already-due audio first and submit that event only when
    // the host block reaches its requested offset. The original Win95
    // guest runs asynchronously, so this is HOST dispatch scheduling,
    // not claimed sample-accurate physical-EMU8000 sound generation.
    const std::uint32_t frames=data.numSamples>0
        ?static_cast<std::uint32_t>(data.numSamples):0u;
    AudioBusBuffers* output=data.numOutputs>0&&data.outputs
        ?&data.outputs[0]:nullptr;
    float* l=nullptr;
    float* r=nullptr;
    if(output&&output->numChannels>=2&&output->channelBuffers32) {
        l=output->channelBuffers32[0];
        r=output->channelBuffers32[1];
    }
    // No allocation, no lock. Called once per audio callback, before
    // segmented rendering, not between MIDI events.
    if(active_&&core_.trimBacklog&&frames!=0u)
        (void)core_.trimBacklog(8192u,2048u);
    std::uint32_t rendered=0u;
    const auto renderTo=[&](std::uint32_t end) noexcept {
        if(end<=rendered)return;
        if(l&&r) {
            outputRate_.render(ready?core_.pull:nullptr,
                l+rendered,r+rendered,end-rendered);
        }
        rendered=end;
    };
    if(ready&&data.inputEvents) {
        const auto n=data.inputEvents->getEventCount();
        for(int32 i=0;i<n;++i) {
            Event event{};
            if(data.inputEvents->getEvent(i,event)!=kResultOk)continue;
            if(event.type!=Event::kNoteOnEvent &&
               event.type!=Event::kNoteOffEvent)continue;
            const auto at=midi_offset::place(
                event.sampleOffset,data.numSamples,rendered);
            renderTo(at);
            if(event.type==Event::kNoteOnEvent) {
                const auto& e=event.noteOn;
                if(e.channel>=0&&e.channel<16&&e.pitch>=0&&e.pitch<128)
                    (void)midiDelivery_.submit(
                        static_cast<std::uint8_t>(0x90|e.channel),
                        static_cast<std::uint8_t>(e.pitch),midiVelocity(e.velocity),
                        [this](std::uint8_t status,std::uint8_t d1,
                               std::uint8_t d2) noexcept {
                            return core_.midi(status,d1,d2);
                        });
            } else {
                const auto& e=event.noteOff;
                if(e.channel>=0&&e.channel<16&&e.pitch>=0&&e.pitch<128)
                    (void)midiDelivery_.submit(
                        static_cast<std::uint8_t>(0x80|e.channel),
                        static_cast<std::uint8_t>(e.pitch),midiVelocity(e.velocity),
                        [this](std::uint8_t status,std::uint8_t d1,
                               std::uint8_t d2) noexcept {
                            return core_.midi(status,d1,d2);
                        });
            }
        }
    }
    renderTo(frames);
    if(!l||!r||!output||frames==0u)return kResultOk;
    // Host Master operates AFTER original Creative/EMU8000 mixer, not
    // inside the emulated chip. Exactly unity = unaltered original PCM.
    const float gain=masterVolume_.load(std::memory_order_acquire);
    if(gain!=1.f) {
        for(int32 i=0;i<data.numSamples;++i){
            l[i]*=gain;
            r[i]*=gain;
        }
    }
    output->silenceFlags=0;
    return kResultOk;
}
} // namespace p007
