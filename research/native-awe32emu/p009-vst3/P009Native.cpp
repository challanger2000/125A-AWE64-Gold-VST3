#include "P009Native.hpp"
#include "Emu8000Box.h"
#include "P009CreativeDefaults.hpp"
#include "P009AssetFingerprint.hpp"
#include "P009CreativeFx.hpp"
#include "Synth.h"
#include "Awe32Driver.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "pluginterfaces/base/ibstream.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#include <vector>
namespace p009 {
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
constexpr ParamID kProgram=1000u,kVolume=1001u,kBank=1002u;
constexpr ParamID kChProgramBase=1200u,kChBankBase=1300u;
constexpr ParamID kMidiMappedBase=2000u;
constexpr unsigned kMappedCount=8u;
constexpr unsigned kAdvancedCount=8u;
constexpr ParamID kAdvancedMappedBase=2400u;
constexpr std::array<CtrlNumber,kAdvancedCount> kAdvancedControllers{
    kCtrlBankSelectMSB,kCtrlBankSelectLSB,
    kCtrlRPNSelectMSB,kCtrlRPNSelectLSB,
    kCtrlDataEntryMSB,kCtrlDataEntryLSB,
    kCtrlNRPNSelectMSB,kCtrlNRPNSelectLSB
};
constexpr std::array<const char*,kAdvancedCount> kAdvancedNames{
    "Bank MSB","Bank LSB","RPN MSB","RPN LSB",
    "Data MSB","Data LSB","NRPN MSB","NRPN LSB"
};
constexpr std::array<CtrlNumber,kMappedCount> kMappedControllers{
    kCtrlModWheel,kCtrlVolume,kCtrlPan,kCtrlExpression,kCtrlSustainOnOff,
    kCtrlEff1Depth,kCtrlEff3Depth,kPitchBend
};
constexpr std::array<const char*,kMappedCount> kMappedNames{
    "Mod", "Volume", "Pan", "Expression", "Sustain", "Reverb",
    "Chorus", "Pitch Bend"
};
constexpr std::array<unsigned,kMappedCount> kMappedDefaults{
    0,100,64,127,0,0,0,8192
};
struct Saved {
    std::uint32_t magic=0x39303050u,version=4u;
    std::uint32_t program=0u,bank=0u;
    float master=1.f;
    std::array<std::uint8_t,15> otherPrograms{};
    std::array<std::uint8_t,15> otherBanks{};
    std::array<std::uint8_t,2> reserved{};
    std::array<std::uint16_t,16*8> midiStates{};
    std::array<std::uint16_t,16*8> advancedStates{};
    std::array<std::uint8_t,16> parameterModes{};
};
static_assert(sizeof(Saved)==580u,"P013 exact 580-byte VST3 state");
// State V5 appends a checked owner SoundFont path and full SHA256 digest.
// The preceding 580 bytes are byte-identical to V4 except the version tag.
// Legacy v1/v2/v3/v4 project formats MUST continue to roundtrip byte-exact.
struct OwnerStateV5 {
    std::uint32_t marker=0x35534150u; // "PAS5" little endian
    std::uint32_t pathBytes=0u;
    std::array<std::uint16_t,12> guiControls{};
    std::array<std::uint8_t,32> assetSha256{};
    std::array<std::uint16_t,2> effectModes{{4u,2u}};
};
static_assert(sizeof(OwnerStateV5)==68u,"exact V5 owner-bank and FX-type header");
constexpr std::uint32_t kStateV1=1u,kStateV2=2u,kStateV3=3u,kStateV4=4u,kStateV5=5u;
constexpr int32 kV1Bytes=20,kV2Bytes=52,kV3Bytes=308;
struct MidiPoint {
    int32 sampleOffset=0;
    std::uint8_t channel=0,slot=0;
    std::uint16_t value=0;
    // Master is continuous and must not be forced through a 7-bit MIDI
    // value or 16-bit quantizer; retain the exact host float at its offset.
    float normalized=0.f;
};
bool finite01(double x){return std::isfinite(x) && x>=0. && x<=1.;}
unsigned toByte(float v) {
    return static_cast<unsigned>(std::clamp(
        static_cast<double>(v),0.,1.)*127.+0.5);
}
std::filesystem::path bundleResourcePath(){
    // Locate THIS plugin module, never the DAW executable.
    HMODULE own=nullptr;
    static unsigned char selfAddress=0;
    if(!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&selfAddress),&own))return {};
    wchar_t name[4096]{};
    auto n=GetModuleFileNameW(own,name,4096);
    if(!n||n>=4096)return {};
    // <bundle>.vst3/Contents/x86_64-win/<binary>.vst3
    const std::filesystem::path modulePath{name};
    return modulePath.parent_path().parent_path()/"Resources"/"P009-Private";
}
} // namespace
tresult PLUGIN_API NativeVst3::queryInterface(const TUID id,void** obj){
    if(!obj)return kInvalidArgument;
    if(FUnknownPrivate::iidEqual(id,IMidiMapping::iid.toTUID())){
        *obj=static_cast<IMidiMapping*>(this);
        SingleComponentEffect::addRef();
        return kResultOk;
    }
    return SingleComponentEffect::queryInterface(id,obj);
}
tresult PLUGIN_API NativeVst3::getMidiControllerAssignment(
    int32 bus,int16 channel,CtrlNumber controller,ParamID& id) {
    if(bus!=0||channel<0||channel>=16)return kResultFalse;
    for(unsigned slot=0;slot<kMappedCount;++slot)
        if(kMappedControllers[slot]==controller){
            id=kMidiMappedBase+static_cast<unsigned>(channel)*kMappedCount+slot;
            return kResultTrue;
        }
    for(unsigned slot=0;slot<kAdvancedCount;++slot)
        if(kAdvancedControllers[slot]==controller){
            id=kAdvancedMappedBase+static_cast<unsigned>(channel)*kAdvancedCount+slot;
            return kResultTrue;
        }
    return kResultFalse;
}
tresult PLUGIN_API NativeVst3::initialize(FUnknown* context){
    if(SingleComponentEffect::initialize(context)!=kResultOk)
        return kResultFalse;
    addEventInput(STR16("Native Creative MIDI 16 channels"),16);
    addAudioOutput(STR16("EMU8000 stereo"),SpeakerArr::kStereo);
    // Latest P007BW P165 actual hardware UI: identical host parameter IDs.
    // Zero on all 12 modulation controls is original chip neutral.
    guiCatalog_.SetOriginalCreativeMidiNumbers();
    parameters.addParameter(new RangeParameter(STR16("Program CH1"),
        kProgram,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Master"),
        kVolume,STR16("%"),0.,100.,100.));
    parameters.addParameter(new RangeParameter(STR16("Bank CH1"),
        kBank,nullptr,0.,2.,0.,2));
    const std::array<const char*,12> guiNames{{
      "AMP Attack","AMP Decay","AMP Sustain","AMP Release",
      "Filter Attack","Filter Decay","Filter Sustain","Filter Release",
      "Filter Cutoff","Filter Resonance","Reverb Send","Chorus Send"
    }};
    for(unsigned i=0;i<guiNames.size();++i) {
        TChar wide[64]{};
        for(unsigned j=0;j<63&&guiNames[i][j];++j)
            wide[j]=static_cast<TChar>(guiNames[i][j]);
        parameters.addParameter(new RangeParameter(
            wide,1100u+i,STR16("%"),0.,100.,0.));
    }
    parameters.addParameter(new RangeParameter(
        STR16("Reverb Type"),1400u,nullptr,0.,7.,4.,7));
    parameters.addParameter(new RangeParameter(
        STR16("Chorus Type"),1401u,nullptr,0.,7.,2.,7));
    parameters.addParameter(new RangeParameter(
        STR16("Reverb Send Original/Manual"),1420u,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(
        STR16("Chorus Send Original/Manual"),1421u,nullptr,0.,1.,0.,1));
    // VST3 hosts translate actual MIDI CC to these per-channel parameters
    // through IMidiMapping. This is host-standard, NOT legacy CC input events.
    for(unsigned ch=0;ch<16;++ch){
        for(unsigned slot=0;slot<kMappedCount;++slot){
            char shortName[64]{};
            std::snprintf(shortName,sizeof(shortName),"CH%u %s",
                          ch+1,kMappedNames[slot]);
            TChar vstName[64]{};
            for(unsigned j=0;j<63&&shortName[j];++j)
                vstName[j]=static_cast<TChar>(shortName[j]);
            const bool pitch=slot==kMappedCount-1;
            parameters.addParameter(new RangeParameter(
                vstName,kMidiMappedBase+ch*kMappedCount+slot,
                nullptr,0.,pitch?16383.:127.,
                static_cast<double>(kMappedDefaults[slot]),
                pitch?16383:127));
        }
    }
    for(unsigned ch=0;ch<16;++ch){
        for(unsigned slot=0;slot<kAdvancedCount;++slot){
            char ascii[64]{};
            std::snprintf(ascii,sizeof(ascii),"CH%u %s",
                          ch+1,kAdvancedNames[slot]);
            TChar name[64]{};
            for(unsigned j=0;j<63&&ascii[j];++j)
                name[j]=static_cast<TChar>(ascii[j]);
            parameters.addParameter(new RangeParameter(
                name,kAdvancedMappedBase+ch*kAdvancedCount+slot,
                nullptr,0.,127.,0.,127));
        }
    }
    // All 16 physical MIDI channels have distinct, automatable patch state.
    // CH10 remains the original Creative GM drum channel, no user selection.
    parameters.addParameter(new RangeParameter(STR16("Program CH2"),
        kChProgramBase+1,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH2"),
        kChBankBase+1,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH3"),
        kChProgramBase+2,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH3"),
        kChBankBase+2,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH4"),
        kChProgramBase+3,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH4"),
        kChBankBase+3,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH5"),
        kChProgramBase+4,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH5"),
        kChBankBase+4,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH6"),
        kChProgramBase+5,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH6"),
        kChBankBase+5,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH7"),
        kChProgramBase+6,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH7"),
        kChBankBase+6,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH8"),
        kChProgramBase+7,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH8"),
        kChBankBase+7,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH9"),
        kChProgramBase+8,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH9"),
        kChBankBase+8,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH10"),
        kChProgramBase+9,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH10"),
        kChBankBase+9,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH11"),
        kChProgramBase+10,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH11"),
        kChBankBase+10,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH12"),
        kChProgramBase+11,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH12"),
        kChBankBase+11,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH13"),
        kChProgramBase+12,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH13"),
        kChBankBase+12,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH14"),
        kChProgramBase+13,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH14"),
        kChBankBase+13,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH15"),
        kChProgramBase+14,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH15"),
        kChBankBase+14,nullptr,0.,1.,0.,1));
    parameters.addParameter(new RangeParameter(STR16("Program CH16"),
        kChProgramBase+15,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Bank CH16"),
        kChBankBase+15,nullptr,0.,1.,0.,1));
    return kResultOk;
}
tresult PLUGIN_API NativeVst3::terminate(){
    // Host has stopped processing before calling terminate(): even if SF2
    // selection was staged while inactive, do not leak its chip RAM.
    cleanStagedAndRetiredNonRealtime();
    synth_.reset();active_=false;authenticRomLoaded_=false;
    return SingleComponentEffect::terminate();
}
tresult PLUGIN_API NativeVst3::setupProcessing(ProcessSetup& setup){
    if(!std::isfinite(setup.sampleRate) || setup.sampleRate<8000. ||
       setup.sampleRate>192000.)return kResultFalse;
    const auto result=SingleComponentEffect::setupProcessing(setup);
    if(result==kResultOk)sampleRate_=setup.sampleRate;
    return result;
}
tresult PLUGIN_API NativeVst3::canProcessSampleSize(int32 size){
    return size==kSample32?kResultTrue:kResultFalse;
}
uint32 PLUGIN_API NativeVst3::getLatencySamples(){
    // P019: never report invented zero latency to the DAW. The pinned
    // 86Box chip has one render-window FIFO (980 native 44.1k frames).
    // Derive the contract from the ACTUAL chip wrapper constant.
    return static_cast<uint32>(std::ceil(
        static_cast<double>(Emu8000Box::kLatencyFrames)*
        sampleRate_/44100.));
}
uint32 PLUGIN_API NativeVst3::getTailSamples(){
    // User SoundFonts and the Creative sustain pedal can hold looping
    // voices. The original FX can continue after the last note as well.
    // There is no safe fixed cutoff common to all configured instruments.
    return kInfiniteTail;
}
bool NativeVst3::buildInstrumentNonRealtime(const std::filesystem::path& selected,
                                              std::unique_ptr<Synth>& result,
                                              std::string& error){
    const auto folder=bundleResourcePath();
    if(folder.empty()){error="Missing VST3 resource folder";return false;}
    const auto rom=folder/"awe32.raw";
    const auto sbk=folder/"SYNTHGM.SBK";
    if(!std::filesystem::is_regular_file(rom) ||
       !std::filesystem::is_regular_file(sbk)){
        error="Original ROM / Creative SBK are missing";return false;
    }
    // This complete Synth is constructed on the host lifecycle or GUI
    // thread, NEVER in the process() callback. Original chip and Creative
    // voice/FX/memory path remain byte-identical to the P022 engine.
    auto candidate=std::make_unique<Synth>(
        static_cast<std::uint32_t>(sampleRate_+0.5));
    const auto bytes=[](const std::filesystem::path& p) {
        const auto utf8=p.u8string();
        return std::string(utf8.begin(),utf8.end());
    };
    const auto romPath=bytes(rom);
    if(!candidate->LoadWaveRom(romPath,error))return false;
    candidate->Core().SetChipRamKb(28672);
    if(!candidate->Core().UseBox86Chip(romPath,error))return false;
    candidate->Core().SetDriver(Awe32::Driver::Win95);
    if(!candidate->LoadBank(bytes(sbk),error,false,-1))return false;
    if(!selected.empty()){
        if(!std::filesystem::is_regular_file(selected)){
            error="Selected owner SoundFont cannot be found";return false;
        }
        if(!candidate->LoadBank(bytes(selected),error,false,1))return false;
    }
    if(auto* ram=candidate->Core().ChipRam()){
        const auto n=std::min(candidate->Core().DramSize(),
                              candidate->Core().ChipRamWords());
        std::memcpy(ram,candidate->Core().DramData(),n*sizeof(std::int16_t));
    } else {error="EMU8000 original 28 MiB memory not mapped";return false;}
    candidate->Core().PowerOnInit();
    if(!candidate->PrepareRealtime(1024u)){
        error="Realtime chip render cannot be prepared";return false;
    }
    result=std::move(candidate);
    error.clear();
    return true;
}
bool NativeVst3::openPrivateSoundBanksNonRealtime(){
    authenticRomLoaded_=false;
    const auto folder=bundleResourcePath();
    if(folder.empty())return false;
    std::filesystem::path sf2;
    {std::lock_guard<std::mutex> lock(guiAssetMutex_);
     if(!selectedUserSf2_.empty())sf2=std::filesystem::u8path(selectedUserSf2_);}
    if(sf2.empty()&&std::filesystem::is_regular_file(folder/"user.sf2"))
        sf2=folder/"user.sf2";
    std::unique_ptr<Synth> candidate;
    std::string error;
    if(!buildInstrumentNonRealtime(sf2,candidate,error))return false;
    synth_=std::move(candidate);
    authenticRomLoaded_=true;
    // CH1..16 and original Creative mapped CC state are restored in exactly
    // the previous order; no previous-bank fallback and no fake preset.
    changeBankProgram();
    replayMidiState();
    for(unsigned i=0;i<guiControls_.size();++i)
        synth_->SetGuiControl(static_cast<int>(i),guiControls_[i]);
    for(unsigned i=0;i<2u;++i)
        synth_->SetGuiSendManual(static_cast<int>(i),sendManual_[i]);
    // Catalog is GUI-only; its data MUST follow the exact loaded owner file.
    auto* nextCatalog=new AWE125::PresetCatalog;
    nextCatalog->SetOriginalCreativeMidiNumbers();
    if(!sf2.empty()){
        const auto text=sf2.u8string();
        const std::string name(text.begin(),text.end());
        AWE125::PresetCatalog user;
        if(user.LoadSoundFont(name,error))
            nextCatalog->AppendCreativeMelodicUserOneFromSource(user);
        std::array<std::uint8_t,32> digest{};
        // Accurate content SHA is prepared while the host is NOT running
        // an audio callback; missing/changed external file will fail recall.
        (void)sha256FileNonRealtime(sf2,digest);
        std::lock_guard<std::mutex> lock(guiAssetMutex_);
        selectedUserSf2_=name;
        selectedUserSha256_=digest;
    }
    delete pendingGuiCatalog_.exchange(nextCatalog,std::memory_order_acq_rel);
    return true;
}
void NativeVst3::publishRuntimeSnapshot() noexcept {
    // Only process() publishes while active. Host setState publishes only
    // while inactive; all members are ATOMIC so even a racing getState sees
    // either a coherent version or safely retries.
    stateMirrorEpoch_.fetch_add(1,std::memory_order_acq_rel); // odd
    stateProgramMirror_.store(static_cast<std::uint32_t>(program_),std::memory_order_relaxed);
    stateBankMirror_.store(static_cast<std::uint32_t>(bank_),std::memory_order_relaxed);
    stateMasterMirror_.store(master_,std::memory_order_relaxed);
    for(unsigned ch=0;ch<16u;++ch){
        stateProgramsMirror_[ch].store(
            ch?programsByChannel_[ch]:static_cast<std::uint8_t>(program_),
            std::memory_order_relaxed);
        stateBanksMirror_[ch].store(
            ch?banksByChannel_[ch]:static_cast<std::uint8_t>(bank_),
            std::memory_order_relaxed);
        stateModesMirror_[ch].store(selectedParameterMode_[ch],
                                   std::memory_order_relaxed);
    }
    for(unsigned i=0;i<midiState_.size();++i){
        stateMidiMirror_[i].store(midiState_[i],std::memory_order_relaxed);
        stateAdvancedMirror_[i].store(advancedMidiState_[i],std::memory_order_relaxed);
    }
    stateMirrorEpoch_.fetch_add(1,std::memory_order_release); // even
}
void NativeVst3::applyRuntimeRestore(const NativeRuntimeRestore& desired) noexcept {
    // Executed at process() boundary (or by the stopped host before active).
    // Never mutate any chip register or audio-owned state from setState().
    program_=static_cast<int>(desired.program);
    bank_=static_cast<int>(desired.bank);
    master_=desired.master;
    programsByChannel_=desired.programs;
    banksByChannel_=desired.banks;
    midiState_=desired.midi;
    advancedMidiState_=desired.advanced;
    selectedParameterMode_=desired.modes;
    guiControls_=desired.gui;
    sendManual_=desired.sendManual;
    const bool changedFx=fxEdited_ || desired.reverb!=4 ||
        desired.chorus!=2 || desired.reverb!=reverbPreset_ ||
        desired.chorus!=chorusPreset_;
    reverbPreset_=desired.reverb;chorusPreset_=desired.chorus;
    // A truly untouched ROM/SBK SoundFont must remain ORIGINAL PCM:
    // never overwrite PowerOnInit original-chip FX coefficients merely
    // because a DAW restored a legacy V4 default project. Only previously
    // edited or genuinely nondefault FX menus reprogram the registers.
    fxEdited_=changedFx;
    if(changedFx)fxPending_.store(true,std::memory_order_release);
    // The original Creative state is authoritative: discard old controller
    // registers first and only then replay this project's exact CC/RPN.
    resetMidiControllersForStateRestore();
    changeBankProgram();
    replayMidiState();
    if(synth_){
        for(unsigned i=0;i<guiControls_.size();++i)
            synth_->SetGuiControl(static_cast<int>(i),guiControls_[i]);
        for(unsigned i=0;i<2u;++i)
            synth_->SetGuiSendManual(static_cast<int>(i),sendManual_[i]);
    }
    publishRuntimeSnapshot();
}
void NativeVst3::applyPendingRestoreRealtime() noexcept {
    if(auto* next=pendingRestore_.exchange(nullptr,std::memory_order_acq_rel)){
        applyRuntimeRestore(*next);
        restoreAppliedSeq_.store(next->sequence,std::memory_order_release);
        // Lock-free linked retirement: delete ONLY on GUI/lifecycle thread.
        auto* top=retiredRestore_.load(std::memory_order_relaxed);
        do {
            next->next=top;
        } while(!retiredRestore_.compare_exchange_weak(
            top,next,std::memory_order_release,std::memory_order_relaxed));
    }
}
void NativeVst3::drainRetiredRestoresNonRealtime() noexcept {
    auto* node=retiredRestore_.exchange(nullptr,std::memory_order_acq_rel);
    while(node){
        auto* following=node->next;
        delete node;
        node=following;
    }
}
void NativeVst3::cleanStagedAndRetiredNonRealtime() noexcept {
    delete stagedSynth_.exchange(nullptr,std::memory_order_acq_rel);
    delete retiredSynth_.exchange(nullptr,std::memory_order_acq_rel);
    delete pendingGuiCatalog_.exchange(nullptr,std::memory_order_acq_rel);
    delete pendingRestore_.exchange(nullptr,std::memory_order_acq_rel);
    drainRetiredRestoresNonRealtime();
}
void NativeVst3::applyStagedInstrumentRealtime() noexcept {
    // Only the audio thread uses synth_. Old engine must be reclaimed on the
    // UI/non-real-time side; **never** destroy its 28MiB buffers in process.
    if(retiredSynth_.load(std::memory_order_acquire)!=nullptr)return;
    if(auto* incoming=stagedSynth_.exchange(nullptr,std::memory_order_acq_rel)){
        retiredSynth_.store(synth_.release(),std::memory_order_release);
        synth_.reset(incoming);
        authenticRomLoaded_=true;
        redirectedDrumNotes_.fill(0);
        changeBankProgram();
        replayMidiState();
        for(unsigned i=0;i<guiControls_.size();++i)
            synth_->SetGuiControl(static_cast<int>(i),guiControls_[i]);
        for(unsigned i=0;i<2u;++i)
            synth_->SetGuiSendManual(static_cast<int>(i),sendManual_[i]);
        if(fxEdited_)fxPending_.store(true,std::memory_order_release);
    }
}
tresult PLUGIN_API NativeVst3::setActive(TBool on){
    // Steinberg setActive is non-real-time. No process callback may be
    // executing while it changes the original EMU8000 engine.
    if(auto* waiting=pendingRestore_.exchange(nullptr,std::memory_order_acq_rel)){
        applyRuntimeRestore(*waiting);
        restoreAppliedSeq_.store(waiting->sequence,std::memory_order_release);
        delete waiting;
    }
    drainRetiredRestoresNonRealtime();
    if(on){
        if(active_)return kResultOk;
        cleanStagedAndRetiredNonRealtime();
        synth_.reset();
        redirectedDrumNotes_.fill(0);
        // Explicit stopped-transport controller edits are project state,
        // not merely editor decoration. Use them when rebuilding the chip.
        for(unsigned ch=0;ch<16u;++ch){
            const int p=hostProgramEdits_[ch].load(std::memory_order_acquire);
            const int b=hostBankEdits_[ch].load(std::memory_order_acquire);
            if(p>=0){
                if(ch==0)program_=p;
                else programsByChannel_[ch]=static_cast<std::uint8_t>(p);
            }
            if(b>=0){
                if(ch==0)bank_=b;
                else banksByChannel_[ch]=static_cast<std::uint8_t>(b);
            }
        }
        const float masterEdit=hostMasterEdit_.load(std::memory_order_acquire);
        if(masterEdit>=0.f)master_=masterEdit;
        try {
            // Missing PRIVATE assets: an explicitly SILENT prototype.
            // Host scanner/validator can inspect the module safely.
            (void)openPrivateSoundBanksNonRealtime();
        }catch(...){
            synth_.reset();
            authenticRomLoaded_=false;
        }
        if(fxEdited_)fxPending_.store(true,std::memory_order_release);
        active_=true;
        publishRuntimeSnapshot();
    }else{
        cleanStagedAndRetiredNonRealtime();
        synth_.reset();active_=false;authenticRomLoaded_=false;
        redirectedDrumNotes_.fill(0);
    }
    return SingleComponentEffect::setActive(on);
}
tresult PLUGIN_API NativeVst3::setProcessing(TBool){return kResultOk;}
void NativeVst3::changeChannelBankProgram(unsigned ch) noexcept{
    if(!synth_ || ch>=16)return;
    // Never resend ProgramChange to other channels during a host automation
    // event. Their held notes and original Creative controller state remain.
    const unsigned chosenBank=ch==0?static_cast<unsigned>(bank_):
                                static_cast<unsigned>(banksByChannel_[ch]);
    const unsigned chosenProgram=ch==0?static_cast<unsigned>(program_):
                                   static_cast<unsigned>(programsByChannel_[ch]);
    synth_->ControlChange(static_cast<std::uint8_t>(ch),0,
                          static_cast<std::uint8_t>(chosenBank==1?1:0));
    synth_->ControlChange(static_cast<std::uint8_t>(ch),32,0);
    synth_->ProgramChange(static_cast<std::uint8_t>(ch),
                          static_cast<std::uint8_t>(chosenProgram));
}
void NativeVst3::changeBankProgram() noexcept{
    for(unsigned ch=0;ch<16;++ch)changeChannelBankProgram(ch);
}
void NativeVst3::applyMidiController(unsigned channel,unsigned slot,
                                     std::uint16_t raw) noexcept{
    if(channel>=16||slot>=kMappedCount)return;
    midiState_[channel*kMappedCount+slot]=raw;
    if(!synth_)return;
    if(slot==kMappedCount-1){
        synth_->PitchBend(static_cast<std::uint8_t>(channel),
                          static_cast<std::int16_t>(static_cast<int>(raw)-8192));
    }else{
        synth_->ControlChange(static_cast<std::uint8_t>(channel),
                              static_cast<std::uint8_t>(kMappedControllers[slot]),
                              static_cast<std::uint8_t>(raw));
    }
}
void NativeVst3::applyAdvancedController(unsigned channel,
                                         unsigned slot,std::uint16_t value) noexcept{
    if(channel>=16||slot>=kAdvancedCount||value>127u)return;
    advancedMidiState_[channel*kAdvancedCount+slot]=value;
    if(slot==2||slot==3) selectedParameterMode_[channel]=1u;  // RPN
    if(slot==6||slot==7) selectedParameterMode_[channel]=2u;  // NRPN
    if(!synth_)return;
    // NRPN has no implemented Creative parameter interpretation. Explicitly
    // deselect RPN before passing the selector to avoid mistakenly applying
    // later NRPN data-entry to a previously selected pitch-bend RPN.
    if(slot==6||slot==7){
        synth_->ControlChange(static_cast<std::uint8_t>(channel),101,127);
        synth_->ControlChange(static_cast<std::uint8_t>(channel),100,127);
    }
    synth_->ControlChange(static_cast<std::uint8_t>(channel),
                          static_cast<std::uint8_t>(kAdvancedControllers[slot]),
                          static_cast<std::uint8_t>(value));
}
void NativeVst3::resetMidiControllersForStateRestore() noexcept{
    if(synth_)resetOriginalCreativeControllers(*synth_);
}
void NativeVst3::replayMidiState() noexcept{
    if(!synth_)return;
    for(unsigned ch=0;ch<16;++ch){
        // CC bank state first. Then melodic CCs, RPN selection + data entry.
        for(unsigned slot=0;slot<2;++slot){
            const auto v=advancedMidiState_[ch*kAdvancedCount+slot];
            if(v!=0xFFFFu)applyAdvancedController(ch,slot,v);
        }
        for(unsigned slot=0;slot<kMappedCount;++slot){
            const auto v=midiState_[ch*kMappedCount+slot];
            if(v!=0xFFFFu)applyMidiController(ch,slot,v);
        }
        const auto mode=selectedParameterMode_[ch];
        if(mode==1){
            for(unsigned slot=2;slot<=5;++slot){
                const auto v=advancedMidiState_[ch*kAdvancedCount+slot];
                if(v!=0xFFFFu)applyAdvancedController(ch,slot,v);
            }
        }else if(mode==2){
            for(unsigned slot=6;slot<=7;++slot){
                const auto v=advancedMidiState_[ch*kAdvancedCount+slot];
                if(v!=0xFFFFu)applyAdvancedController(ch,slot,v);
            }
            // No synthetic NRPN data-entry action; actual driver ignores it.
        }
    }
}
void NativeVst3::dispatch(const Event& event) noexcept{
    if(!synth_)return;
    if(event.type!=Event::kNoteOnEvent &&
       event.type!=Event::kNoteOffEvent)return;
    const bool on=event.type==Event::kNoteOnEvent;
    const int ch=on?event.noteOn.channel:event.noteOff.channel;
    const int key=on?event.noteOn.pitch:event.noteOff.pitch;
    if(ch<0||ch>=16||key<0||key>127)return;
    const auto idx=static_cast<std::size_t>(ch)*128+
                   static_cast<std::size_t>(key);
    if(on){
        const float v=event.noteOn.velocity;
        if(!std::isfinite(v))return;
        if(v>0.f){
            // Redirect CH1 browser audition to Standard GM CH10.
            const bool drums=bank_==2 && ch==0;
            redirectedDrumNotes_[idx]=static_cast<unsigned char>(drums);
            const auto route=static_cast<std::uint8_t>(drums?9:ch);
            synth_->NoteOn(route,static_cast<std::uint8_t>(key),
                static_cast<std::uint8_t>(std::max(1u,toByte(v))));
            return;
        }
    }
    // A zero-velocity NoteOn releases the same voice as an explicit
    // NoteOff, even if the browser bank changed since the key went down.
    const auto route=static_cast<std::uint8_t>(redirectedDrumNotes_[idx]?9:ch);
    redirectedDrumNotes_[idx]=0u;
    synth_->NoteOff(route,static_cast<std::uint8_t>(key));
}
void NativeVst3::render(float* left,float* right,int32 frames) noexcept{
    if(!left||!right||frames<=0)return;
    if(!synth_){
        std::fill_n(left,frames,0.f);std::fill_n(right,frames,0.f);
        return;
    }
    int32 written=0;
    while(written<frames) {
        const int32 n=std::min<int32>(1024,frames-written);
        synth_->RenderBlock(pcm_.data(),static_cast<std::uint32_t>(n));
        for(int32 i=0;i<n;++i) {
            left[written+i]=static_cast<float>(pcm_[i*2])*(master_/32768.f);
            right[written+i]=static_cast<float>(pcm_[i*2+1])*(master_/32768.f);
        }
        written+=n;
    }
}
tresult PLUGIN_API NativeVst3::process(ProcessData& data){
    if(data.numSamples<0)return kResultFalse;
    // A host can flush parameter changes at offset zero without rendering.
    const int32 parameterFrames=std::max<int32>(1,data.numSamples);
    // Capture host-side requests at this block boundary. A newly arriving
    // GUI edit during process() must not be erased by an older audio point.
    std::array<int,16> programAtBlock{},bankAtBlock{};
    for(unsigned ch=0;ch<16u;++ch){
        programAtBlock[ch]=hostProgramEdits_[ch].load(std::memory_order_acquire);
        bankAtBlock[ch]=hostBankEdits_[ch].load(std::memory_order_acquire);
    }
    const float masterAtBlock=hostMasterEdit_.load(std::memory_order_acquire);
    applyStagedInstrumentRealtime();
    applyPendingRestoreRealtime();
    const auto reassert=bankReassertRequested_.exchange(
        0xFFu,std::memory_order_acq_rel);
    if(reassert<=2u){
        // A bank/Program change via CC0 may have changed the actual engine
        // while the VST3 bank parameter remained at the same host value.
        bank_=static_cast<int>(reassert);
        // A newer browser/ROM bank choice supersedes stale MIDI CC0/CC32.
        // Otherwise a saved V4/V5 project would replay the old CC0 AFTER
        // the stored host bank and silently restore a DIFFERENT chip bank.
        advancedMidiState_[0]=0xFFFFu;
        advancedMidiState_[1]=0xFFFFu;
        changeChannelBankProgram(0u);
    }
    if(fxPending_.exchange(false,std::memory_order_acq_rel)&&synth_){
        applyOriginalEmu8000Reverb(*synth_,unsigned(reverbPreset_));
        applyOriginalEmu8000Chorus(*synth_,unsigned(chorusPreset_));
    }
    AudioBusBuffers* bus=(data.numOutputs>0)?&data.outputs[0]:nullptr;
    float* l=(bus && bus->numChannels>=2 && bus->channelBuffers32)?
        bus->channelBuffers32[0]:nullptr;
    float* r=(bus && bus->numChannels>=2 && bus->channelBuffers32)?
        bus->channelBuffers32[1]:nullptr;
    // Real VST3 MIDI CC is delivered by the host as mapped parameter
    // points, not a LegacyMIDICCOutEvent (which is OUTPUT only).
    // A bounded, stack-only queue ensures sample-offset timing and no heap.
    std::array<MidiPoint,512> midiPoints{};
    std::size_t numMidiPoints=0;
    // P014: host bank/program automation is timestamped just like MIDI CC
    // and NoteOn. Previously the FINAL parameter value was applied to the
    // ENTIRE block, which selected the wrong patch for earlier notes.
    constexpr unsigned kPatchProgram=16u,kPatchBank=17u,kPatchMaster=18u,
       kGuiControlSlotBase=20u,kGuiReverbType=32u,kGuiChorusType=33u,
       kGuiSendManualSlotBase=34u;
    const auto queuePatch=[&](IParamValueQueue* q,unsigned channel,
                              unsigned slot,unsigned maximum)->bool{
        const int32 count=q->getPointCount();
        if(count<0||count>512)return false;
        for(int32 p=0;p<count;++p){
            int32 frame=0;ParamValue value=0.;
            if(q->getPoint(p,frame,value)!=kResultOk ||
               !finite01(value) || frame<0 || frame>=parameterFrames)
                continue;
            if(numMidiPoints==midiPoints.size())return false;
            MidiPoint point{};
            point.sampleOffset=frame;
            point.channel=static_cast<std::uint8_t>(channel);
            point.slot=static_cast<std::uint8_t>(slot);
            point.value=static_cast<std::uint16_t>(std::lround(value*maximum));
            auto j=numMidiPoints;
            while(j>0 && midiPoints[j-1].sampleOffset>frame){
                midiPoints[j]=midiPoints[j-1];--j;
            }
            midiPoints[j]=point;
            ++numMidiPoints;
        }
        return true;
    };
    const auto reject=[&]() -> tresult {
        // Fail CLOSED for overflowing host events and CC queue. Returning
        // false without stopping the original EMU8000 leaves old voices
        // sounding when NoteOff was part of the rejected block.
        // Genuine Win95 Creative CC120, all 16 channels, no heap here.
        if(synth_){
            for(unsigned channel=0;channel<16u;++channel)
                synth_->ControlChange(static_cast<std::uint8_t>(channel),120u,0u);
            // The original 86Box model can continue rendering a sustained
            // SF2 voice after Creative CC120 -> DCYSUSV=0x0080 (verified
            // against F09 held-loop PCM: 91.7% energy survived 8 blocks).
            // Fail closed by ALSO writing the ORIGINAL EMU8000 IFATN
            // hardware register to maximum 8-bit voice attenuation for
            // ALL 32 voices, without touching the pinned 86Box DSP code,
            // ROM/SF2 memory or adding an alternate gain/DSP path.
            // New NoteOn reprograms IFATN with its genuine patch value.
            // The pinned RT source has reserved 65,536 I/O writes,
            // including the 16-channel CC120 panic and these 64 port writes.
            for(int voice=0;voice<Synth::kMaxVoices;++voice)
                synth_->Core().Write(Emu8000::Reg::IFATN,voice,0x00FFu);
        }
        redirectedDrumNotes_.fill(0u);
        if(l&&r&&data.numSamples>0){
            std::fill_n(l,data.numSamples,0.f);
            std::fill_n(r,data.numSamples,0.f);
            if(bus)bus->silenceFlags=3;
        }
        publishRuntimeSnapshot();
        return kResultFalse;
    };
    if(data.inputParameterChanges){
        const int32 n=data.inputParameterChanges->getParameterCount();
        if(n<0||n>1024)return reject();
        for(int32 i=0;i<n;++i){
            auto* q=data.inputParameterChanges->getParameterData(i);
            if(!q||q->getPointCount()<1)continue;
            const ParamID id=q->getParameterId();
            // Bank, Program, and all CH2..CH16 patches are sample-offset
            // events, not last-point block-global state changes.
            if(id==kProgram || id==kBank ||
               (id>=kChProgramBase+1 && id<kChProgramBase+16) ||
               (id>=kChBankBase+1 && id<kChBankBase+16)){
                const bool programChange=(id==kProgram ||
                    (id>=kChProgramBase+1 && id<kChProgramBase+16));
                const unsigned channel=(id==kProgram||id==kBank)?0u:
                    (id>=kChProgramBase+1 && id<kChProgramBase+16)?
                        id-kChProgramBase:id-kChBankBase;
                const unsigned maxValue=programChange?127u:
                    (channel==0u?2u:1u);
                if(!queuePatch(q,channel,programChange?kPatchProgram:kPatchBank,
                               maxValue))return reject();
                continue;
            }
            // Master automation is sample-offset accurate, not a
            // block-global last-point override. This uses the same fixed
            // RT-safe 512-entry queue as note, bank and program changes.
            if(id==kVolume){
                const int32 count=q->getPointCount();
                if(count<0||count>512)return reject();
                for(int32 p=0;p<count;++p){
                    int32 at=0;ParamValue value=0.;
                    if(q->getPoint(p,at,value)!=kResultOk||
                       !finite01(value)||at<0||at>=parameterFrames)
                        continue;
                    if(numMidiPoints==midiPoints.size())return reject();
                    MidiPoint point{};
                    point.sampleOffset=at;
                    point.slot=static_cast<std::uint8_t>(kPatchMaster);
                    point.normalized=static_cast<float>(value);
                    auto j=numMidiPoints;
                    while(j>0&&midiPoints[j-1].sampleOffset>at){
                        midiPoints[j]=midiPoints[j-1];--j;
                    }
                    midiPoints[j]=point;
                    ++numMidiPoints;
                }
                continue;
            }
            // All 12 original P175 hardware controls use the *real*
            // pinned Creative voice-parameter conversion, then actual
            // 86Box EMU8000 register writes. Zero leaves P022 exact PCM.
            if(id>=1100u && id<1112u) {
                if(!queuePatch(q,0,kGuiControlSlotBase+id-1100u,100u))
                    return reject();
                continue;
            }
            if(id==1400u||id==1401u){
                if(!queuePatch(q,0,id==1400u?kGuiReverbType:kGuiChorusType,7u))
                    return reject();
                continue;
            }
            if(id==1420u||id==1421u){
                if(!queuePatch(q,0,kGuiSendManualSlotBase+id-1420u,1u))
                    return reject();
                continue;
            }
            const bool basic=(id>=kMidiMappedBase &&
                             id<kMidiMappedBase+16u*kMappedCount);
            const bool advanced=(id>=kAdvancedMappedBase &&
                             id<kAdvancedMappedBase+16u*kAdvancedCount);
            if(basic||advanced){
                const unsigned offset=basic?id-kMidiMappedBase:
                                               id-kAdvancedMappedBase;
                const unsigned channel=offset/kMappedCount;
                const unsigned slot=(offset%kMappedCount)+
                                    (advanced?kMappedCount:0u);
                const int32 nPoints=q->getPointCount();
                if(nPoints<0||nPoints>512)return reject();
                for(int32 p=0;p<nPoints;++p){
                    int32 frame=0;ParamValue v=0.;
                    if(q->getPoint(p,frame,v)!=kResultOk || !finite01(v) ||
                       frame<0||frame>=parameterFrames)continue;
                    if(numMidiPoints==midiPoints.size())return reject();
                    MidiPoint point{};
                    point.sampleOffset=frame;
                    point.channel=static_cast<std::uint8_t>(channel);
                    point.slot=static_cast<std::uint8_t>(slot);
                    point.value=static_cast<std::uint16_t>(std::lround(
                        v*(slot==kMappedCount-1?16383.:127.)));
                    // Stable insertion sort, preserving host point ordering.
                    auto j=numMidiPoints;
                    while(j>0 && midiPoints[j-1].sampleOffset>frame){
                        midiPoints[j]=midiPoints[j-1];--j;
                    }
                    midiPoints[j]=point;
                    ++numMidiPoints;
                }
                continue;
            }
            int32 off=0;ParamValue val=0.;
            if(q->getPoint(q->getPointCount()-1,off,val)!=kResultOk||
               !finite01(val))continue;
            // All recognized timed controls were handled above.
            // Unknown IDs intentionally have no audio side effects.
        }
    }
    if(data.numSamples>0&&(!l||!r)){
        publishRuntimeSnapshot();
        return kResultOk;
    }
    // Fixed-buffer, bounded host events. Overflow deliberately fails the
    // prototype gate rather than silently dropping NoteOff and sticking.
    std::array<Event,512> events{};
    std::size_t num=0;
    if(data.numSamples>0&&data.inputEvents){
        const int32 n=data.inputEvents->getEventCount();
        if(n<0||n>static_cast<int32>(events.size()))
            return reject();
        for(int32 i=0;i<n;++i){
            Event e{};
            if(data.inputEvents->getEvent(i,e)!=kResultOk)continue;
            if(e.sampleOffset<0||e.sampleOffset>=data.numSamples)continue;
            if(e.type!=Event::kNoteOnEvent &&
               e.type!=Event::kNoteOffEvent)continue;
            // Stable insertion-sort by timestamp. Does not allocate.
            auto j=num;
            while(j>0 && events[j-1].sampleOffset>e.sampleOffset){
                events[j]=events[j-1];--j;
            }
            events[j]=e;++num;
        }
    }
    int32 pos=0;
    std::size_t notePos=0,midiPos=0;
    while(notePos<num||midiPos<numMidiPoints){
        const int32 noteOffset=notePos<num?
            events[notePos].sampleOffset:data.numSamples;
        const int32 midiOffset=midiPos<numMidiPoints?
            midiPoints[midiPos].sampleOffset:data.numSamples;
        const int32 next=std::min(noteOffset,midiOffset);
        if(next>pos){render(l+pos,r+pos,next-pos);pos=next;}
        // Controllers at the very same sample precede NoteOn, preserving
        // sustain/modulation/velocity timing without changing chip DSP.
        // SEND mode commands win over SEND value automation at the same
        // sample, independent of a DAW's parameter queue enumeration.
        // In particular RESET (manual -> original) must never be undone
        // because a simultaneous zero SEND knob gesture ran later.
        std::array<int,2> sendModeAtFrame{{-1,-1}};
        while(midiPos<numMidiPoints &&
              midiPoints[midiPos].sampleOffset==next){
            const auto& mp=midiPoints[midiPos++];
            if(mp.slot<kMappedCount){
                applyMidiController(mp.channel,mp.slot,mp.value);
            }else if(mp.slot<kMappedCount+kAdvancedCount){
                applyAdvancedController(mp.channel,
                                         mp.slot-kMappedCount,mp.value);
            }else if(mp.slot==kPatchMaster){
                master_=mp.normalized;
                if(masterAtBlock>=0.f){
                    float expected=masterAtBlock;
                    (void)hostMasterEdit_.compare_exchange_strong(
                        expected,-1.f,std::memory_order_acq_rel);
                }
            }else if(mp.slot==kPatchProgram){
                if(mp.channel==0)program_=static_cast<int>(mp.value);
                else programsByChannel_[mp.channel]=
                    static_cast<std::uint8_t>(mp.value);
                changeChannelBankProgram(mp.channel);
                const int pending=programAtBlock[mp.channel];
                if(pending>=0){
                    int expected=pending;
                    (void)hostProgramEdits_[mp.channel].compare_exchange_strong(
                        expected,-1,std::memory_order_acq_rel);
                }
            }else if(mp.slot==kPatchBank){
                if(mp.channel==0)bank_=static_cast<int>(mp.value);
                else banksByChannel_[mp.channel]=
                    static_cast<std::uint8_t>(mp.value);
                // Host bank selection (including an unchanged bank value)
                // is later than any previously recorded raw CC0/CC32.
                // Erase obsolete overrides instead of reapplying them on
                // project restore; preserve CC/RPN on OTHER MIDI channels.
                const auto base=static_cast<std::size_t>(mp.channel)*
                    kAdvancedCount;
                advancedMidiState_[base]=0xFFFFu;
                advancedMidiState_[base+1u]=0xFFFFu;
                changeChannelBankProgram(mp.channel);
                const int pending=bankAtBlock[mp.channel];
                if(pending>=0){
                    int expected=pending;
                    (void)hostBankEdits_[mp.channel].compare_exchange_strong(
                        expected,-1,std::memory_order_acq_rel);
                }
            }else if(mp.slot==kGuiReverbType||mp.slot==kGuiChorusType){
                if(mp.slot==kGuiReverbType){
                    reverbPreset_=int(mp.value);
                    fxPresetMirrors_[0].store(static_cast<std::uint8_t>(mp.value),
                                             std::memory_order_release);
                    if(synth_)applyOriginalEmu8000Reverb(*synth_,unsigned(reverbPreset_));
                }else{
                    chorusPreset_=int(mp.value);
                    fxPresetMirrors_[1].store(static_cast<std::uint8_t>(mp.value),
                                             std::memory_order_release);
                    if(synth_)applyOriginalEmu8000Chorus(*synth_,unsigned(chorusPreset_));
                }
                fxEdited_=true;
            }else if(mp.slot>=kGuiSendManualSlotBase &&
                     mp.slot<kGuiSendManualSlotBase+2u){
                const auto i=static_cast<unsigned>(mp.slot-kGuiSendManualSlotBase);
                sendModeAtFrame[i]=int(mp.value);
            }else if(mp.slot>=kGuiControlSlotBase &&
                     mp.slot<kGuiControlSlotBase+guiControls_.size()){
                const auto index=static_cast<unsigned>(
                    mp.slot-kGuiControlSlotBase);
                guiControls_[index]=static_cast<float>(mp.value)/100.f;
                guiControlMirrors_[index].store(
                    static_cast<std::uint16_t>(mp.value*100u),
                    std::memory_order_release);
                if(synth_)synth_->SetGuiControl(
                    static_cast<int>(index),guiControls_[index]);
                if(index>=10u){
                    // A real performed send gesture (including an explicit
                    // 0%) switches to absolute hardware send operation.
                    sendManual_[index-10u]=true;
                    sendManualMirrors_[index-10u].store(true,std::memory_order_release);
                    if(synth_)synth_->SetGuiSendManual(int(index-10u),true);
                }
            }
        }
        for(unsigned i=0;i<2u;++i)
            if(sendModeAtFrame[i]>=0){
                sendManual_[i]=sendModeAtFrame[i]!=0;
                sendManualMirrors_[i].store(sendManual_[i],std::memory_order_release);
                if(synth_)synth_->SetGuiSendManual(int(i),sendManual_[i]);
            }
        while(notePos<num && events[notePos].sampleOffset==next)
            dispatch(events[notePos++]);
    }
    if(pos<data.numSamples)render(l+pos,r+pos,data.numSamples-pos);
    if(bus&&data.numSamples>0)bus->silenceFlags=authenticRomLoaded_?0:3;
    publishRuntimeSnapshot();
    return kResultOk;
}
tresult PLUGIN_API NativeVst3::getState(IBStream* stream){
    if(!stream)return kResultFalse;
    // Host save is allowed concurrently with audio callbacks. NEVER read
    // the live non-atomic program/MIDI/RPN arrays here.
    drainRetiredRestoresNonRealtime();
    // After the chip swap, the old 28-MiB native Synth is no longer
    // accessed by the render thread. Reclaim on this host SAVE thread so
    // repeated project switches work even without any GUI editor open.
    delete retiredSynth_.exchange(nullptr,std::memory_order_acq_rel);
    NativeRuntimeRestore stable{};
    const auto request=restoreRequestedSeq_.load(std::memory_order_acquire);
    if(request!=restoreAppliedSeq_.load(std::memory_order_acquire)){
        // A valid project was accepted by setState but will be applied at
        // the next audio block. Return its full requested state meanwhile.
        std::lock_guard<std::mutex> lock(hostRestoreMutex_);
        stable=lastHostRestore_;
    }else{
        bool consistent=false;
        for(int attempt=0;attempt<128 && !consistent;++attempt){
            const auto before=stateMirrorEpoch_.load(std::memory_order_acquire);
            if(before&1u)continue;
            stable.program=stateProgramMirror_.load(std::memory_order_relaxed);
            stable.bank=stateBankMirror_.load(std::memory_order_relaxed);
            stable.master=stateMasterMirror_.load(std::memory_order_relaxed);
            for(unsigned ch=0;ch<16u;++ch){
                stable.programs[ch]=stateProgramsMirror_[ch].load(std::memory_order_relaxed);
                stable.banks[ch]=stateBanksMirror_[ch].load(std::memory_order_relaxed);
                stable.modes[ch]=stateModesMirror_[ch].load(std::memory_order_relaxed);
            }
            for(unsigned i=0;i<stable.midi.size();++i){
                stable.midi[i]=stateMidiMirror_[i].load(std::memory_order_relaxed);
                stable.advanced[i]=stateAdvancedMirror_[i].load(std::memory_order_relaxed);
            }
            const auto after=stateMirrorEpoch_.load(std::memory_order_acquire);
            consistent=(before==after && !(after&1u));
        }
        if(!consistent)return kResultFalse; // never return a torn session
    }
    // The controller's last edit can precede the first process callback
    // (stopped transport, inactive project, or host saving immediately).
    // Keep it ahead of the last published audio snapshot until acknowledged.
    for(unsigned ch=0;ch<16u;++ch){
        const int program=hostProgramEdits_[ch].load(std::memory_order_acquire);
        const int bank=hostBankEdits_[ch].load(std::memory_order_acquire);
        if(program>=0){
            stable.programs[ch]=static_cast<std::uint8_t>(program);
            if(ch==0)stable.program=static_cast<std::uint32_t>(program);
        }
        if(bank>=0){
            stable.banks[ch]=static_cast<std::uint8_t>(bank);
            if(ch==0)stable.bank=static_cast<std::uint32_t>(bank);
        }
    }
    const float masterEdit=hostMasterEdit_.load(std::memory_order_acquire);
    if(masterEdit>=0.f)stable.master=masterEdit;
    Saved s{};
    s.program=stable.program;s.bank=stable.bank;s.master=stable.master;
    for(unsigned ch=1;ch<16u;++ch){
        s.otherPrograms[ch-1]=stable.programs[ch];
        s.otherBanks[ch-1]=stable.banks[ch];
    }
    s.midiStates=stable.midi;
    s.advancedStates=stable.advanced;
    s.parameterModes=stable.modes;
    OwnerStateV5 extended{};
    std::string ownerPath;
    bool explicitOwner=false;
    {
        std::lock_guard<std::mutex> guard(guiAssetMutex_);
        explicitOwner=userChoseSf2_;
        if(explicitOwner){
            ownerPath=selectedUserSf2_;
            extended.assetSha256=selectedUserSha256_;
        }
    }
    // V5 extension high bit encodes independent manual-mode overrides;
    // low three bits remain the actual original EMU8000 FX type. Old V5
    // files without the flag retain their exact unmodified SF2 defaults.
    extended.effectModes[0]=fxPresetMirrors_[0].load(std::memory_order_acquire)|
        (sendManualMirrors_[0].load(std::memory_order_acquire)?0x0100u:0u);
    extended.effectModes[1]=fxPresetMirrors_[1].load(std::memory_order_acquire)|
        (sendManualMirrors_[1].load(std::memory_order_acquire)?0x0100u:0u);
    bool changed=explicitOwner||extended.effectModes[0]!=4u||
                                  extended.effectModes[1]!=2u;
    for(unsigned i=0;i<guiControls_.size();++i){
        extended.guiControls[i]=guiControlMirrors_[i].load(
            std::memory_order_acquire);
        if(extended.guiControls[i]>0)changed=true;
    }
    if(changed){
        if(ownerPath.size()>4096u)return kResultFalse;
        s.version=kStateV5;
        extended.pathBytes=static_cast<std::uint32_t>(ownerPath.size());
    }
    int32 n=0;
    if(stream->write(&s,static_cast<int32>(sizeof(s)),&n)!=kResultOk ||
       n!=static_cast<int32>(sizeof(s)))return kResultFalse;
    if(!changed)return kResultOk;
    if(stream->write(&extended,static_cast<int32>(sizeof(extended)),&n)!=kResultOk ||
       n!=static_cast<int32>(sizeof(extended)))return kResultFalse;
    if(!ownerPath.empty() &&
       (stream->write(ownerPath.data(),static_cast<int32>(ownerPath.size()),&n)!=kResultOk ||
        n!=static_cast<int32>(ownerPath.size())))return kResultFalse;
    return kResultOk;
}
// Steinberg's controller-side IEditController::setComponentState is a
// distinct VST3 host contract. Our SingleComponentEffect IS both the
// component and controller. The default EditController implementation
// returns kNotImplemented even for a correctly serialized 580B V4 / V5
// project; this caused three independently observed warnings in the actual
// published 125A Plugin Tester v0.2.10. Delegating to the SAME fully
// validated state parser guarantees that a controller-only restore updates
// both UI and real chip at the next audio boundary; corrupt projects still
// reject atomically, and no fake success code is returned.
tresult PLUGIN_API NativeVst3::setComponentState(IBStream* stream){
    return setState(stream);
}
tresult PLUGIN_API NativeVst3::setState(IBStream* stream){
    if(!stream)return kResultFalse;
    Saved s{};
    // Compatible with existing owner P009 v1 (20B), P011 v2 (52B).
    int32 n=0;
    if(stream->read(&s,kV1Bytes,&n)!=kResultOk||n!=kV1Bytes||
       s.magic!=0x39303050u||
       (s.version!=kStateV1&&s.version!=kStateV2&&
        s.version!=kStateV3&&s.version!=kStateV4&&s.version!=kStateV5)||
       s.program>127u||s.bank>2u||!finite01(s.master))
        return kResultFalse;
    if(s.version>=kStateV2){
        int32 tail=0;
        if(stream->read(reinterpret_cast<unsigned char*>(&s)+kV1Bytes,
                        kV2Bytes-kV1Bytes,&tail)!=kResultOk ||
           tail!=kV2Bytes-kV1Bytes)return kResultFalse;
        for(unsigned ch=0;ch<15;++ch)
            if(s.otherPrograms[ch]>127u||s.otherBanks[ch]>1u)
                return kResultFalse;
    }
    if(s.version>=kStateV3){
        int32 tail=0;
        if(stream->read(reinterpret_cast<unsigned char*>(&s)+kV2Bytes,
                        kV3Bytes-kV2Bytes,&tail)!=kResultOk ||
           tail!=kV3Bytes-kV2Bytes)return kResultFalse;
        for(unsigned ch=0;ch<16;++ch)
            for(unsigned slot=0;slot<kMappedCount;++slot){
                const auto raw=s.midiStates[ch*kMappedCount+slot];
                if(raw!=0xFFFFu && raw>(slot==kMappedCount-1?16383u:127u))
                    return kResultFalse;
            }
    }
    if(s.version>=kStateV4){
        int32 tail=0;
        if(stream->read(reinterpret_cast<unsigned char*>(&s)+kV3Bytes,
                        static_cast<int32>(sizeof(s))-kV3Bytes,&tail)!=kResultOk ||
           tail!=static_cast<int32>(sizeof(s))-kV3Bytes)return kResultFalse;
        for(unsigned ch=0;ch<16;++ch){
            if(s.parameterModes[ch]>2u)return kResultFalse;
            for(unsigned slot=0;slot<kAdvancedCount;++slot){
                const auto val=s.advancedStates[ch*kAdvancedCount+slot];
                if(val!=0xFFFFu&&val>127u)return kResultFalse;
            }
        }
    }
    OwnerStateV5 ext{};
    std::string requestedPath;
    std::unique_ptr<Synth> preparedAsset;
    AWE125::PresetCatalog freshCatalog;
    bool replaceAsset=false;
    bool nextExplicit=false;
    if(s.version==kStateV5){
        int32 tail=0;
        if(stream->read(&ext,static_cast<int32>(sizeof(ext)),&tail)!=kResultOk ||
           tail!=static_cast<int32>(sizeof(ext))||
           ext.marker!=0x35534150u||ext.pathBytes>4096u)
            return kResultFalse;
        for(const auto control:ext.guiControls)
            if(control>10000u)return kResultFalse;
        for(const auto effect:ext.effectModes)
            if((effect&~0x0107u)!=0u)return kResultFalse;
        if(ext.pathBytes){
            requestedPath.resize(ext.pathBytes);
            if(stream->read(requestedPath.data(),
                static_cast<int32>(ext.pathBytes),&tail)!=kResultOk ||
                tail!=static_cast<int32>(ext.pathBytes) ||
                requestedPath.find('\0')!=std::string::npos)
                return kResultFalse;
            const auto path=std::filesystem::u8path(requestedPath);
            std::array<std::uint8_t,32> actualSha{};
            if(!sha256FileNonRealtime(path,actualSha) ||
                actualSha!=ext.assetSha256)
                return kResultFalse; // no silent fallback to another SF2
            nextExplicit=true;
        }
    }
    // Existing legacy projects have no user-bank identity. Restore their
    // original bundle bank instead of inheriting a previous project's file.
    {
        std::lock_guard<std::mutex> guard(guiAssetMutex_);
        replaceAsset=(nextExplicit!=userChoseSf2_) ||
            (nextExplicit&&(requestedPath!=selectedUserSf2_ ||
                             ext.assetSha256!=selectedUserSha256_));
    }
    if(replaceAsset){
        const auto folder=bundleResourcePath();
        if(folder.empty())return kResultFalse;
        const auto replacement=nextExplicit?
            std::filesystem::u8path(requestedPath):
            (std::filesystem::is_regular_file(folder/"user.sf2")?
                folder/"user.sf2":std::filesystem::path{});
        std::string error;
        if(!buildInstrumentNonRealtime(replacement,preparedAsset,error))
            return kResultFalse;
        freshCatalog.SetOriginalCreativeMidiNumbers();
        if(!replacement.empty()){
            const auto utf8=replacement.u8string();
            AWE125::PresetCatalog user;
            if(user.LoadSoundFont(std::string(utf8.begin(),utf8.end()),error))
                freshCatalog.AppendCreativeMelodicUserOneFromSource(user);
        }
    }

    // Full state is validated, including SHA and FX modes, before any
    // mutation or host hand-off. Allocation here is NON-realtime.
    drainRetiredRestoresNonRealtime();
    std::unique_ptr<NativeRuntimeRestore> desired(
        new(std::nothrow) NativeRuntimeRestore);
    if(!desired)return kResultFalse;
    desired->program=s.program;desired->bank=s.bank;desired->master=s.master;
    for(unsigned ch=1;ch<16u;++ch){
        desired->programs[ch]=s.otherPrograms[ch-1];
        desired->banks[ch]=s.otherBanks[ch-1];
    }
    desired->programs[0]=static_cast<std::uint8_t>(s.program);
    desired->banks[0]=static_cast<std::uint8_t>(s.bank);
    desired->midi.fill(0xFFFFu);
    if(s.version>=kStateV3)desired->midi=s.midiStates;
    desired->advanced.fill(0xFFFFu);
    desired->modes.fill(0u);
    if(s.version>=kStateV4){
        desired->advanced=s.advancedStates;
        desired->modes=s.parameterModes;
    }
    for(unsigned i=0;i<desired->gui.size();++i)
        desired->gui[i]=(s.version==kStateV5?ext.guiControls[i]:0u)/10000.f;
    desired->reverb=s.version==kStateV5?int(ext.effectModes[0]&7u):4;
    desired->chorus=s.version==kStateV5?int(ext.effectModes[1]&7u):2;
    if(s.version==kStateV5)
        for(unsigned i=0;i<2u;++i)
            desired->sendManual[i]=(ext.effectModes[i]&0x0100u)!=0u ||
                ext.guiControls[10u+i]>0u; // pre-override V5 compatibility
    // User file and preset catalog are always handled on the host thread.
    // The complete original-chip candidate is staged atomically and
    // consumed at the following audio block, with no heap in process().
    if(replaceAsset){
        delete retiredSynth_.exchange(nullptr,std::memory_order_acq_rel);
        if(preparedAsset)
            delete stagedSynth_.exchange(preparedAsset.release(),
                std::memory_order_acq_rel);
        delete pendingGuiCatalog_.exchange(
            new AWE125::PresetCatalog(std::move(freshCatalog)),
            std::memory_order_acq_rel);
        // Only the host-owned atomic active flag may be read here. Never
        // inspect synth_ (an audio-thread-owned unique_ptr) from setState.
        if(!active_.load(std::memory_order_acquire))
            delete stagedSynth_.exchange(nullptr,std::memory_order_acq_rel);
        std::lock_guard<std::mutex> guard(guiAssetMutex_);
        selectedUserSf2_=nextExplicit?requestedPath:std::string{};
        selectedUserSha256_=nextExplicit?ext.assetSha256:
            std::array<std::uint8_t,32>{};
        userChoseSf2_=nextExplicit;
    }
    // An accepted project restore replaces every pending controller edit.
    // Invalid/corrupt projects return above WITHOUT changing these flags.
    for(auto& p:hostProgramEdits_)p.store(-1,std::memory_order_release);
    for(auto& b:hostBankEdits_)b.store(-1,std::memory_order_release);
    hostMasterEdit_.store(-1.f,std::memory_order_release);
    for(unsigned i=0;i<desired->gui.size();++i){
        const auto value=s.version==kStateV5?ext.guiControls[i]:0u;
        guiControlMirrors_[i].store(value,std::memory_order_release);
        (void)SingleComponentEffect::setParamNormalized(
            1100u+i,static_cast<double>(value)/10000.);
    }
    for(unsigned i=0;i<2u;++i){
        sendManualMirrors_[i].store(desired->sendManual[i],std::memory_order_release);
        (void)SingleComponentEffect::setParamNormalized(
            1420u+i,desired->sendManual[i]?1.:0.);
    }
    fxPresetMirrors_[0].store(static_cast<std::uint8_t>(
        desired->reverb),std::memory_order_release);
    fxPresetMirrors_[1].store(static_cast<std::uint8_t>(
        desired->chorus),std::memory_order_release);
    (void)SingleComponentEffect::setParamNormalized(
        1400u,double(desired->reverb)/7.);
    (void)SingleComponentEffect::setParamNormalized(
        1401u,double(desired->chorus)/7.);
    (void)SingleComponentEffect::setParamNormalized(
        kProgram,double(desired->program)/127.);
    (void)SingleComponentEffect::setParamNormalized(
        kBank,double(desired->bank)/2.);
    (void)SingleComponentEffect::setParamNormalized(
        kVolume,double(desired->master));
    // Host getState must observe the just-accepted full session even if
    // process() has not yet reached the next block boundary.
    {
        std::lock_guard<std::mutex> lock(hostRestoreMutex_);
        lastHostRestore_=*desired;
    }
    desired->sequence=restoreRequestedSeq_.fetch_add(
        1,std::memory_order_acq_rel)+1u;
    if(active_.load(std::memory_order_acquire)){
        // Superseded pending plans are reclaimed on this host thread only.
        delete pendingRestore_.exchange(desired.release(),
                                         std::memory_order_acq_rel);
    }else{
        applyRuntimeRestore(*desired);
        restoreAppliedSeq_.store(desired->sequence,
                                 std::memory_order_release);
    }
    return kResultOk;
}
} // namespace p009
