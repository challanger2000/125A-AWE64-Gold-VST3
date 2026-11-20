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
struct Saved {
    std::uint32_t magic=0x39303050u,version=1u;
    std::uint32_t program=0u,bank=0u;
    float master=1.f;
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
    // All channels preserve independent Creative state. A selected drum
    // browser token redirects ONLY CH1 audition to standard GM CH10.
    const int melodicBank=bank_==1?1:0;
    synth_->ControlChange(0,0,static_cast<std::uint8_t>(melodicBank));
    synth_->ControlChange(0,32,0);
    synth_->ProgramChange(0,static_cast<std::uint8_t>(program_));
    synth_->ProgramChange(9,0); // default Standard GM kit
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
                default:break;
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
    Saved s{};s.program=static_cast<std::uint32_t>(program_);
    s.bank=static_cast<std::uint32_t>(bank_);s.master=master_;
    int32 n=0;
    return stream->write(&s,sizeof(s),&n)==kResultOk &&
           n==sizeof(s)?kResultOk:kResultFalse;
}
tresult PLUGIN_API NativeVst3::setState(IBStream* stream){
    if(!stream)return kResultFalse;
    Saved s{};int32 n=0;
    if(stream->read(&s,sizeof(s),&n)!=kResultOk||n!=sizeof(s)||
       s.magic!=0x39303050u||s.version!=1u||
       s.program>127u||s.bank>2u||!finite01(s.master))
        return kResultFalse;
    program_=static_cast<int>(s.program);bank_=static_cast<int>(s.bank);
    master_=s.master;changeBankProgram();
    return kResultOk;
}
} // namespace p009
