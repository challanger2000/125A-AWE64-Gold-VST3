// Final P165 owner interface, imported from the LAST original P007BW Win95 build.
// Only the backend is adapted: this is the real native 86Box EMU8000 Synth.
// Preserve the P168 LCD x=370 and approved alpha=186 control panel literally.
#include "P009Native.hpp"
#include "P009AssetFingerprint.hpp"
#include "p165/PresetBrowserView.h"
#include "p165/PresetBrowserModel.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

namespace p009 {
using namespace Steinberg;
using namespace Steinberg::Vst;

VSTGUI::CView* NativeVst3::createCustomView(VSTGUI::UTF8StringPtr name,
    const VSTGUI::UIAttributes&,const VSTGUI::IUIDescription*,
    VSTGUI::VST3Editor*) {
    if(!name || std::strcmp(name,"P007P165Panel")!=0)return nullptr;
    return new AWE125::PresetBrowserView({0,0,1000,708},this,
        guiCatalog_,unusedFmCatalog_,std::string{},
        [this](){
            std::lock_guard<std::mutex> guard(guiAssetMutex_);
            if(selectedUserSf2_.empty())
                return std::string("ORIGINAL AWE64 ROM | USER SF2 NOT LOADED");
            if(stagedSynth_.load(std::memory_order_acquire))
                return std::string("SOUNDFONT WAITING FOR NEXT AUDIO CALLBACK");
            return std::string("ORIGINAL EMU8000 | USER SOUNDFONT READY");
        },
        [this](){
            return std::string("32 ORIGINAL EMU8000 VOICES | 44.1k CHIP | 28 MiB RAM");
        },
        [](const std::string&){return false;}, // no fake hot-swap ROM
        [this](const std::string& path){return requestUserSf2NonRealtime(path);},
        [this](){
            std::lock_guard<std::mutex> guard(guiAssetMutex_);
            return selectedUserSf2_;
        },
        [this](){
            // Reclaim the *old* chip's 28 MiB heap outside the audio thread.
            delete retiredSynth_.exchange(nullptr,std::memory_order_acq_rel);
            return guiCatalogRevision_.load(std::memory_order_acquire);
        },
        [](std::uint16_t bank){return bank<=2u||bank==128u;});
}
IPlugView* PLUGIN_API NativeVst3::createView(FIDString name) {
    if(name && std::strcmp(name,ViewType::kEditor)==0){
        auto* editor=new VSTGUI::VST3Editor(this,"view","p007_p165.uidesc");
        editor->setDelegate(this);
        return editor;
    }
    return nullptr;
}
tresult PLUGIN_API NativeVst3::setParamNormalized(ParamID id,ParamValue value){
    // Only update host parameter objects on the UI thread. ProcessData
    // receives host-owned points at exact offsets and changes audio state.
    if(!std::isfinite(value)||value<0.||value>1.)return kInvalidArgument;
    return SingleComponentEffect::setParamNormalized(id,value);
}
bool NativeVst3::requestUserSf2NonRealtime(const std::string& path) {
    if(path.empty()||path.size()>4096)return false;
    // A candidate must be a *real* complete SoundFont with valid 'phdr':
    // never fabricate 128 presets from an owner's 77-entry sparse bank.
    std::string err;
    AWE125::PresetCatalog parsed;
    if(!parsed.LoadSoundFont(path,err))return false;
    AWE125::PresetCatalog nextCatalog;
    nextCatalog.SetOriginalCreativeMidiNumbers();
    if(nextCatalog.AppendCreativeMelodicUserOneFromSource(parsed)==0)
        return false;
    // A second complete original-chip/SBK/selected SF2 is built offline,
    // including 28 MiB RAM upload, preallocated 64-frame source FIFO and FX.
    // Neither audio render thread nor the old Synth is touched.
    const auto selected=std::filesystem::u8path(path);
    std::array<std::uint8_t,32> fingerprint{};
    if(!sha256FileNonRealtime(selected,fingerprint))return false;
    std::unique_ptr<Synth> ready;
    if(!buildInstrumentNonRealtime(selected,ready,err))return false;
    // Staging queue is single-slot; superseded *unconsumed* candidate can
    // be deleted here on the GUI thread, NEVER in process().
    delete retiredSynth_.exchange(nullptr,std::memory_order_acq_rel);
    if(auto* prior=stagedSynth_.exchange(ready.release(),
                                        std::memory_order_acq_rel))
        delete prior;
    // The browser catalog and its revision are written on this UI thread;
    // one event-loop idle cannot observe half-parsed asset metadata.
    guiCatalog_=std::move(nextCatalog);
    { std::lock_guard<std::mutex> guard(guiAssetMutex_);
      selectedUserSf2_=path;
      selectedUserSha256_=fingerprint;
      userChoseSf2_=true;
    }
    guiCatalogRevision_.fetch_add(1u,std::memory_order_release);
    return true;
}

} // namespace p009
