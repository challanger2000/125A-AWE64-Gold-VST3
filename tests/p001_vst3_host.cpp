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
#include <cstdlib>
#include <cstdint>
#include <filesystem>
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
std::vector<float> play(IPluginFactory* factory,bool release,
                        std::vector<std::int16_t>* capturedStereo=nullptr) {
    Instance synth(factory);
    synth.activate();
    std::vector<float> output;
    output.reserve(512u*36u);
    if(capturedStereo) {
        capturedStereo->clear();
        capturedStereo->reserve(512u*36u*2u);
    }
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
        for(int i=0;i<512;++i) {
            require(std::isfinite(left[i])&&std::isfinite(right[i]),"nonfinite output");
            if(capturedStereo) {
                // This is the opted-in ORIGINAL pre-card PCM16 tap:
                // exactly signed PCM16 / 32768.0f. A conversion with
                // saturation, interpolation or rounding must NOT hide
                // a changed source signal.
                for(const float value:{left[i],right[i]}) {
                    require(value>=-1.f && value<1.f,
                            "pre-card PCM16 tap exceeded valid signed range");
                    const float scaled=value*32768.f;
                    const auto word=static_cast<std::int16_t>(scaled);
                    require(static_cast<float>(word)==scaled,
                            "VST3 pre-card float not exactly signed PCM16");
                    capturedStereo->push_back(word);
                }
            }
        }
        output.insert(output.end(),left.begin(),left.end());
    }
    return output;
}
// The capture is OFFLINE HOST-SIDE only, after every realtime callback.
// No DAW thread I/O, allocation, source-core change, normalization,
// guessed Creative RAM bank, or decoding of unverified private inputs.
void writeCapturedPrecardWav(const std::filesystem::path& destination,
                             const std::vector<std::int16_t>& stereo) {
    require(stereo.size()==512u*36u*2u && (stereo.size()&1u)==0u,
            "VST3 pre-card capture frame count incorrect");
    const std::uint32_t audioBytes=
        static_cast<std::uint32_t>(stereo.size()*sizeof(std::int16_t));
    std::array<std::uint8_t,44> hdr{};
    const auto label=[&](unsigned i,const char* text) {
        for(unsigned n=0;n<4u;++n)hdr[i+n]=static_cast<std::uint8_t>(text[n]);
    };
    const auto u16=[&](unsigned i,std::uint16_t value) {
        hdr[i]=static_cast<std::uint8_t>(value);
        hdr[i+1u]=static_cast<std::uint8_t>(value>>8u);
    };
    const auto u32=[&](unsigned i,std::uint32_t value) {
        for(unsigned n=0;n<4u;++n)
            hdr[i+n]=static_cast<std::uint8_t>(value>>(8u*n));
    };
    label(0,"RIFF");u32(4,36u+audioBytes);label(8,"WAVE");
    label(12,"fmt ");u32(16,16u);u16(20,1u);u16(22,2u);
    u32(24,44100u);u32(28,44100u*4u);u16(32,4u);u16(34,16u);
    label(36,"data");u32(40,audioBytes);
    // CREATE_NEW guarantees no private evidence WAV can be overwritten.
    const auto path=destination.wstring();
    const HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,
                                  CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(file!=INVALID_HANDLE_VALUE,
            "private pre-card PCM16 WAV destination exists or inaccessible");
    DWORD written=0u;
    const bool ok=WriteFile(file,hdr.data(),static_cast<DWORD>(hdr.size()),
                            &written,nullptr)!=0 && written==hdr.size() &&
                  WriteFile(file,stereo.data(),audioBytes,&written,nullptr)!=0 &&
                  written==audioBytes && FlushFileBuffers(file)!=0;
    CloseHandle(file);
    if(!ok) {
        DeleteFileW(path.c_str());
        throw std::runtime_error("failed to write exact private PCM16 WAV");
    }
}
}
int wmain(int argc,wchar_t** argv){
    // Preserve the independent pre-CT1745 AWE32_WAV PCM16 tap for
    // all historical original-chip reference and RT heap regressions.
    // Normal VST3 instances without this private flag use full CT1745.
    if(_wputenv_s(L"AWE64_P002_PRE_CARD_PCM_DIAGNOSTIC",L"1")!=0)return 87;
    if(argc!=3 && argc!=4)return 2;
    HMODULE module=nullptr;
    IPluginFactory* factory=nullptr;
    try{
        require(_wputenv_s(L"AWE64_P001_ROM_PATH",argv[2])==0,
                "cannot configure private original ROM");
        module=LoadLibraryW(argv[1]);require(module!=nullptr,"VST3 LoadLibraryW failed");
        auto get=reinterpret_cast<IPluginFactory*(*)()>(
            GetProcAddress(module,"GetPluginFactory"));
        require(get!=nullptr,"actual DLL has no GetPluginFactory");
        factory=get();require(factory!=nullptr,"null factory");
        std::vector<std::int16_t> privatePrecardStereo{};
        auto held=play(factory,false,argc==4?&privatePrecardStereo:nullptr);
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
        // Independently invoke the SAME actual exported Windows VST3
        // again with its real AWE64 Gold CT1745 post-chip mixer path.
        // Compare every unclipped pre-card sample to the ORIGINAL
        // pinned 86Box reset: fm/master 32767/32768 each, int32
        // truncation, float sink /32768 WITHOUT PCM16 saturation.
        require(_wputenv_s(
            L"AWE64_P002_PRE_CARD_PCM_DIAGNOSTIC",L"")==0,
            "cannot switch exported VST3 to full original CT1745 output");
        auto full=play(factory,false);
        auto fullOff=play(factory,true);
        require(full.size()==held.size() && fullOff.size()==off.size(),
                "full AWE64 card path frame count differs from diagnostic");
        std::size_t verified=0u,different=0u;
        constexpr double sourceAtt=32767.0/32768.0;
        for(std::size_t i=0;i<held.size();++i) {
            if(std::fabs(held[i])>=0.999f ||
               std::fabs(off[i])>=0.999f)continue; // either PCM tap clipped
            const auto originalChip=static_cast<std::int32_t>(held[i]*32768.f);
            const auto post=static_cast<std::int32_t>(
                (static_cast<double>(originalChip)*sourceAtt)*sourceAtt);
            const float expected=static_cast<float>(post)/32768.f;
            require(full[i]==expected,
                    "actual exported AWE64 Gold CT1745 default float samples differ from fork");
            const auto originalOff=static_cast<std::int32_t>(off[i]*32768.f);
            const auto postOff=static_cast<std::int32_t>(
                (static_cast<double>(originalOff)*sourceAtt)*sourceAtt);
            require(fullOff[i]==static_cast<float>(postOff)/32768.f,
                    "AWE64 CT1745 noteoff vector differs from fork");
            ++verified;
            if(full[i]!=held[i])++different;
        }
        require(verified>5000 && different>100,
                "full CT1745 VST3 was not independently exercised");
        if(argc==4) {
            writeCapturedPrecardWav(argv[3],privatePrecardStereo);
            std::cout<<"P002_ACTUAL_EXPORTED_VST3_PRECARD_PCM16_WAV_WRITTEN"
                     <<" original_driver_RAM_SF2_parity=NOT_PROVEN"
                     <<" sample_rate=44100 channels=2 signed_pcm_bits=16"
                     <<" frames="<<privatePrecardStereo.size()/2u
                     <<" note_on_frame=137"
                     <<" gain_normalization=NONE"
                     <<" wrote_outside_audio_callback=TRUE\n";
        }
        factory->release();factory=nullptr;
        FreeLibrary(module);module=nullptr;
        std::cout<<"P001_REAL_VST3_HOST_MIDI_PASS frames="<<held.size()
                 <<" noteon=137 noteoff="<<noteoffFrame<<" nonzero="<<nonzero
                 <<" full_awe64_ct1745_fork_samples_verified="<<verified
                 <<" original_card_mixer_output_samples_changed="<<different
                 <<" no_double_emu8000_DSP=TRUE pre_card_tap_preserved=TRUE\n";
        return 0;
    }catch(const std::exception& e) {
        if(factory)factory->release();
        if(module)FreeLibrary(module);
        std::cerr<<"P001_REAL_VST3_HOST_MIDI_FAIL "<<e.what()<<'\n';
        return 1;
    }
}
