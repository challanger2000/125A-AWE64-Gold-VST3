#include "P009Native.hpp"
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
constexpr std::uint32_t kStateV1=1u,kStateV2=2u,kStateV3=3u,kStateV4=4u;
constexpr int32 kV1Bytes=20,kV2Bytes=52,kV3Bytes=308;
struct MidiPoint {
    int32 sampleOffset=0;
    std::uint8_t channel=0,slot=0;
    std::uint16_t value=0;
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
    parameters.addParameter(new RangeParameter(STR16("Program CH1"),
        kProgram,nullptr,0.,127.,0.,127));
    parameters.addParameter(new RangeParameter(STR16("Master"),
        kVolume,STR16("%"),0.,100.,100.));
    parameters.addParameter(new RangeParameter(STR16("Bank CH1"),
        kBank,nullptr,0.,2.,0.,2));
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
    synth_.reset();active_=false;
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
    return static_cast<uint32>(std::ceil(980.*sampleRate_/44100.));
}
bool NativeVst3::openPrivateSoundBanksNonRealtime(){
    authenticRomLoaded_=false;
    const auto folder=bundleResourcePath();
    if(folder.empty())return false;
    const auto rom=folder/"awe32.raw";
    const auto sbk=folder/"SYNTHGM.SBK";
    if(!std::filesystem::is_regular_file(rom) ||
       !std::filesystem::is_regular_file(sbk))return false;
    // All allocations, ROM I/O, 28MiB RAM, and bank loading happen off
    // process() in the host's setActive lifecycle.
    auto candidate=std::make_unique<Synth>(
        static_cast<std::uint32_t>(sampleRate_+0.5));
    std::string error;
    const auto romPath=rom.u8string();
    if(!candidate->LoadWaveRom(romPath,error))return false;
    candidate->Core().SetChipRamKb(28672);
    if(!candidate->Core().UseBox86Chip(romPath,error))return false;
    candidate->Core().SetDriver(Awe32::Driver::Win95);
    if(!candidate->LoadBank(sbk.u8string(),error,false,-1))
        return false;
    // User bank1 is OPTIONAL, never a fake sound. Existing ROM bank
    // remains loaded if no user SF2 is present.
    const auto user=folder/"user.sf2";
    if(std::filesystem::is_regular_file(user) &&
       !candidate->LoadBank(user.u8string(),error,false,1))
        return false;
    if(auto* ram=candidate->Core().ChipRam()){
        const auto n=std::min(candidate->Core().DramSize(),
                              candidate->Core().ChipRamWords());
        std::memcpy(ram,candidate->Core().DramData(),n*sizeof(std::int16_t));
    }else return false;
    candidate->Core().PowerOnInit();
    // Allocate ALL sample, selection, chorus, port FIFO, resampling scratch
    // outside process() and fail closed if an owner bank is unbounded.
    if (!candidate->PrepareRealtime(1024u))return false;
    synth_=std::move(candidate);
    authenticRomLoaded_=true;
    changeBankProgram();
    replayMidiState();
    return true;
}
tresult PLUGIN_API NativeVst3::setActive(TBool on){
    if(on){
        if(active_)return kResultOk;
        synth_.reset();
        redirectedDrumNotes_.fill(0);
        try {
            // Missing PRIVATE assets: an explicitly SILENT prototype.
            // Host scanner/validator can inspect the module safely.
            (void)openPrivateSoundBanksNonRealtime();
        }catch(...){
            synth_.reset();
            authenticRomLoaded_=false;
        }
        active_=true;
    }else{
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
    if(!synth_)return;
    // A state replacement on an ALREADY active VST3 must be independent of
    // the previous project's controller values. An absent CC (0xFFFF) means
    // authentic freshly constructed ChannelState default, not "keep the
    // previous song's CC7=0 / sustain / bend / FX send".
    // These constants exactly match pinned AWE32Emu Synth::ChannelState.
    // Only called for explicit host state replacement, NEVER on a clean
    // fresh activation; do not introduce gratuitous register writes there.
    for(unsigned ch=0;ch<16;++ch){
        const auto channel=static_cast<std::uint8_t>(ch);
        synth_->ControlChange(channel,64,0);     // sustain off
        synth_->ControlChange(channel,1,0);      // modulation
        synth_->ControlChange(channel,7,100);    // volume
        synth_->ControlChange(channel,10,64);    // pan center
        synth_->ControlChange(channel,11,127);   // expression
        synth_->ControlChange(channel,91,0);     // original dry send = zero
        synth_->ControlChange(channel,93,0);     // original dry send = zero
        synth_->ControlChange(channel,101,0);    // select RPN 0,0
        synth_->ControlChange(channel,100,0);
        synth_->ControlChange(channel,6,2);      // original bend range = 2
        synth_->ControlChange(channel,101,127);  // deselect RPN
        synth_->ControlChange(channel,100,127);
        synth_->PitchBend(channel,0);            // center
    }
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
    const bool drums=bank_==2 && ch==0;
    if(on){
        const float v=event.noteOn.velocity;
        if(!std::isfinite(v))return;
        // Change to Standard GM CH10 only for CH1 browser audition;
        // original CH2..CH16 are always separately addressable.
        redirectedDrumNotes_[idx]=static_cast<unsigned char>(drums);
        const unsigned route=drums?9u:static_cast<unsigned>(ch);
        if(v<=0.f)synth_->NoteOff(route,key);
        else synth_->NoteOn(route,key,
             static_cast<std::uint8_t>(std::max(1u,toByte(v))));
    }else{
        const unsigned route=redirectedDrumNotes_[idx]?9u:
                              static_cast<unsigned>(ch);
        redirectedDrumNotes_[idx]=0u;
        synth_->NoteOff(static_cast<std::uint8_t>(route),
                       static_cast<std::uint8_t>(key));
    }
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
    constexpr unsigned kPatchProgram=16u,kPatchBank=17u;
    const auto queuePatch=[&](IParamValueQueue* q,unsigned channel,
                              unsigned slot,unsigned maximum)->bool{
        const int32 count=q->getPointCount();
        if(count<0||count>512)return false;
        for(int32 p=0;p<count;++p){
            int32 frame=0;ParamValue value=0.;
            if(q->getPoint(p,frame,value)!=kResultOk ||
               !finite01(value) || frame<0 || frame>=data.numSamples)
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
        if(l&&r&&data.numSamples>0){
            std::fill_n(l,data.numSamples,0.f);
            std::fill_n(r,data.numSamples,0.f);
            if(bus)bus->silenceFlags=3;
        }
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
                       frame<0||frame>=data.numSamples)continue;
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
            if(id==kVolume)master_=static_cast<float>(val);
        }
    }
    if(!l||!r||data.numSamples==0)return kResultOk;
    // Fixed-buffer, bounded host events. Overflow deliberately fails the
    // prototype gate rather than silently dropping NoteOff and sticking.
    std::array<Event,512> events{};
    std::size_t num=0;
    if(data.inputEvents){
        const int32 n=data.inputEvents->getEventCount();
        if(n<0||n>static_cast<int32>(events.size())){
            std::fill_n(l,data.numSamples,0.f);
            std::fill_n(r,data.numSamples,0.f);
            if(bus)bus->silenceFlags=3;
            return kResultFalse;
        }
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
        while(midiPos<numMidiPoints &&
              midiPoints[midiPos].sampleOffset==next){
            const auto& mp=midiPoints[midiPos++];
            if(mp.slot<kMappedCount){
                applyMidiController(mp.channel,mp.slot,mp.value);
            }else if(mp.slot<kMappedCount+kAdvancedCount){
                applyAdvancedController(mp.channel,
                                         mp.slot-kMappedCount,mp.value);
            }else if(mp.slot==kPatchProgram){
                if(mp.channel==0)program_=static_cast<int>(mp.value);
                else programsByChannel_[mp.channel]=
                    static_cast<std::uint8_t>(mp.value);
                changeChannelBankProgram(mp.channel);
            }else if(mp.slot==kPatchBank){
                if(mp.channel==0)bank_=static_cast<int>(mp.value);
                else banksByChannel_[mp.channel]=
                    static_cast<std::uint8_t>(mp.value);
                changeChannelBankProgram(mp.channel);
            }
        }
        while(notePos<num && events[notePos].sampleOffset==next)
            dispatch(events[notePos++]);
    }
    if(pos<data.numSamples)render(l+pos,r+pos,data.numSamples-pos);
    if(bus)bus->silenceFlags=authenticRomLoaded_?0:3;
    return kResultOk;
}
tresult PLUGIN_API NativeVst3::getState(IBStream* stream){
    if(!stream)return kResultFalse;
    Saved s{};s.program=static_cast<std::uint32_t>(program_);
    s.bank=static_cast<std::uint32_t>(bank_);s.master=master_;
    for(unsigned ch=1;ch<16;++ch){
        s.otherPrograms[ch-1]=programsByChannel_[ch];
        s.otherBanks[ch-1]=banksByChannel_[ch];
    }
    s.midiStates=midiState_;
    s.advancedStates=advancedMidiState_;
    s.parameterModes=selectedParameterMode_;
    int32 n=0;
    return stream->write(&s,static_cast<int32>(sizeof(s)),&n)==kResultOk &&
           n==static_cast<int32>(sizeof(s))?kResultOk:kResultFalse;
}
tresult PLUGIN_API NativeVst3::setState(IBStream* stream){
    if(!stream)return kResultFalse;
    Saved s{};
    // Compatible with existing owner P009 v1 (20B), P011 v2 (52B).
    int32 n=0;
    if(stream->read(&s,kV1Bytes,&n)!=kResultOk||n!=kV1Bytes||
       s.magic!=0x39303050u||
       (s.version!=kStateV1&&s.version!=kStateV2&&
        s.version!=kStateV3&&s.version!=kStateV4)||
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
    if(s.version==kStateV4){
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
    // Only now mutate processor. Corrupt v4 cannot change any channel.
    program_=static_cast<int>(s.program);bank_=static_cast<int>(s.bank);
    master_=s.master;
    programsByChannel_.fill(0u);banksByChannel_.fill(0u);
    for(unsigned ch=1;ch<16;++ch){
        programsByChannel_[ch]=s.otherPrograms[ch-1];
        banksByChannel_[ch]=s.otherBanks[ch-1];
    }
    midiState_.fill(0xFFFFu);
    if(s.version>=kStateV3)midiState_=s.midiStates;
    advancedMidiState_.fill(0xFFFFu);
    selectedParameterMode_.fill(0u);
    if(s.version==kStateV4){
        advancedMidiState_=s.advancedStates;
        selectedParameterMode_=s.parameterModes;
    }
    // P015: when another project is restored while the engine is live,
    // clear the previous project's chip-side controller state FIRST.
    // Host state has been fully validated above, so invalid streams never
    // reset or otherwise touch a sounding plugin.
    resetMidiControllersForStateRestore();
    changeBankProgram();
    replayMidiState();
    return kResultOk;
}
} // namespace p009
