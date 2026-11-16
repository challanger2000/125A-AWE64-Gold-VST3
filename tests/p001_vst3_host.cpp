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
#include "public.sdk/source/common/memorystream.h"
#include "P003VstBankState.h"
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
        for(const double rate:{48000.0,88200.0,96000.0}) {
            ProcessSetup supported{kRealtime,kSample32,512,rate};
            require(audio->setupProcessing(supported)==kResultOk,
                    "supported P004 host rate rejected by real DLL");
        }
        for(const double rate:{32000.0,192000.0}) {
            ProcessSetup unsupported{kRealtime,kSample32,512,rate};
            require(audio->setupProcessing(unsupported)!=kResultOk,
                    "unsupported P004 host rate accepted");
        }
        require(audio->setupProcessing(good)==kResultOk,"restoring 44.1k");
        require(audio->getLatencySamples()==0u,
                "44.1k path incorrectly reports resampler latency");
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
void verifyExportedVst3ProjectState(IPluginFactory* factory) {
    // Test the REAL exported DLL, not only the standalone codec.
    // No private SF2/driver/ROM data is serialized into project state.
    Instance synth(factory);
    MemoryStream empty{};
    require(synth.component->getState(&empty)==kResultOk,
            "initial P001 getState failed");
    require(empty.getSize()==8u &&
            std::memcmp(empty.getData(),
              awe64::vst3::state::kLegacyMagic.data(),8u)==0,
            "legacy 8-byte P001 project state changed");

    using namespace awe64::vst3::state;
    auto wanted=encode(BankSelection{128u,67u,99u,22u,120u});
    MemoryStream incoming(wanted.data(),wanted.size());
    require(synth.component->setState(&incoming)==kResultOk,
            "exported VST3 rejected valid versioned state");
    MemoryStream saved{};
    require(synth.component->getState(&saved)==kResultOk &&
            saved.getSize()==kStateSize &&
            std::memcmp(saved.getData(),wanted.data(),kStateSize)==0,
            "exported VST3 lost restored SF2 browser/CC snapshot");

    auto mutation=wanted;
    mutation[12]^=1u; // CC10 data changed without CRC
    MemoryStream bad(mutation.data(),mutation.size());
    require(synth.component->setState(&bad)!=kResultOk,
            "exported VST3 accepted corrupt CRC");
    MemoryStream afterBad{};
    require(synth.component->getState(&afterBad)==kResultOk &&
            afterBad.getSize()==kStateSize &&
            std::memcmp(afterBad.getData(),wanted.data(),kStateSize)==0,
            "corrupt state unexpectedly modified the live snapshot");

    MemoryStream truncated(wanted.data(),wanted.size()-1u);
    require(synth.component->setState(&truncated)!=kResultOk,
            "truncated VST3 project state accepted");
    // The existing 8-byte P001 sessions remain readable and writable.
    auto legacy=kLegacyMagic;
    MemoryStream oldProject(legacy.data(),legacy.size());
    require(synth.component->setState(&oldProject)==kResultOk,
            "legacy P001 project restore rejected");
    MemoryStream afterLegacy{};
    require(synth.component->getState(&afterLegacy)==kResultOk &&
            afterLegacy.getSize()==legacy.size() &&
            std::memcmp(afterLegacy.getData(),legacy.data(),
                        legacy.size())==0,
            "legacy P001 project state round trip changed");
}
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
    // Required VST3 host contract: a legal zero-frame flush may omit
    // every audio buffer. The unchanged original EMU8000 sample clock
    // cannot advance. Ordinary P001 PCM tests below still compare
    // the same exact note-on offset and chip-generated samples.
    ProcessData flush{};
    flush.processMode=kRealtime;
    flush.symbolicSampleSize=kSample32;
    flush.numSamples=0;
    flush.numOutputs=0;
    flush.outputs=nullptr;
    require(synth.audio->process(flush)==kResultOk,
            "exported VST3 refused zero-frame host flush");
    // A late invalid MIDI event must be detected BEFORE the early
    // NoteOn starts the genuine EMU8000 chip. Verify strict rejection
    // with no consumed samples or partial output/voice state.
    {
        std::array<float,512> left{},right{};
        float* buffers[]{left.data(),right.data()};
        AudioBusBuffers bus{};
        bus.numChannels=2;bus.channelBuffers32=buffers;
        EventList malformed;
        event(malformed,true,137);
        event(malformed,false,513);
        ProcessData invalid{};
        invalid.processMode=kRealtime;
        invalid.symbolicSampleSize=kSample32;
        invalid.numSamples=512;
        invalid.numOutputs=1;invalid.outputs=&bus;
        invalid.inputEvents=&malformed;
        require(synth.audio->process(invalid)!=kResultOk,
                "late malformed VST3 MIDI offset was accepted");
        require(std::all_of(left.begin(),left.end(),
                            [](float s){return s==0.f;}) &&
                std::all_of(right.begin(),right.end(),
                            [](float s){return s==0.f;}),
                "late invalid VST3 MIDI event partially rendered PCM");
    }
    // Real exported VST3: structurally invalid NoteOff is not an
    // ordinary unmatched note. A late invalid semantic payload must
    // reject the WHOLE block even if a valid NoteOn was earlier.
    {
        std::array<float,512> left{},right{};
        float* buffers[]{left.data(),right.data()};
        AudioBusBuffers bus{};
        bus.numChannels=2;bus.channelBuffers32=buffers;
        EventList events;
        event(events,true,137);
        Event malformed{};
        malformed.type=Event::kNoteOffEvent;
        malformed.busIndex=0;
        malformed.sampleOffset=255;
        malformed.noteOff.channel=0;
        malformed.noteOff.pitch=128; // invalid note pitch
        malformed.noteOff.velocity=0.f;
        malformed.noteOff.noteId=60;
        require(events.addEvent(malformed)==kResultOk,
                "invalid semantic NoteOff enqueue failed");
        ProcessData invalid{};
        invalid.processMode=kRealtime;invalid.symbolicSampleSize=kSample32;
        invalid.numSamples=512;invalid.numOutputs=1;invalid.outputs=&bus;
        invalid.inputEvents=&events;
        require(synth.audio->process(invalid)!=kResultOk,
                "late invalid NoteOff accepted after valid NoteOn");
        require(std::all_of(left.begin(),left.end(),
                            [](float x){return x==0.f;}) &&
                std::all_of(right.begin(),right.end(),
                            [](float x){return x==0.f;}),
                "invalid NoteOff left partially rendered chip PCM");
    }
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
// P004: exported DLL at 48/88.2/96 kHz, with REAL ROM NoteOn/Off.
// All event offsets are HOST-rate frames, but chip rendering stays
// at 44100 frames/second and never runs ahead of future MIDI events.
// Partition-invariance catches resampler resets at process() boundaries.
std::vector<float> renderActualHostRate(IPluginFactory* factory,
                                        std::uint32_t hostHz,
                                        int blockFrames) {
    Instance synth(factory);
    ProcessSetup setup{kRealtime,kSample32,512,double(hostHz)};
    require(synth.audio->setupProcessing(setup)==kResultOk,
            "real VST3 rejected P004 host sample rate");
    const auto expectedLatency=
        (24u*hostHz+44099u)/44100u;
    require(synth.audio->getLatencySamples()==expectedLatency,
            "real DLL reported wrong host-resampler latency");
    synth.activate();
    constexpr int total=512*36;
    constexpr int noteOnAt=137;
    constexpr int noteOffAt=512*2+127;
    std::vector<float> output;
    output.reserve(total*2u);
    for(int start=0;start<total;) {
        const int n=std::min(blockFrames,total-start);
        std::array<float,512> left{},right{};
        float* buffers[]{left.data(),right.data()};
        AudioBusBuffers bus{};
        bus.numChannels=2;bus.channelBuffers32=buffers;
        EventList events;
        if(noteOnAt>=start && noteOnAt<start+n)
            event(events,true,noteOnAt-start);
        if(noteOffAt>=start && noteOffAt<start+n)
            event(events,false,noteOffAt-start);
        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
        data.numSamples=n;data.numOutputs=1;data.outputs=&bus;
        data.inputEvents=&events;
        require(synth.audio->process(data)==kResultOk,
                "real VST3 host-rate callback failed");
        for(int i=0;i<n;++i) {
            require(std::isfinite(left[i]) && std::isfinite(right[i]),
                    "real host-rate output not finite");
            output.push_back(left[i]);
            output.push_back(right[i]);
        }
        start+=n;
    }
    require(output.size()==total*2u,"host-rate stereo frame count");
    const auto before=output.begin()+noteOnAt*2;
    require(std::all_of(output.begin(),before,
            [](float sample){return sample==0.f;}),
            "output present before host-rate NoteOn");
    require(std::count_if(before,output.end(),
            [](float sample){return std::abs(sample)>0.00001f;})>1000,
            "real host-rate output stayed silent after ROM NoteOn");
    return output;
}
void verifyActualMultiRateHost(IPluginFactory* factory) {
    for(const auto rate:{48000u,88200u,96000u}) {
        const auto blocks512=renderActualHostRate(factory,rate,512);
        const auto blocks128=renderActualHostRate(factory,rate,128);
        require(blocks512==blocks128,
                "host-rate PCM depends on DAW audio callback partition");
    }
    std::cout<<"P004_ACTUAL_EXPORTED_VST3_MULTI_RATE_PASS"
             <<" rates=48000,88200,96000"
             <<" original_44100_chip_clock=UNCHANGED"
             <<" two_callback_partitions_bitidentical=TRUE"
             <<" host_latency_reported=TRUE"
             <<" original_win95_parity=NOT_PROVEN\n";
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
        verifyExportedVst3ProjectState(factory);
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
        verifyActualMultiRateHost(factory);
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
