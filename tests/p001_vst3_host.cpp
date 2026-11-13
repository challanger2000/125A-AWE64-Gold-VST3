// An independent host invokes the *exported* actual Windows VST3 DLL.
// This is P001's ONE fixed AWETEST ROM voice, not full Creative GM/SBK/SF2.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
void require(bool ok,const char* reason){if(!ok) throw std::runtime_error(reason);}
struct Instance {
    IComponent* component=nullptr;
    IAudioProcessor* audio=nullptr;
    explicit Instance(IPluginFactory* factory) {
        PClassInfo info{};
        bool found=false;
        for(int32 i=0;i<factory->countClasses();++i){
            require(factory->getClassInfo(i,&info)==kResultOk,"factory class info");
            if(std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}
        }
        require(found,"instrument class absent");
        require(factory->createInstance(info.cid,IComponent::iid,
            reinterpret_cast<void**>(&component))==kResultOk&&component,
            "actual DLL could not instantiate IComponent");
        require(component->initialize(nullptr)==kResultOk,"actual DLL initialize");
        require(component->queryInterface(IAudioProcessor::iid,
            reinterpret_cast<void**>(&audio))==kResultOk&&audio,
            "actual DLL audio interface missing");
        require(component->getBusCount(kAudio,kInput)==0,"unexpected audio input");
        require(component->getBusCount(kAudio,kOutput)==1,"stereo output absent");
        require(component->getBusCount(kEvent,kInput)==1,"MIDI event bus absent");
        SpeakerArrangement mono=SpeakerArr::kMono;
        require(audio->setBusArrangements(nullptr,0,&mono,1)!=kResultTrue,
                "mono wrongly advertised as supported");
        SpeakerArrangement stereo=SpeakerArr::kStereo;
        require(audio->setBusArrangements(nullptr,0,&stereo,1)==kResultTrue,
                "stereo bus negotiation failed");
        ProcessSetup good{kRealtime,kSample32,512,44100.0};
        require(audio->setupProcessing(good)==kResultOk,"44.1k setup");
        ProcessSetup unsupported{kRealtime,kSample32,512,48000.0};
        require(audio->setupProcessing(unsupported)!=kResultOk,
                "48k must be rejected until proven resampler exists");
        require(audio->setupProcessing(good)==kResultOk,"restoring 44.1k");
    }
    void activate(){
        require(component->setActive(true)==kResultOk,"real-ROM setActive failed");
        require(audio->setProcessing(true)==kResultOk,"setProcessing failed");
    }
    ~Instance() {
        if(audio){audio->setProcessing(false);audio->release();}
        if(component){component->setActive(false);component->terminate();component->release();}
    }
};
void event(EventList& events,bool on,int offset) {
    Event e{};
    e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;
    e.sampleOffset=offset;e.busIndex=0;
    if(on){
        e.noteOn.channel=0;e.noteOn.pitch=60;
        e.noteOn.velocity=.85f;e.noteOn.noteId=42;
    } else {
        e.noteOff.channel=0;e.noteOff.pitch=60;
        e.noteOff.velocity=0.f;e.noteOff.noteId=42;
    }
    require(events.addEvent(e)==kResultOk,"host MIDI enqueue");
}
std::vector<float> play(IPluginFactory* factory,bool release) {
    Instance synth(factory);
    synth.activate();
    std::vector<float> output;
    output.reserve(512u*36u);
    for(int block=0;block<36;++block){
        std::array<float,512> left{},right{};
        float* buffers[]{left.data(),right.data()};
        AudioBusBuffers bus{};
        bus.numChannels=2;bus.channelBuffers32=buffers;
        EventList events;
        if(block==0) event(events,true,137);
        if(release && block==2) event(events,false,127);
        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
        data.numSamples=512;data.outputs=&bus;data.numOutputs=1;
        data.inputEvents=&events;
        require(synth.audio->process(data)==kResultOk,"actual VST3 DLL process");
        for(int i=0;i<512;++i)
            require(std::isfinite(left[i])&&std::isfinite(right[i]),"nonfinite output");
        output.insert(output.end(),left.begin(),left.end());
    }
    return output;
}
}
int wmain(int argc,wchar_t** argv){
    // Preserve the independent pre-CT1745 AWE32_WAV PCM16 tap for
    // all historical original-chip reference and RT heap regressions.
    // Normal VST3 instances without this private flag use full CT1745.
    if(!SetEnvironmentVariableW(L"AWE64_P002_PRE_CARD_PCM_DIAGNOSTIC",L"1"))return 87;
    if(argc!=3)return 2;
    HMODULE module=nullptr;
    IPluginFactory* factory=nullptr;
    try{
        require(SetEnvironmentVariableW(L"AWE64_P001_ROM_PATH",argv[2])!=0,
                "cannot configure private original ROM");
        module=LoadLibraryW(argv[1]);require(module!=nullptr,"VST3 LoadLibraryW failed");
        auto get=reinterpret_cast<IPluginFactory*(*)()>(
            GetProcAddress(module,"GetPluginFactory"));
        require(get!=nullptr,"actual DLL has no GetPluginFactory");
        factory=get();require(factory!=nullptr,"null factory");
        auto held=play(factory,false);
        auto off=play(factory,true);
        require(held.size()==512u*36u && held.size()==off.size(),"wrong PCM size");
        require(std::all_of(held.begin(),held.begin()+137,[](float v){return v==0.f;}),
                "sample offset 137 violated: audio before NoteOn");
        const auto nonzero=std::count_if(held.begin()+137,held.end(),
            [](float v){return std::abs(v)>0.00001f;});
        require(nonzero>5000,"exported VST3 rendered no audible original ROM");
        const auto noteoffFrame=512u*2u+127u;
        require(std::equal(held.begin(),held.begin()+noteoffFrame,off.begin()),
                "NoteOff changed samples before its exact host offset");
        require(!std::equal(held.begin()+noteoffFrame,held.end(),
                             off.begin()+noteoffFrame),
                "NoteOff had no audible effect at any later frame");
        factory->release();factory=nullptr;
        FreeLibrary(module);module=nullptr;
        std::cout<<"P001_REAL_VST3_HOST_MIDI_PASS frames="<<held.size()
                 <<" noteon=137 noteoff="<<noteoffFrame<<" nonzero="<<nonzero<<'\n';
        return 0;
    }catch(const std::exception& e) {
        if(factory)factory->release();
        if(module)FreeLibrary(module);
        std::cerr<<"P001_REAL_VST3_HOST_MIDI_FAIL "<<e.what()<<'\n';
        return 1;
    }
}
