// P007W: exercise the ACTUAL Windows VST3 module through Steinberg hosting APIs.
// Public CI: real compiled VST3 getState/setState roundtrip, no private Win95.
// --private-audio: additionally boot the privately bundled original Win95 and
// verify real host->VST3 MIDI NoteOn/Off -> nonzero output PCM. Never upload it.
// This is a QA executable, NOT part of the shipping one-folder VST3.
#define NOMINMAX
#include <windows.h>
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

namespace fs = std::filesystem;
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace std::chrono_literals;

namespace {
constexpr ParamID bankId = 1002, programId = 1000, masterId = 1001;
constexpr int block = 1024;
void require(bool ok, const char* description) {
    if (!ok) {
        std::fprintf(stderr, "P007W_FAIL %s\n", description);
        throw std::runtime_error(description);
    }
}
bool closeTo(double a, double b) { return std::fabs(a-b) < 0.000001; }

struct Instance {
    IPtr<IComponent> component;
    IPtr<IEditController> controller;
    IPtr<IAudioProcessor> processor;
    bool initialized = false;
    Instance()=default;
    Instance(const Instance&)=delete;
    Instance& operator=(const Instance&)=delete;
    Instance(Instance&&)=default;
    Instance& operator=(Instance&&)=delete;
    ~Instance() {
        if (component && initialized) (void)component->terminate();
    }
};
Instance instantiate(const VST3::Hosting::PluginFactory& factory,
                     const VST3::UID& id, FUnknown* context) {
    Instance result;
    result.component = factory.createInstance<IComponent>(id);
    require(bool(result.component), "factory could not create IComponent");
    require(result.component->initialize(context)==kResultOk, "IComponent initialize");
    result.initialized=true;
    IEditController* edit=nullptr;
    require(result.component->queryInterface(IEditController::iid,
        reinterpret_cast<void**>(&edit))==kResultOk && edit!=nullptr,
        "combined IEditController missing");
    result.controller=owned(edit);
    IAudioProcessor* proc=nullptr;
    require(result.component->queryInterface(IAudioProcessor::iid,
        reinterpret_cast<void**>(&proc))==kResultOk && proc!=nullptr,
        "IAudioProcessor missing");
    result.processor=owned(proc);
    return result;
}
void rewind(IBStream& s) {
    require(s.seek(0,kIBSeekSet,nullptr)==kResultOk, "IBStream seek");
}
std::array<char,16> readState(IComponent& component) {
    MemoryStream stream;
    require(component.getState(&stream)==kResultOk, "IComponent getState");
    require(stream.getSize()==16, "state is not canonical 16 bytes");
    std::array<char,16> result{};
    std::memcpy(result.data(), stream.getData(), result.size());
    return result;
}
void writeState(IComponent& component, const std::array<char,16>& bytes,
                bool shouldSucceed) {
    MemoryStream s;
    int32 written=0;
    require(s.write(const_cast<char*>(bytes.data()), 16, &written)==kResultOk
        && written==16, "QA IBStream write");
    rewind(s);
    const auto result=component.setState(&s);
    require((result==kResultOk)==shouldSucceed,
        shouldSucceed ? "valid IComponent setState" : "corrupt state accepted");
}
void stateRoundtrip(const VST3::Hosting::PluginFactory& factory,
                    const VST3::UID& id, FUnknown* host) {
    auto first=instantiate(factory,id,host);
    // Distinct values catch bank/program swapped or ignored in the real plugin.
    require(first.controller->setParamNormalized(bankId,71./127.)==kResultOk,
            "set bank");
    require(first.controller->setParamNormalized(programId,40./127.)==kResultOk,
            "set program");
    require(first.controller->setParamNormalized(masterId,.75)==kResultOk,
            "set master");
    const auto saved=readState(*first.component);
    require(saved[0]=='P'&&saved[1]=='7'&&saved[2]=='S'&&saved[3]=='T',
            "VST3 stream signature");

    // Completely new instance: real component persistence, not codec-only test.
    auto reopened=instantiate(factory,id,host);
    writeState(*reopened.component,saved,true);
    require(closeTo(reopened.controller->getParamNormalized(bankId),71./127.),
            "bank missing after plugin reopen");
    require(closeTo(reopened.controller->getParamNormalized(programId),40./127.),
            "program missing after plugin reopen");
    require(closeTo(reopened.controller->getParamNormalized(masterId),.75),
            "master missing after plugin reopen");
    require(readState(*reopened.component)==saved, "state bytes changed on reopen");

    // Verify the combined controller reload path, too (many hosts call both).
    MemoryStream controllerState;
    int32 written=0;
    require(controllerState.write(const_cast<char*>(saved.data()),16,&written)==kResultOk
            && written==16,"controller state fixture");
    rewind(controllerState);
    require(reopened.controller->setComponentState(&controllerState)==kResultOk,
            "IEditController setComponentState");
    require(readState(*reopened.component)==saved,"controller reload mismatch");

    // Corruption must leave the live plugin state completely untouched.
    auto broken=saved;
    broken[15]^=1;
    writeState(*reopened.component,broken,false);
    require(readState(*reopened.component)==saved,"corrupt state modified plugin");
    std::puts("P007W_REAL_WINDOWS_VST3_TWO_INSTANCE_PROJECT_STATE_REOPEN_PASS");
}

void privateMidiAudio(const VST3::Hosting::PluginFactory& factory,
                      const VST3::UID& id,FUnknown* host,
                      const fs::path& bundle) {
    const auto privateDisk=bundle/"Contents"/"Resources"/"PrivateVM"/"vm"/"win95.img";
    require(fs::is_regular_file(privateDisk),
            "private Win95 HDD absent; refusing synthetic Creative audio PASS");
    auto plugin=instantiate(factory,id,host);
    require(plugin.controller->setParamNormalized(bankId,0.)==kResultOk,"bank 0");
    require(plugin.controller->setParamNormalized(programId,40./127.)==kResultOk,
            "program 40");
    require(plugin.controller->setParamNormalized(masterId,1.)==kResultOk,
            "master unity");

    ProcessSetup setup{};
    setup.processMode=kRealtime;
    setup.symbolicSampleSize=kSample32;
    setup.maxSamplesPerBlock=block;
    setup.sampleRate=44100.0;
    require(plugin.processor->setupProcessing(setup)==kResultOk,
            "private sample setup");
    require(plugin.component->setActive(true)==kResultOk,"private VST3 activation");
    bool processing=false,active=true;
    try {
        require(plugin.processor->setProcessing(true)==kResultOk,
                "private setProcessing");
        processing=true;
        std::array<float,block> left{},right{};
        float* channels[2]={left.data(),right.data()};
        AudioBusBuffers output{};
        output.numChannels=2;
        output.channelBuffers32=channels;
        EventList events;
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;
        data.numSamples=block;
        data.numInputs=0;
        data.numOutputs=1;
        data.outputs=&output;
        data.inputEvents=&events;

        using ReadyFn=int (__cdecl*)();
        ReadyFn ready=nullptr;
        bool sentOn=false,sentOff=false;
        unsigned long long nonzero=0,frames=0;
        double peak=0.;
        const auto start=std::chrono::steady_clock::now();
        auto next=start;
        auto noteAt=start;
        auto lastReport=start;
        while(std::chrono::steady_clock::now()-start < 110s) {
            events.clear();
            if(!ready){
                if(HMODULE core=GetModuleHandleW(L"125A_AWE_Core.dll"))
                    ready=reinterpret_cast<ReadyFn>(
                        GetProcAddress(core,"p007_midi_guest_ready"));
            }
            if(!sentOn && ready && ready()){
                Event e{};
                e.type=Event::kNoteOnEvent;
                e.busIndex=0;
                e.sampleOffset=0;
                e.noteOn.channel=0;
                e.noteOn.pitch=60;
                e.noteOn.velocity=.86f;
                require(events.addEvent(e)==kResultOk,"host MIDI NoteOn");
                sentOn=true;
                noteAt=std::chrono::steady_clock::now();
                std::puts("P007W_REAL_CREATIVE_WINMM_READY_AND_VST3_NOTEON");
            }else if(sentOn && !sentOff &&
                     std::chrono::steady_clock::now()-noteAt>=4s){
                Event e{};
                e.type=Event::kNoteOffEvent;
                e.busIndex=0;
                e.sampleOffset=0;
                e.noteOff.channel=0;
                e.noteOff.pitch=60;
                e.noteOff.velocity=0.f;
                require(events.addEvent(e)==kResultOk,"host MIDI NoteOff");
                sentOff=true;
            }
            require(plugin.processor->process(data)==kResultOk,"real VST3 process");
            if(sentOn){
                frames+=block;
                for(int i=0;i<block;++i){
                    require(std::isfinite(left[i]) && std::isfinite(right[i]),
                            "NaN/Inf in original EMU8000 PCM");
                    const double a=std::max(std::fabs(double(left[i])),
                                            std::fabs(double(right[i])));
                    peak=std::max(peak,a);
                    if(a>0.000001)++nonzero;
                }
            }
            if(sentOff && std::chrono::steady_clock::now()-noteAt>=5s &&
               nonzero>=128 && peak>=0.00001) break;
            if(std::chrono::steady_clock::now()-lastReport >= 10s){
                std::printf("P007W_PRIVATE_HOST_PROGRESS ready=%d frames=%llu nonzero=%llu peak=%.7f\n",
                    ready?ready():0,frames,nonzero,peak);
                std::fflush(stdout);
                lastReport=std::chrono::steady_clock::now();
            }
            next+=std::chrono::microseconds(23220);
            std::this_thread::sleep_until(next);
        }
        require(sentOn&&sentOff&&nonzero>=128&&peak>=0.00001,
                "no real private Win95 Creative MIDI-to-PCM proof");
        std::printf("P007W_PRIVATE_REAL_VST3_ORIGINAL_CREATIVE_MIDI_PCM_PASS"
                    " frames=%llu nonzero=%llu peak=%.8f\n",frames,nonzero,peak);
    }catch(...){
        if(processing)(void)plugin.processor->setProcessing(false);
        if(active)(void)plugin.component->setActive(false);
        throw;
    }
    if(processing)(void)plugin.processor->setProcessing(false);
    if(active)(void)plugin.component->setActive(false);
}
} // namespace

int main(int argc,char** argv) {
    try{
        require(argc==2 || (argc==3 && std::strcmp(argv[2],"--private-audio")==0),
                "usage: P007ActualHostQA.exe <AWE64_Gold_P007.vst3 folder> [--private-audio]");
        const fs::path bundle=fs::absolute(argv[1]);
        require(fs::is_directory(bundle),"actual VST3 bundle not found");
        std::string error;
        auto module=VST3::Hosting::Module::create(bundle.string(),error);
        if(!module)std::fprintf(stderr,"P007W_MODULE_ERROR %s\n",error.c_str());
        require(bool(module),"Steinberg Module cannot load actual Windows VST3");
        auto const& factory=module->getFactory();
        auto classes=factory.classInfos();
        const auto choice=std::find_if(classes.begin(),classes.end(),
            [](auto const& c){return c.category()==kVstAudioEffectClass;});
        require(choice!=classes.end(),"no instrument IComponent in real VST3");
        auto host=owned(new HostApplication());
        stateRoundtrip(factory,choice->ID(),host.get());
        if(argc==3) privateMidiAudio(factory,choice->ID(),host.get(),bundle);
        std::puts("P007W_ACTUAL_WINDOWS_VST3_HOST_QA_PASS");
        return 0;
    }catch(const std::exception& ex){
        std::fprintf(stderr,"P007W_ACTUAL_WINDOWS_VST3_HOST_QA_FAIL %s\n",ex.what());
        return 1;
    }
}
