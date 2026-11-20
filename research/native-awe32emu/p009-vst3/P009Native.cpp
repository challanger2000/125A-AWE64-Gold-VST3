#include "P009Native.hpp"
#include "Synth.h"
#include "Awe32Driver.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "pluginterfaces/base/ibstream.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cmath>
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
struct Saved {
    std::uint32_t magic=0x39303050u,version=2u;
    std::uint32_t program=0u,bank=0u;
    float master=1.f;
    std::array<std::uint8_t,15> otherPrograms{};
    std::array<std::uint8_t,15> otherBanks{};
    std::array<std::uint8_t,2> reserved{}; // explicit 4-byte aligned length
};
static_assert(sizeof(Saved)==52u,"P011 precise VST3 state bytes");
constexpr std::uint32_t kStateV1=1u,kStateV2=2u;
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
void NativeVst3::changeBankProgram() noexcept{
    if(!synth_)return;
    // Apply a distinct original Creative program/bank selection on ALL 16
    // MIDI channels. Only CH1's browser bank2 is an audition shortcut to
    // GM channel 10. CH10 itself always uses the original GM drum bank128.
    for(unsigned ch=0;ch<16;++ch){
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
    if(data.inputParameterChanges){
        const int32 n=std::clamp<int32>(
            data.inputParameterChanges->getParameterCount(),0,128);
        bool updated=false;
        for(int32 i=0;i<n;++i){
            auto* q=data.inputParameterChanges->getParameterData(i);
            if(!q||q->getPointCount()<1)continue;
            int32 off=0;ParamValue val=0.;
            if(q->getPoint(q->getPointCount()-1,off,val)!=kResultOk||
               !finite01(val))continue;
            switch(q->getParameterId()){
                case kProgram:
                    program_=std::clamp<int>(
                        static_cast<int>(std::lround(val*127.)),0,127);
                    updated=true;break;
                case kBank:
                    bank_=std::clamp<int>(
                        static_cast<int>(std::lround(val*2.)),0,2);
                    updated=true;break;
                case kVolume:
                    master_=static_cast<float>(val);break;
                default: {
                    // CH2..CH16, independent automation. Preserve every
                    // channel's MIDI state and last selected program.
                    const ParamID id=q->getParameterId();
                    if(id>=kChProgramBase+1 && id<kChProgramBase+16){
                        const unsigned ch=id-kChProgramBase;
                        programsByChannel_[ch]=static_cast<std::uint8_t>(
                            std::clamp<int>(static_cast<int>(std::lround(val*127.)),0,127));
                        updated=true;
                    }else if(id>=kChBankBase+1 && id<kChBankBase+16){
                        const unsigned ch=id-kChBankBase;
                        banksByChannel_[ch]=static_cast<std::uint8_t>(
                            std::clamp<int>(static_cast<int>(std::lround(val)),0,1));
                        updated=true;
                    }
                    break;
                }
            }
        }
        if(updated)changeBankProgram();
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
    for(std::size_t i=0;i<num;++i){
        const auto end=events[i].sampleOffset;
        if(end>pos){render(l+pos,r+pos,end-pos);pos=end;}
        dispatch(events[i]);
    }
    if(pos<data.numSamples)render(l+pos,r+pos,data.numSamples-pos);
    if(bus)bus->silenceFlags=authenticRomLoaded_?0:3;
    return kResultOk;
}
tresult PLUGIN_API NativeVst3::getState(IBStream* stream){
    if(!stream)return kResultFalse;
    Saved s{};
    s.program=static_cast<std::uint32_t>(program_);
    s.bank=static_cast<std::uint32_t>(bank_);s.master=master_;
    for(unsigned ch=1;ch<16;++ch){
        s.otherPrograms[ch-1]=programsByChannel_[ch];
        s.otherBanks[ch-1]=banksByChannel_[ch];
    }
    int32 n=0;
    return stream->write(&s,static_cast<int32>(sizeof(s)),&n)==kResultOk &&
           n==static_cast<int32>(sizeof(s))?kResultOk:kResultFalse;
}
tresult PLUGIN_API NativeVst3::setState(IBStream* stream){
    if(!stream)return kResultFalse;
    Saved s{};
    // Existing P009 v1 project payload was exactly 20 bytes. Read the
    // original 20-byte prefix, then the optional v2 extension separately.
    constexpr int32 kLegacyBytes=20;
    int32 n=0;
    if(stream->read(&s,kLegacyBytes,&n)!=kResultOk||n!=kLegacyBytes||
       s.magic!=0x39303050u||
       (s.version!=kStateV1&&s.version!=kStateV2)||
       s.program>127u||s.bank>2u||!finite01(s.master))
        return kResultFalse;
    if(s.version==kStateV2){
        int32 tail=0;
        if(stream->read(reinterpret_cast<unsigned char*>(&s)+kLegacyBytes,
                        static_cast<int32>(sizeof(s))-kLegacyBytes,&tail)
                        !=kResultOk ||
           tail!=static_cast<int32>(sizeof(s))-kLegacyBytes)
            return kResultFalse;
        for(unsigned ch=0;ch<15;++ch)
            if(s.otherPrograms[ch]>127u||s.otherBanks[ch]>1u)
                return kResultFalse;
    }
    // Full validation above, THEN mutation: corrupt state cannot partially
    // overwrite a running project's 16 channels.
    program_=static_cast<int>(s.program);bank_=static_cast<int>(s.bank);
    master_=s.master;
    programsByChannel_.fill(0u);banksByChannel_.fill(0u);
    for(unsigned ch=1;ch<16;++ch){
        programsByChannel_[ch]=s.otherPrograms[ch-1];
        banksByChannel_[ch]=s.otherBanks[ch-1];
    }
    changeBankProgram();
    return kResultOk;
}
} // namespace p009
