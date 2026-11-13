// Independent Windows host tests the ACTUAL exported VST3 DLL in opt-in
// P002 source-diagnostic mode, with a separate genuine 86Box ROM.
// The fixture is synthetic original-x86-GM0-derived CHANNEL STATE;
// NOT a private original Win95 runtime capture, NOT full bank support.
// It intentionally tests the SAME real P002 note engine, not fixed P001.
// Private Creative ROM is downloaded by CI from pinned upstream revision,
// validated by SHA, and deleted; none is shipped in the public repo.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef AWE64_P002_RT_HEAP_AUDIT
#include "MinHook.h"
#endif
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
void require(const bool p,const char* msg){if(!p)throw std::runtime_error(msg);}
#ifdef AWE64_P002_RT_HEAP_AUDIT
using RtlAllocateHeapFn=PVOID (NTAPI*)(PVOID,ULONG,SIZE_T);
RtlAllocateHeapFn originalAllocateHeap=nullptr;
thread_local bool inOriginalBankCallback=false;
thread_local std::uint64_t originalBankAllocations=0u;
thread_local std::uint64_t originalBankAllocatedBytes=0u;
PVOID NTAPI monitoredAllocateHeap(PVOID heap,ULONG flags,SIZE_T bytes) {
    if(inOriginalBankCallback) {
        ++originalBankAllocations;
        originalBankAllocatedBytes+=bytes;
    }
    return originalAllocateHeap(heap,flags,bytes);
}
class P002RuntimeHeapMonitor {
public:
    P002RuntimeHeapMonitor() {
        require(MH_Initialize()==MH_OK,"P002 MinHook init failure");
        const auto result=MH_CreateHookApi(
            L"ntdll.dll","RtlAllocateHeap",
            reinterpret_cast<LPVOID>(&monitoredAllocateHeap),
            reinterpret_cast<LPVOID*>(&originalAllocateHeap));
        require(result==MH_OK && originalAllocateHeap,
                "P002 RtlAllocateHeap hook absent");
        require(MH_EnableHook(MH_ALL_HOOKS)==MH_OK,
                "P002 RT allocation instrumentation inactive");
        const auto before=originalBankAllocations;
        inOriginalBankCallback=true;
        void* p=HeapAlloc(GetProcessHeap(),0,64u);
        inOriginalBankCallback=false;
        require(p!=nullptr,"P002 RT hook control allocation failed");
        HeapFree(GetProcessHeap(),0,p);
        require(originalBankAllocations>before &&
                originalBankAllocatedBytes>=64u,
                "P002 RT allocation instrumentation positive control failed");
        originalBankAllocations=0u;
        originalBankAllocatedBytes=0u;
    }
    ~P002RuntimeHeapMonitor() {
        inOriginalBankCallback=false;
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
    }
    P002RuntimeHeapMonitor(const P002RuntimeHeapMonitor&)=delete;
    P002RuntimeHeapMonitor& operator=(const P002RuntimeHeapMonitor&)=delete;
};
#endif
tresult auditedOriginalBankProcess(
    IAudioProcessor* processor,ProcessData& data) {
#ifdef AWE64_P002_RT_HEAP_AUDIT
    const auto before=originalBankAllocations;
    inOriginalBankCallback=true;
    const auto result=processor->process(data);
    inOriginalBankCallback=false;
    require(originalBankAllocations==before,
            "P002 exported ROM-SF2 bank process() allocated realtime heap");
    return result;
#else
    return processor->process(data);
#endif
}

void word(std::array<std::uint8_t,148>& b,unsigned id,int v){
    const auto w=static_cast<std::uint16_t>(v);
    b[2u*id]=static_cast<std::uint8_t>(w);
    b[2u*id+1u]=static_cast<std::uint8_t>(w>>8u);
}
void dword(std::array<std::uint8_t,148>& b,unsigned offset,std::uint32_t n){
    for(unsigned i=0;i<4u;++i)b[offset+i]=static_cast<std::uint8_t>(n>>(8u*i));
}
std::array<std::uint8_t,148> realSourceDerivedGm0() {
    std::array<std::uint8_t,148> b{};
    b[0x8cu]=1u;b[0x88u]=0xe0u;b[0x8au]=60u;
    word(b,58,77);word(b,56,100);word(b,52,4);
    word(b,8,179);word(b,19,58);
    word(b,34,125);word(b,35,120);
    word(b,26,125);word(b,27,127);word(b,28,127);word(b,29,0);
    word(b,21,static_cast<std::int16_t>(0x8000u));
    word(b,23,static_cast<std::int16_t>(0x8000u));
    word(b,25,static_cast<std::int16_t>(0x8000u));
    word(b,33,static_cast<std::int16_t>(0x8000u));
    word(b,22,128);word(b,54,1);word(b,15,0);word(b,16,27);
    word(b,17,127);word(b,18,129);word(b,36,3);word(b,37,0);
    word(b,30,127);word(b,38,41); // actual private GM0 x86 handoff
    dword(b,0x76,0xfeb9u);dword(b,0x7a,0x152feu);
    dword(b,0x7e,0x152bbu);dword(b,0x82,0x152fau);
    return b;
}
void writeDiagFixture(const std::filesystem::path& p,bool badSha,
                      bool releaseV2=false,bool sustain=false,
                      bool wrongOriginalMarker=false,
                      bool sourceDerivedV3=false,
                      bool twoNoteV4=false,bool duplicateSecondVoice=false,
                      unsigned extraNotesV5=0u,
                      bool duplicateExtraKey=false,
                      bool duplicateExtraVoice=false){
    std::ofstream o(p,std::ios::binary|std::ios::trunc);
    require(bool(o),"cannot open synthetic P002 host fixture");
    auto raw=[&](std::uint8_t b){o.put(static_cast<char>(b));};
    for(const char c:std::string("125AP2D1"))o.put(c);
    const std::array<std::uint8_t,6> head{{
        static_cast<std::uint8_t>(extraNotesV5?5u:twoNoteV4?4u:sourceDerivedV3?3u:releaseV2?2u:1u),
        0u,0u,60u,90u,1u}};
    for(const auto v:head)raw(v);
    std::array<std::uint8_t,32> vxd{{
        0xd3,0x43,0xf2,0x42,0x8a,0xb3,0xe5,0x93,
        0xc9,0xe4,0x2a,0x66,0x8c,0xf5,0x21,0xb7,
        0x80,0x1e,0x12,0x39,0x79,0x2e,0xaf,0x7f,
        0x2b,0x11,0xb5,0xf1,0x79,0xbc,0x7c,0x9a
    }};
    constexpr std::array<std::uint8_t,32> bank{{
        0x78,0xf8,0x73,0x9a,0x25,0xd4,0x9e,0x51,
        0x9b,0x3f,0xdc,0x92,0xa7,0xf0,0x3d,0x4d,
        0xdd,0x68,0x58,0x8f,0xd0,0x26,0x05,0x78,
        0xa8,0x80,0x29,0x6e,0x30,0x4b,0x7b,0x5f
    }};
    if(badSha)vxd[0]^=1u; // mutation rejects provenance
    for(const auto v:vxd)raw(v);
    for(const auto v:bank)raw(v);
    std::array<std::uint8_t,0x1592u> state{};
    state[0x158eu]=0x31u;state[0x158fu]=0x4du;
    state[0x1590u]=0x47u;state[0x1591u]=0x4du; // 1MGM
    state[0x442u+4u]=sustain?1u:0u; // CHANNEL sustain original LCOD 60B4
    if(releaseV2) {
        // Entirely synthetic but source-shaped *original* 32-byte
        // physical voice record in the real Win95 VXD state layout.
        // These are encoded release rates observed in the genuine
        // LCOD+0x60E4 x86 differential, not guessed ADSR parameters.
        constexpr unsigned at=0x42u;
        state[at]=wrongOriginalMarker?61u:60u;
        state[at+1u]=0u; // physical voice0 MIDI channel0
        state[at+4u]=1u; // voice+4 is NOT sustain; independent gen48 word
        state[at+0x15u]=127u; // original GM0 mod-release (gen30)
        state[at+0x16u]=41u; // original GM0 vol-release (gen38)
        // +0x06 DWORD retained end 0 for this GM0 ROM mode; the
        // nonzero path and genuine CSL/PSST reads have native tests.
    }
    for(const auto v:state)raw(v);
    for(int i=0;i<62;++i)raw(0u); // unused for GM0 attack 125
    raw(0u); // physical voice0, NOT the fixed P001 voice29
    for(const auto v:realSourceDerivedGm0())raw(v);
    if(twoNoteV4 || extraNotesV5>0u) {
        // Exact second fixture record (no new driver template):
        // independently selected original physical voice 3 and MIDI
        // key64, same source GM0 148-byte descriptor family. The
        // second note is NOT started during fixture parsing.
        raw(64u);raw(90u);raw(1u);
        raw(duplicateSecondVoice?0u:3u);
        auto second=realSourceDerivedGm0();
        word(second,46u,64); // explicit second MIDI key in source-shaped case
        for(const auto v:second)raw(v);
    }
    if(extraNotesV5>0u) {
        // v5 extends the prior authenticated two-note fixture with
        // independently source-backed additional groups. Each one
        // owns its original physical voice and NoteOn descriptor.
        require(extraNotesV5<=6u,"too many synthetic v5 note groups");
        raw(static_cast<std::uint8_t>(extraNotesV5));
        constexpr std::array<std::uint8_t,6> keys{{67,69,72,74,76,79}};
        constexpr std::array<std::uint8_t,6> physical{{6,9,12,15,18,21}};
        for(unsigned i=0;i<extraNotesV5;++i) {
            const auto key=static_cast<std::uint8_t>(
                duplicateExtraKey && i+1u==extraNotesV5?64u:keys[i]);
            raw(key);raw(90u);raw(1u);
            raw(duplicateExtraVoice && i+1u==extraNotesV5
                ?3u:physical[i]);
            auto desc=realSourceDerivedGm0();
            word(desc,46u,key);
            for(const auto v:desc)raw(v);
        }
    }
    require(bool(o),"synthetic diagnostic fixture truncated");
}
struct Instance {
    IComponent* component=nullptr;
    IAudioProcessor* audio=nullptr;
    explicit Instance(IPluginFactory* factory,bool expectActivation){
        PClassInfo info{};
        bool found=false;
        for(int32 i=0;i<factory->countClasses();++i){
            require(factory->getClassInfo(i,&info)==kResultOk,"factory info");
            if(std::strcmp(info.category,kVstAudioEffectClass)==0){found=true;break;}
        }
        require(found,"exported instrument class not found");
        require(factory->createInstance(info.cid,IComponent::iid,
            reinterpret_cast<void**>(&component))==kResultOk && component,
            "real exported VST3 instantiation failed");
        require(component->initialize(nullptr)==kResultOk,"component init failed");
        require(component->queryInterface(IAudioProcessor::iid,
            reinterpret_cast<void**>(&audio))==kResultOk && audio,
            "VST3 audio interface absent");
        SpeakerArrangement stereo=SpeakerArr::kStereo;
        require(audio->setBusArrangements(nullptr,0,&stereo,1)==kResultTrue,
                "stereo bus negotiation");
        ProcessSetup setup{kRealtime,kSample32,512,44100.};
        require(audio->setupProcessing(setup)==kResultOk,"44.1k setup failed");
        const bool activated=component->setActive(true)==kResultOk;
        if(expectActivation){
            require(activated,"source-backed diagnostic activation failed");
            require(audio->setProcessing(true)==kResultOk,
                "diagnostic setProcessing failed");
        }else require(!activated,"bad private provenance unexpectedly accepted");
    }
    ~Instance(){
        if(audio){audio->setProcessing(false);audio->release();}
        if(component){component->setActive(false);component->terminate();component->release();}
    }
};
void addNoteOffEvent(EventList& list,int channel,int pitch) {
    Event e{};
    e.type=Event::kNoteOffEvent;e.busIndex=0;e.sampleOffset=101;
    e.noteOff.channel=static_cast<Steinberg::int16>(channel);
    e.noteOff.pitch=static_cast<Steinberg::int16>(pitch);
    e.noteOff.velocity=0.f;
    e.noteOff.noteId=1;
    require(list.addEvent(e)==kResultOk,"host note-off event");
}
void addEvent(EventList& list,float vel) {
    Event e{};
    e.type=Event::kNoteOnEvent;e.busIndex=0;e.sampleOffset=137;
    e.noteOn.channel=0;e.noteOn.pitch=60;
    e.noteOn.velocity=vel;e.noteOn.noteId=1;
    require(list.addEvent(e)==kResultOk,"host note on event");
}
struct Audio {
    std::uint64_t nonzero=0;
    std::uint64_t hash=0xcbf29ce484222325ull;
    std::uint64_t hashBeforeBlock12=0xcbf29ce484222325ull;
    std::uint64_t hashBeforeBlock2=0xcbf29ce484222325ull;
    std::uint64_t hashBeforeBlock13=0xcbf29ce484222325ull;
    std::uint64_t hashBeforeBlock22=0xcbf29ce484222325ull;
};
Audio renderNote(IPluginFactory* factory,float velocity,
                 int noteOffBlock=-1,int noteOffChannel=0,
                 int noteOffPitch=60,int pedalDownBlock=-1,
                 int pedalUpBlock=-1){
    Instance instance(factory,true);
    Audio audio{};
    for(int block=0;block<30;++block){
        std::array<float,512> left{},right{};
        float* buf[2]{left.data(),right.data()};
        AudioBusBuffers bus{};
        bus.numChannels=2;bus.channelBuffers32=buf;
        EventList list;
        if(block==0)addEvent(list,velocity);
        if(block==noteOffBlock)
            addNoteOffEvent(list,noteOffChannel,noteOffPitch);
        if(block==12)audio.hashBeforeBlock12=audio.hash;
        ParameterChanges changes(1);
        if(block==pedalDownBlock || block==pedalUpBlock) {
            int32 idx=0;
            auto* queue=changes.addParameterData(0x125A0064u,idx);
            require(queue!=nullptr,"CC64 parameter event queue absent");
            int32 pointIndex=0;
            require(queue->addPoint(block==pedalDownBlock?73:37,
                                   block==pedalDownBlock?1.:0.,
                                   pointIndex)==kResultOk,
                    "actual VST3 CC64 point not accepted");
        }
        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
        data.numSamples=512;data.outputs=&bus;data.numOutputs=1;
        data.inputEvents=&list;
        data.inputParameterChanges=&changes;
        require(auditedOriginalBankProcess(instance.audio,data)==kResultOk,
                "actual exported VST3 source-DSP process failed");
        for(int i=0;i<512;++i){
            require(std::isfinite(left[i])&&std::isfinite(right[i]),
                    "nonfinite digital output");
            if(block==0 && i<137)
                require(left[i]==0.f && right[i]==0.f,
                        "original NoteOn MIDI sample offset violated");
            for(const auto x:{left[i],right[i]}){
                if(x!=0.f)++audio.nonzero;
                std::uint32_t bits=0;
                std::memcpy(&bits,&x,sizeof(bits));
                for(unsigned b=0;b<4u;++b){
                    audio.hash^=(bits>>(8u*b))&0xffu;
                    audio.hash*=0x100000001b3ull;
                }
            }
        }
    }
    return audio;
}
// Real exported VST3 multi-key diagnostic, with exactly two distinct
// independently fixture-verified SF2 NoteOn descriptors/physical voices.
// Not a production bank loader or original Windows95 PCM oracle.
Audio renderTwoSourceNotes(IPluginFactory* factory,bool sendSecond,
                           bool liftPedal,bool wrongSecondVelocity=false) {
    Instance instance(factory,true);
    Audio audio{};
    auto enqueue=[&](EventList& list,bool on,int key,int offset,float vel){
        Event e{};
        e.busIndex=0;e.sampleOffset=offset;
        e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;
        if(on) {
            e.noteOn.channel=0;
            e.noteOn.pitch=static_cast<Steinberg::int16>(key);
            e.noteOn.velocity=vel;
            e.noteOn.noteId=key;
        }else {
            e.noteOff.channel=0;
            e.noteOff.pitch=static_cast<Steinberg::int16>(key);
            e.noteOff.velocity=0.f;
            e.noteOff.noteId=key;
        }
        require(list.addEvent(e)==kResultOk,"two-note VST3 MIDI enqueue");
    };
    for(int block=0;block<32;++block) {
        std::array<float,512> left{},right{};
        float* buf[]{left.data(),right.data()};
        AudioBusBuffers bus{};
        bus.numChannels=2;bus.channelBuffers32=buf;
        EventList events;
        if(block==0)enqueue(events,true,60,137,90.f/127.f);
        if(block==2 && sendSecond)
            enqueue(events,true,64,77,
                    wrongSecondVelocity?75.f/127.f:90.f/127.f);
        if(block==8)enqueue(events,false,60,101,0.f);
        if(block==9 && sendSecond)
            enqueue(events,false,64,91,0.f);
        ParameterChanges changes(1);
        if(block==5 || (block==13 && liftPedal)) {
            int32 index=0;
            auto* queue=changes.addParameterData(0x125A0064u,index);
            require(queue!=nullptr,"two-note CC64 input queue");
            int32 point=0;
            require(queue->addPoint(block==5?73:37,
                                    block==5?1.:0.,point)==kResultOk,
                    "two-note CC64 point");
        }
        if(block==2)audio.hashBeforeBlock2=audio.hash;
        if(block==13)audio.hashBeforeBlock13=audio.hash;
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;
        data.numSamples=512;
        data.outputs=&bus;data.numOutputs=1;
        data.inputEvents=&events;
        data.inputParameterChanges=&changes;
        require(auditedOriginalBankProcess(instance.audio,data)==kResultOk,
                "actual VST3 dual-key process failed");
        for(unsigned i=0;i<512u;++i)
            for(const auto value:{left[i],right[i]}) {
                require(std::isfinite(value),"dual-key PCM invalid");
                if(value!=0.f)++audio.nonzero;
                std::uint32_t bits=0;
                std::memcpy(&bits,&value,sizeof bits);
                for(unsigned byte=0;byte<4u;++byte) {
                    audio.hash^=(bits>>(byte*8u))&255u;
                    audio.hash*=0x100000001b3ull;
                }
            }
    }
    return audio;
}
// First exported real VST3 eight-source-note exercise. This is a
// bounded externally authenticated source-note table, not arbitrary
// soundbank key mapping, voice stealing or original Win95 PCM parity.
Audio renderManySourceNotes(IPluginFactory* factory,
                            const unsigned count,
                            const bool liftPedal,
                            const bool badThirdVelocity=false) {
    require(count>0u && count<=8u,"invalid authenticated MIDI event count");
    constexpr std::array<int,8> keys{{60,64,67,69,72,74,76,79}};
    Instance instance(factory,true);
    Audio audio{};
    const auto enqueue=[&](EventList& list,bool on,int key,int offset,
                           float velocity) {
        Event e{};
        e.busIndex=0;e.sampleOffset=offset;
        e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;
        if(on) {
            e.noteOn.channel=0;
            e.noteOn.pitch=static_cast<Steinberg::int16>(key);
            e.noteOn.velocity=velocity;
            e.noteOn.noteId=key;
        }else {
            e.noteOff.channel=0;
            e.noteOff.pitch=static_cast<Steinberg::int16>(key);
            e.noteOff.velocity=0.f;
            e.noteOff.noteId=key;
        }
        require(list.addEvent(e)==kResultOk,"v5 original-source MIDI enqueue");
    };
    for(int block=0;block<40;++block) {
        std::array<float,512> left{},right{};
        float* buffers[]{left.data(),right.data()};
        AudioBusBuffers bus{};
        bus.numChannels=2;bus.channelBuffers32=buffers;
        EventList events;
        for(unsigned i=0;i<count;++i) {
            if(block==static_cast<int>(i))
                enqueue(events,true,keys[i],47+static_cast<int>(i),
                        badThirdVelocity && i==2u?75.f/127.f:90.f/127.f);
            if(block==static_cast<int>(10u+i))
                enqueue(events,false,keys[i],101+static_cast<int>(i),0.f);
        }
        ParameterChanges changes(1);
        if(block==9 || (block==22 && liftPedal)) {
            int32 idx=0;
            auto* q=changes.addParameterData(0x125A0064u,idx);
            require(q!=nullptr,"v5 CC64 queue absent");
            int32 point=0;
            require(q->addPoint(block==9?73:37,
                                block==9?1.:0.,point)==kResultOk,
                    "v5 actual CC64 sample-offset rejected");
        }
        if(block==22)audio.hashBeforeBlock22=audio.hash;
        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
        data.numSamples=512;data.outputs=&bus;data.numOutputs=1;
        data.inputEvents=&events;
        data.inputParameterChanges=&changes;
        require(auditedOriginalBankProcess(instance.audio,data)==kResultOk,
                "exported VST3 source-v5 multi-note process failed");
        for(unsigned frame=0;frame<512u;++frame)
            for(const auto value:{left[frame],right[frame]}) {
                require(std::isfinite(value),"v5 digital PCM nonfinite");
                if(value!=0.f)++audio.nonzero;
                std::uint32_t bits=0;
                std::memcpy(&bits,&value,sizeof bits);
                for(unsigned byte=0;byte<4u;++byte) {
                    audio.hash^=(bits>>(byte*8u))&255u;
                    audio.hash*=0x100000001b3ull;
                }
            }
    }
    return audio;
}
// P002 private REAL-ROM-SF2-bank research test: the exported plugin
// reads this independent, small, STANDARD RIFF SoundFont at activation,
// not a prerecorded 148-byte private NoteOn. Its audio callback runs
// actual bank-to-original-VXD conversion and live chip voice allocation.
// No original Creative VXD, SF2 bank data or ROM is embedded here.
// This test's driver-table pack is SYNTHETIC and cannot prove original
// Win95 state provenance; the user-facing pack generator hashes SBAWE.VXD.
using PrivateBytes=std::vector<std::uint8_t>;
void bankW16(PrivateBytes& b,std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v>>8u));
}
void bankW32(PrivateBytes& b,std::uint32_t v) {
    bankW16(b,v);bankW16(b,v>>16u);
}
void bankName(PrivateBytes& b,std::string_view s) {
    require(s.size()<=20,"synthetic RIFF name too long");
    b.insert(b.end(),s.begin(),s.end());
    b.insert(b.end(),20u-s.size(),0u);
}
void bankChunk(PrivateBytes& out,std::string_view tag,
               const PrivateBytes& data) {
    require(tag.size()==4,"synthetic RIFF chunk id");
    out.insert(out.end(),tag.begin(),tag.end());
    bankW32(out,static_cast<std::uint32_t>(data.size()));
    out.insert(out.end(),data.begin(),data.end());
    if(data.size()&1u)out.push_back(0u);
}
PrivateBytes buildPrivateSyntheticRomSf2Bank(bool ramInsteadOfRom=false,
                                             unsigned matchingZones=1u,
                                             bool invalidLastZone=false,
                                             bool secondProgram=false,
                                             bool mixedRamAfterRom=false) {
    require(matchingZones>=1u && matchingZones<=7u &&
            (!secondProgram || matchingZones==1u),
            "synthetic SF2 layer count/program test invalid");
    PrivateBytes phdr,pbag,pgen,inst,ibag,igen,shdr,info,root;
    bankName(phdr,"SYNTHBANK");bankW16(phdr,7u);bankW16(phdr,0u);
    bankW16(phdr,0u);bankW32(phdr,0u);bankW32(phdr,0u);bankW32(phdr,0u);
    if(secondProgram) {
        bankName(phdr,"SECOND");bankW16(phdr,9u);bankW16(phdr,0u);
        bankW16(phdr,1u);bankW32(phdr,0u);bankW32(phdr,0u);bankW32(phdr,0u);
    }
    bankName(phdr,"DRUM");bankW16(phdr,0u);bankW16(phdr,128u);
    bankW16(phdr,secondProgram?2u:1u);
    bankW32(phdr,0u);bankW32(phdr,0u);bankW32(phdr,0u);
    bankName(phdr,"EOP");bankW16(phdr,0u);bankW16(phdr,0u);
    bankW16(phdr,secondProgram?3u:2u);
    bankW32(phdr,0u);bankW32(phdr,0u);bankW32(phdr,0u);
    for(unsigned i=0u;i<(secondProgram?4u:3u);++i) {
        bankW16(pbag,i);bankW16(pbag,0u);
    }
    for(unsigned i=0u;i<(secondProgram?3u:2u);++i) {
        bankW16(pgen,41u);bankW16(pgen,
            secondProgram && i==1u ?1u:0u);
    }
    bankName(inst,"PIANO");bankW16(inst,0u);
    if(secondProgram) {
        bankName(inst,"SECOND");bankW16(inst,matchingZones);
    }
    bankName(inst,"EOI");bankW16(inst,
        matchingZones+(secondProgram?1u:0u));
    for(unsigned i=0u;i<=matchingZones+(secondProgram?1u:0u);++i) {
        bankW16(ibag,2u*i);bankW16(ibag,0u);
    }
    for(unsigned i=0u;i<matchingZones;++i) {
        bankW16(igen,8u);bankW16(igen,9617u);
        bankW16(igen,53u);
        bankW16(igen,(invalidLastZone && i+1u==matchingZones)?1u:0u);
    }
    if(secondProgram) {
        // Second real SF2 instrument has a markedly different original
        // filter-cutoff generator; no invented sample DSP parameters.
        bankW16(igen,8u);bankW16(igen,5000u);
        bankW16(igen,53u);bankW16(igen,0u);
    }
    // Addresses point to the unchanged original ROM's actual GM0
    // sample region; no copyright-protected PCM is embedded in the SF2.
    bankName(shdr,"ROM-MONO");
    bankW32(shdr,0xfeb9u);bankW32(shdr,0x152feu);
    bankW32(shdr,0x152bbu);bankW32(shdr,0x152fau);
    bankW32(shdr,44100u);
    shdr.push_back(60u);shdr.push_back(0u);
    bankW16(shdr,0u);bankW16(shdr,ramInsteadOfRom?1u:0x8001u);
    if(mixedRamAfterRom) {
        // Original shdr[0]-only guard incorrectly admitted this
        // mixed source, silently aliasing second RAM word addresses
        // into the immutable Creative ROM range.
        bankName(shdr,"RAM-SECOND");
        bankW32(shdr,0xfeb9u);bankW32(shdr,0x152feu);
        bankW32(shdr,0x152bbu);bankW32(shdr,0x152fau);
        bankW32(shdr,44100u);
        shdr.push_back(60u);shdr.push_back(0u);
        bankW16(shdr,0u);bankW16(shdr,1u);
    }
    bankName(shdr,"EOS");
    for(int j=0;j<5;++j)bankW32(shdr,0u);
    shdr.push_back(60u);shdr.push_back(0u);
    bankW16(shdr,0u);bankW16(shdr,0u);
    PrivateBytes ifil;bankW16(ifil,2u);bankW16(ifil,0u);
    bankChunk(info,"ifil",ifil);
    bankChunk(info,"isng",PrivateBytes{'E','M','U','8','0','0','0',0});
    bankChunk(info,"IPRD",PrivateBytes{'S','B','A','W','E','3','2',0});
    bankChunk(info,"irom",PrivateBytes{'1','M','G','M',0});
    PrivateBytes listInfo{'I','N','F','O'};
    listInfo.insert(listInfo.end(),info.begin(),info.end());
    PrivateBytes pdta;
    bankChunk(pdta,"phdr",phdr);bankChunk(pdta,"pbag",pbag);
    bankChunk(pdta,"pgen",pgen);bankChunk(pdta,"inst",inst);
    bankChunk(pdta,"ibag",ibag);bankChunk(pdta,"igen",igen);
    bankChunk(pdta,"shdr",shdr);
    PrivateBytes listPdta{'p','d','t','a'};
    listPdta.insert(listPdta.end(),pdta.begin(),pdta.end());
    PrivateBytes riffBody;
    bankChunk(riffBody,"LIST",listInfo);
    if(ramInsteadOfRom) {
        // A TRUE signed-16-bit standard SoundFont RAM waveform, not
        // merely a shdr with bit 0x8000 cleared. Until the original
        // Creative DRAM allocator/upload is traced, activation MUST
        // reject this bank instead of producing fake chip ROM audio.
        PrivateBytes smpl;
        smpl.reserve(0x15300u*2u);
        for(unsigned i=0;i<0x15300u;++i)
            bankW16(smpl,static_cast<std::uint16_t>(
                i>=0xfeb9u && i<0x152feu ? (i*71u)&0xffffu:0u));
        PrivateBytes sdta{'s','d','t','a'};
        bankChunk(sdta,"smpl",smpl);
        bankChunk(riffBody,"LIST",sdta);
    }
    bankChunk(riffBody,"LIST",listPdta);
    PrivateBytes result{'R','I','F','F'};
    bankW32(result,static_cast<std::uint32_t>(riffBody.size()+4u));
    result.insert(result.end(),{'s','f','b','k'});
    result.insert(result.end(),riffBody.begin(),riffBody.end());
    return result;
}
void writePrivateResearchFile(const std::filesystem::path& path,
                              const PrivateBytes& bytes) {
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);
    require(bool(stream),"open synthetic private source file");
    stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    require(bool(stream),"write synthetic private source file");
}
PrivateBytes buildPrivateSyntheticVxdResourcePack() {
    PrivateBytes b{'1','2','5','A','B','K','0','1'};
    constexpr std::array<std::uint8_t,32> originalVxdSha{{
        0xd3,0x43,0xf2,0x42,0x8a,0xb3,0xe5,0x93,
        0xc9,0xe4,0x2a,0x66,0x8c,0xf5,0x21,0xb7,
        0x80,0x1e,0x12,0x39,0x79,0x2e,0xaf,0x7f,
        0x2b,0x11,0xb5,0xf1,0x79,0xbc,0x7c,0x9a}};
    b.insert(b.end(),originalVxdSha.begin(),originalVxdSha.end());
    b.push_back(7u);b.push_back(2u); // program7, MIDI ch2
    bankW16(b,0u);bankW32(b,524288u); // source 1MGM 1-MiB ROM
    PrivateBytes state(0x1592u,0u);
    state[0x158eu]=0x31u;state[0x158fu]=0x4du;
    state[0x1590u]=0x47u;state[0x1591u]=0x4du;
    state[0x06u]=30u;state[0x18u]=30u;
    for(std::size_t v=0;v<30u;++v) {
        const auto off=0x42u+v*0x20u;
        state[off]=0xfeu;state[off+1u]=0x0fu;
    }
    b.insert(b.end(),state.begin(),state.end());
    for(int i=0;i<59;++i)bankW16(b,0u); // source-shaped preset base
    const auto gm0=realSourceDerivedGm0();
    // SYNTHETIC instrument source template for public test only.
    // No original private VXD table bytes are in Git or the plugin.
    for(unsigned i=0u;i<59u;++i) {
        auto value=static_cast<std::uint16_t>(
            static_cast<unsigned>(gm0[2u*i])|
            (static_cast<unsigned>(gm0[2u*i+1u])<<8u));
        if(i==8u)value=0u; // bank's raw cutoff supplies this op
        if(i==46u || i==47u)value=0u; // actual note key/velocity
        bankW16(b,value);
    }
    for(unsigned i=0u;i<128u;++i)bankW32(b,0u);
    for(unsigned i=0u;i<128u;++i)b.push_back(0u);
    for(unsigned i=0u;i<15u;++i)bankW16(b,0u);
    for(unsigned i=0u;i<16u;++i)bankW16(b,0u);
    require(b.size()==6508u,"synthetic VXD source pack length wrong");
    return b;
}
Audio renderRealBankNotes(IPluginFactory* factory,unsigned noteCount,
                          bool pedalUp,bool wrongChannel=false) {
    require(noteCount>=1u && noteCount<=6u,
            "bank note count out of range");
    constexpr std::array<int,6> keys{{60,64,67,69,72,74}};
    Instance instance(factory,true);
    Audio audio{};
    for(int block=0;block<40;++block) {
        std::array<float,512> left{},right{};
        float* buffers[]{left.data(),right.data()};
        AudioBusBuffers out{};
        out.numChannels=2;out.channelBuffers32=buffers;
        EventList events;
        for(unsigned i=0;i<noteCount;++i) {
            if(block!=static_cast<int>(i*2u) &&
               block!=static_cast<int>(10u+i))continue;
            const bool on=block==static_cast<int>(i*2u);
            Event e{};
            e.busIndex=0;e.sampleOffset=53+static_cast<int>(i);
            e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;
            if(on) {
                e.noteOn.channel=wrongChannel?1:2;
                e.noteOn.pitch=static_cast<Steinberg::int16>(keys[i]);
                e.noteOn.velocity=100.f/127.f;
                e.noteOn.noteId=keys[i];
            }else {
                e.noteOff.channel=wrongChannel?1:2;
                e.noteOff.pitch=static_cast<Steinberg::int16>(keys[i]);
                e.noteOff.velocity=0.f;e.noteOff.noteId=keys[i];
            }
            require(events.addEvent(e)==kResultOk,
                    "real SF2 bank MIDI event");
        }
        ParameterChanges changes(1);
        if(block==8 || (block==18 && pedalUp)) {
            int32 index=0;
            auto* queue=changes.addParameterData(0x125A0064u,index);
            require(queue!=nullptr,"real SF2 bank CC64 queue");
            int32 point=0;
            require(queue->addPoint(block==8?29:37,
                                    block==8?1.:0.,point)==kResultOk,
                    "real SF2 bank sustain point");
        }
        if(block==18)audio.hashBeforeBlock22=audio.hash;
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;data.numSamples=512;
        data.outputs=&out;data.numOutputs=1;
        data.inputEvents=&events;
        data.inputParameterChanges=&changes;
        require(auditedOriginalBankProcess(instance.audio,data)==kResultOk,
                "actual VST3 bank source processing failed");
        for(unsigned frame=0;frame<512u;++frame)
            for(const auto sample:{left[frame],right[frame]}) {
                require(std::isfinite(sample),"ROM-SF2 bank PCM non-finite");
                if(sample!=0.f)++audio.nonzero;
                std::uint32_t bits=0u;
                std::memcpy(&bits,&sample,sizeof bits);
                for(unsigned byte=0;byte<4u;++byte) {
                    audio.hash^=(bits>>(byte*8u))&255u;
                    audio.hash*=0x100000001b3ull;
                }
            }
    }
    return audio;
}
// Independent exported VST3 tests for actual SF2 MIDI Program Change.
// The private VST3 parameter changes only the original logical channel
// program; the resulting NOTE START is freshly compiled from program9's
// genuinely distinct SF2 preset/instrument zone. No retuning existing
// voices, no extra synthetic/second sound engine.
Audio renderBankProgramChange(
    IPluginFactory* factory,int changeToProgram,
    bool playSecond=true,bool tieWithSecondNote=false) {
    Instance instance(factory,true);
    Audio audio{};
    const auto event=[&](EventList& list,bool on,int key,int offset) {
        Event e{};
        e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;
        e.busIndex=0;e.sampleOffset=offset;
        if(on) {
            e.noteOn.channel=2;e.noteOn.pitch=static_cast<Steinberg::int16>(key);
            e.noteOn.velocity=100.f/127.f;e.noteOn.noteId=key;
        }else {
            e.noteOff.channel=2;e.noteOff.pitch=static_cast<Steinberg::int16>(key);
            e.noteOff.velocity=0.f;e.noteOff.noteId=key;
        }
        require(list.addEvent(e)==kResultOk,
                "original ROM SF2 program-switch MIDI event");
    };
    for(int block=0;block<25;++block) {
        std::array<float,512> left{},right{};
        float* buffers[]{left.data(),right.data()};
        AudioBusBuffers output{};
        output.numChannels=2;output.channelBuffers32=buffers;
        EventList events;
        if(block==0)event(events,true,60,53);
        if(block==2)event(events,false,60,71);
        if(playSecond && block==5)event(events,true,64,79);
        if(playSecond && block==11)event(events,false,64,91);
        ParameterChanges changes(1);
        const bool programAtThisBlock=changeToProgram>=0 &&
            block==(tieWithSecondNote?5:4);
        if(programAtThisBlock) {
            int32 index=0;
            auto* queue=changes.addParameterData(0x125A00C0u,index);
            require(queue!=nullptr,"original source program queue absent");
            int32 point=0;
            require(queue->addPoint(tieWithSecondNote?79:29,
                static_cast<Steinberg::Vst::ParamValue>(
                    changeToProgram)/127.,point)==kResultOk,
                "original source program step invalid");
        }
        if(block==5)audio.hashBeforeBlock2=audio.hash;
        ProcessData data{};
        data.processMode=kRealtime;
        data.symbolicSampleSize=kSample32;data.numSamples=512;
        data.outputs=&output;data.numOutputs=1;
        data.inputEvents=&events;
        data.inputParameterChanges=&changes;
        require(auditedOriginalBankProcess(instance.audio,data)==kResultOk,
                "real VST3 ROM SF2 per-program playback failed");
        for(unsigned i=0;i<512u;++i)
            for(const auto sample:{left[i],right[i]}) {
                require(std::isfinite(sample),"original program PCM nonfinite");
                if(sample!=0.f)++audio.nonzero;
                std::uint32_t bits=0u;
                std::memcpy(&bits,&sample,sizeof bits);
                for(unsigned byte=0;byte<4u;++byte) {
                    audio.hash^=(bits>>(byte*8u))&255u;
                    audio.hash*=0x100000001b3ull;
                }
            }
    }
    return audio;
}
} // namespace
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 2;
    HMODULE module=nullptr;
    IPluginFactory* factory=nullptr;
    auto fixture=std::filesystem::temp_directory_path()/
        ("125a-p002-native-vst3-test-"+std::to_string(GetCurrentProcessId())+".bin");
    auto mutant=fixture;
    mutant+=L".invalid";
    auto releaseFixture=fixture;
    releaseFixture+=L".release";
    auto sustainedFixture=fixture;
    sustainedFixture+=L".sustained";
    auto badMarkerFixture=fixture;
    badMarkerFixture+=L".badmarker";
    auto sourceDerivedFixture=fixture;
    sourceDerivedFixture+=L".derived";
    auto derivedSustainFixture=fixture;
    derivedSustainFixture+=L".derivedsustain";
    auto twoSourceNotesFixture=fixture;
    twoSourceNotesFixture+=L".two-source-notes";
    auto duplicateVoiceFixture=fixture;
    duplicateVoiceFixture+=L".duplicate-voice";
    auto multiSourceFixture=fixture;
    multiSourceFixture+=L".eight-source-notes";
    auto duplicateExtraKeyFixture=fixture;
    duplicateExtraKeyFixture+=L".v5-duplicate-key";
    auto duplicateExtraVoiceFixture=fixture;
    duplicateExtraVoiceFixture+=L".v5-duplicate-physical";
    auto sf2BankPath=fixture;
    sf2BankPath+=L".private-rom-sf2";
    auto ramSf2Path=fixture;
    ramSf2Path+=L".private-ram-sf2";
    auto mixedSf2Path=fixture;
    mixedSf2Path+=L".private-rom-first-ram-second";
    auto layeredSf2Path=fixture;
    layeredSf2Path+=L".private-three-zone-rom-sf2";
    auto sixZoneSf2Path=fixture;
    sixZoneSf2Path+=L".private-six-zone-rom-sf2";
    auto sevenZoneSf2Path=fixture;
    sevenZoneSf2Path+=L".private-seven-zone-capped-sf2";
    auto invalidLayerSf2Path=fixture;
    invalidLayerSf2Path+=L".private-invalid-final-zone";
    auto twoProgramSf2Path=fixture;
    twoProgramSf2Path+=L".private-two-program-rom-sf2";
    auto vxdResourcePath=fixture;
    vxdResourcePath+=L".private-source-pack";
    auto badVxdResourcePath=fixture;
    badVxdResourcePath+=L".mutated-source-pack";
    try{
        writeDiagFixture(fixture,false);
        writeDiagFixture(mutant,true);
        writeDiagFixture(releaseFixture,false,true,false,false);
        writeDiagFixture(sustainedFixture,false,true,true,false);
        writeDiagFixture(badMarkerFixture,false,true,false,true);
        writeDiagFixture(sourceDerivedFixture,false,false,false,false,true);
        writeDiagFixture(derivedSustainFixture,false,false,true,false,true);
        writeDiagFixture(twoSourceNotesFixture,false,false,false,false,false,true);
        writeDiagFixture(duplicateVoiceFixture,false,false,false,false,false,true,true);
        writeDiagFixture(multiSourceFixture,false,false,false,false,false,
                         false,false,6u);
        writeDiagFixture(duplicateExtraKeyFixture,false,false,false,false,false,
                         false,false,6u,true,false);
        writeDiagFixture(duplicateExtraVoiceFixture,false,false,false,false,false,
                         false,false,6u,false,true);
        writePrivateResearchFile(sf2BankPath,
                                 buildPrivateSyntheticRomSf2Bank());
        writePrivateResearchFile(ramSf2Path,
                                 buildPrivateSyntheticRomSf2Bank(true));
        writePrivateResearchFile(mixedSf2Path,
                                 buildPrivateSyntheticRomSf2Bank(
                                     false,1u,false,false,true));
        writePrivateResearchFile(layeredSf2Path,
                                 buildPrivateSyntheticRomSf2Bank(false,3u));
        writePrivateResearchFile(sixZoneSf2Path,
                                 buildPrivateSyntheticRomSf2Bank(false,6u));
        writePrivateResearchFile(sevenZoneSf2Path,
                                 buildPrivateSyntheticRomSf2Bank(false,7u,true));
        writePrivateResearchFile(invalidLayerSf2Path,
                                 buildPrivateSyntheticRomSf2Bank(false,3u,true));
        writePrivateResearchFile(twoProgramSf2Path,
                                 buildPrivateSyntheticRomSf2Bank(
                                     false,1u,false,true));
        const auto privatePack=buildPrivateSyntheticVxdResourcePack();
        writePrivateResearchFile(vxdResourcePath,privatePack);
        auto mutatedPack=privatePack;
        mutatedPack[8u]^=1u; // original driver identity must fail closed
        writePrivateResearchFile(badVxdResourcePath,mutatedPack);
        require(_wputenv_s(L"AWE64_P001_ROM_PATH",argv[2])==0,
                "private Creative ROM process binding failed");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           fixture.c_str())==0,
                "P002 private diagnostic process binding failed");
        module=LoadLibraryW(argv[1]);
        require(module!=nullptr,"cannot load actual VST3 DLL");
        auto get=reinterpret_cast<IPluginFactory*(*)()>(
            GetProcAddress(module,"GetPluginFactory"));
        require(get!=nullptr,"exported plugin factory absent");
        factory=get();
        require(factory!=nullptr,"actual VST3 factory null");
#ifdef AWE64_P002_RT_HEAP_AUDIT
        P002RuntimeHeapMonitor rtAllocationGuard{};
#endif
        const auto correct=renderNote(factory,90.f/127.f);
        require(correct.nonzero>300u,"actual P002 note-on PCM is silent");
        const auto repeat=renderNote(factory,90.f/127.f);
        require(repeat.hash==correct.hash && repeat.nonzero==correct.nonzero,
                "P002 actual VST3 PCM is nondeterministic");
        const auto wrongVelocity=renderNote(factory,75.f/127.f);
        require(wrongVelocity.nonzero==0,
                "wrong original x86 fixture velocity unexpectedly played");
        // Source-matched original GM0 release bytes 0x29/0x7F live in the
        // original VXD voice record. Test the EXPORTED VST3 NoteOff
        // audio against an otherwise identical held-note rendering.
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           releaseFixture.c_str())==0,
                "set source authenticated release fixture");
        const auto releaseHeld=renderNote(factory,90.f/127.f);
        const auto released=renderNote(factory,90.f/127.f,8);
        require(releaseHeld.nonzero>300u && released.nonzero>300u,
                "real P002 source-released or held audio unexpectedly silent");
        require(releaseHeld.hash!=released.hash,
                "genuine source-record NoteOff did not change real PCM");
        const auto wrongChannel=renderNote(factory,90.f/127.f,8,1,60);
        require(wrongChannel.hash==releaseHeld.hash,
                "wrong channel unexpectedly released source VXD voice");
        const auto wrongKey=renderNote(factory,90.f/127.f,8,0,61);
        require(wrongKey.hash==releaseHeld.hash,
                "wrong MIDI key unexpectedly released source VXD voice");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           sustainedFixture.c_str())==0,
                "set source-authenticated sustain record fixture");
        const auto sustainHeld=renderNote(factory,90.f/127.f);
        const auto sustainDeferred=renderNote(factory,90.f/127.f,8);
        require(sustainHeld.hash==sustainDeferred.hash,
                "original sustain-deferred NoteOff unexpectedly changed PCM");
        // Version3 is the new authentic source-code dataflow: the
        // instrument's already ORIGINAL-encoded descriptor gen30 and
        // gen38 are copied to the live Win95 voice state by the VST3
        // parser itself (LCOD+0x5A9D/+0x5AA6). No caller-supplied
        // fabricated release-rate fields are accepted or required.
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           sourceDerivedFixture.c_str())==0,
                "set descriptor-derived original NoteOff fixture");
        const auto descriptorHeld=renderNote(factory,90.f/127.f);
        const auto descriptorReleased=renderNote(factory,90.f/127.f,8);
        require(descriptorHeld.hash==releaseHeld.hash,
                "source-derived NoteOn differs from source-verified v2");
        require(descriptorReleased.hash==released.hash,
                "original SF2 gen30/38 NoteOff differs from exact v2 record");
        require(descriptorHeld.hash!=descriptorReleased.hash,
                "original descriptor-derived NoteOff did not change PCM");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           derivedSustainFixture.c_str())==0,
                "set descriptor-derived channel sustain fixture");
        const auto derivedSustainHeld=renderNote(factory,90.f/127.f);
        const auto derivedSustainOff=renderNote(factory,90.f/127.f,8);
        require(derivedSustainOff.hash==derivedSustainHeld.hash,
                "original CHANNEL+4 sustain wrongly released source voice");
        // Original VXD LCOD+0x656D: CC64 on BEFORE NoteOff, then
        // NoteOff defers with marker low0xFF, then CC64 off releases
        // the deferred physical voice with source gen38/gen30 bytes.
        // The CC points are genuine VST3 parameter events, not illegal
        // LegacyMIDICCOutEvent INPUT spoofing.
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           sourceDerivedFixture.c_str())==0,
                "set source-derived CC64 lifecycle fixture");
        const auto pedalled=renderNote(factory,90.f/127.f,
                                      8,0,60,6,12);
        const auto longHeld=renderNote(factory,90.f/127.f);
        const auto immediateRelease=renderNote(factory,90.f/127.f,8);
        require(pedalled.hashBeforeBlock12==longHeld.hashBeforeBlock12,
                "original CC64 held NoteOff changed PCM before pedal-up");
        require(pedalled.hash!=longHeld.hash,
                "actual CC64 pedal-up failed to release deferred VST3 voice");
        require(pedalled.hash!=immediateRelease.hash,
                "CC64 release incorrectly happened at NoteOff time");
        // Version4: exact two separately authorized MIDI key events
        // must address different real EMU8000 voices in one exported
        // VST3 instance. Sustain defers BOTH NoteOffs. A single CC64
        // pedal-up releases both and changes real stereo PCM.
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           twoSourceNotesFixture.c_str())==0,
                "set two-note VST3 source fixture");
        const auto monoInV4=renderTwoSourceNotes(factory,false,false);
        const auto twoHeld=renderTwoSourceNotes(factory,true,false);
        const auto twoLift=renderTwoSourceNotes(factory,true,true);
        const auto invalidSecondVelocity=
            renderTwoSourceNotes(factory,true,false,true);
        require(twoHeld.nonzero>monoInV4.nonzero/2u,
                "two-note actual VST3 PCM unexpectedly silent");
        require(twoHeld.hashBeforeBlock2==monoInV4.hashBeforeBlock2,
                "second note changed PCM before second MIDI sample");
        require(twoHeld.hash!=monoInV4.hash,
                "original second source-bound key did not alter VST3 PCM");
        require(invalidSecondVelocity.hash==monoInV4.hash,
                "unproven second MIDI velocity unexpectedly played");
        require(twoLift.hashBeforeBlock13==twoHeld.hashBeforeBlock13,
                "two-note VST3 sustain did not hold until CC64 pedal-up");
        require(twoLift.hash!=twoHeld.hash,
                "original CC64 pedal-up did not release both VST3 notes");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           duplicateVoiceFixture.c_str())==0,
                "set duplicate physical voice fixture");
        {Instance rejectSecondDup(factory,false);}
        // v5: actual 8 independent original-source NoteOn groups in
        // the exported DLL. Control sample sequences differ from four
        // notes; independently reject one wrong MIDI velocity. Each
        // NoteOff is deferred by original channel CC64 and the original
        // pedal-up scan releases all eight physical voice slots.
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           multiSourceFixture.c_str())==0,
                "set original-source v5 eight-note fixture");
        const auto fourHeld=renderManySourceNotes(factory,4u,false);
        const auto eightHeld=renderManySourceNotes(factory,8u,false);
        const auto eightHeldAgain=renderManySourceNotes(factory,8u,false);
        const auto eightLift=renderManySourceNotes(factory,8u,true);
        const auto wrongThird=renderManySourceNotes(factory,8u,false,true);
        require(eightHeld.nonzero>1000u &&
                eightHeld.hash!=fourHeld.hash,
                "original-source VST3 v5 additional notes were silent");
        require(eightHeldAgain.hash==eightHeld.hash &&
                eightHeldAgain.nonzero==eightHeld.nonzero,
                "original-source v5 multi-voice chip PCM nondeterministic");
        require(eightHeld.hash!=wrongThird.hash,
                "unverified third MIDI velocity unexpectedly played");
        require(eightLift.hashBeforeBlock22==eightHeld.hashBeforeBlock22,
                "v5 eight-note release changed audio before CC64 pedal-up");
        require(eightLift.hash!=eightHeld.hash,
                "original channel CC64 failed to release 8 VST3 notes");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           duplicateExtraKeyFixture.c_str())==0,
                "set v5 duplicate key invalid fixture");
        {Instance rejectV5DuplicateKey(factory,false);}
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           duplicateExtraVoiceFixture.c_str())==0,
                "set v5 duplicate physical voice invalid fixture");
        {Instance rejectV5DuplicateVoice(factory,false);}
        // P002 actual ROM-SF2 bank mode: same real exported DLL, this
        // time loading a STANDARD RIFF bank at activation and converting
        // each independent VST3 MIDI note through the bank parser,
        // source generator resources, original live VXD allocator, and
        // the genuine NoteOff/CC64 engine. No preauthored 148-byte
        // NoteOn fixture is used by this path.
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",L"")==0,
                "clear old private diagnostic mode");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_ROM_BANK_PATH",
                           sf2BankPath.c_str())==0,
                "set external ROM SoundFont bank");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_VXD_RESOURCES_PATH",
                           vxdResourcePath.c_str())==0,
                "set externally-derived source resource pack");
        const auto firstBankKey=renderRealBankNotes(factory,1u,false);
        const auto threeBankKeys=renderRealBankNotes(factory,3u,false);
        const auto threeBankKeysAgain=renderRealBankNotes(factory,3u,false);
        const auto releasedBankKeys=renderRealBankNotes(factory,3u,true);
        const auto noAuthorizedChannel=renderRealBankNotes(factory,3u,false,true);
        require(threeBankKeys.nonzero>100u &&
                threeBankKeys.hash!=firstBankKey.hash,
                "bank-derived multi-key MIDI did not create distinct real PCM");
        require(threeBankKeys.hash==threeBankKeysAgain.hash,
                "original bank-derived multi-NoteOn real VST3 PCM changed");
        require(noAuthorizedChannel.nonzero==0u,
                "unverified ROM-SF2 MIDI channel unexpectedly played");
        require(threeBankKeys.hashBeforeBlock22==
                releasedBankKeys.hashBeforeBlock22,
                "original ROM-SF2 bank sustain changed PCM before pedal-up");
        require(threeBankKeys.hash!=releasedBankKeys.hash,
                "bank-derived original CC64 release did not affect PCM");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_VXD_RESOURCES_PATH",
                           badVxdResourcePath.c_str())==0,
                "select corrupt original VXD source resource pack");
        {Instance badSourcePack(factory,false);}
        require(_wputenv_s(L"AWE64_P002_PRIVATE_VXD_RESOURCES_PATH",
                           vxdResourcePath.c_str())==0,
                "restore source pack");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_ROM_BANK_PATH",
                           ramSf2Path.c_str())==0,
                "select RAM-sample source negative control");
        {Instance unsupportedRam(factory,false);}
        require(_wputenv_s(L"AWE64_P002_PRIVATE_ROM_BANK_PATH",
                           mixedSf2Path.c_str())==0,
                "select mixed ROM-first RAM-second SoundFont");
        {Instance mixedBankRejectedBeforeActivation(factory,false);}
        // This fully formed SDTA waveform is not authorized to play
        // from fake ROM: original Win95 RAM sample placement/upload
        // provenance has not been validated. Refuse at activation.
        require(_wputenv_s(L"AWE64_P002_PRIVATE_ROM_BANK_PATH",
                           layeredSf2Path.c_str())==0,
                "select three-layer original-source ROM bank");
        const auto layeredOne=renderRealBankNotes(factory,1u,false);
        const auto layeredHeld=renderRealBankNotes(factory,3u,false);
        const auto layeredLift=renderRealBankNotes(factory,3u,true);
        require(layeredOne.nonzero>100u &&
                layeredOne.hash!=firstBankKey.hash,
                "actual VST3 three-layer original bank unchanged from mono");
        require(layeredHeld.hash!=threeBankKeys.hash,
                "actual VST3 three-layer bank is not musically different");
        require(layeredHeld.hashBeforeBlock22==
                layeredLift.hashBeforeBlock22 &&
                layeredHeld.hash!=layeredLift.hash,
                "actual VST3 layered source CC64 failed");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_ROM_BANK_PATH",
                           sixZoneSf2Path.c_str())==0,
                "select six-layer original SF2 note");
        const auto sixVoices=renderRealBankNotes(factory,1u,false);
        require(sixVoices.nonzero>100u &&
                sixVoices.hash!=layeredOne.hash,
                "six matching source SF2 zones not independently played");
        // Five separate keys, six SF2 zones per NoteOn = all THIRTY
        // original EMU8000 musical voices. The sixth key MUST NOT
        // overwrite any held physical slot (no unverified voice steal).
        // CC64 lifts and releases the full 30 source-verified ledger.
        const auto thirtyVoiceHeld=renderRealBankNotes(factory,5u,false);
        const auto thirtyVoiceLift=renderRealBankNotes(factory,5u,true);
        const auto refusedThirtyFirst=renderRealBankNotes(factory,6u,false);
        require(thirtyVoiceHeld.nonzero>100u &&
                thirtyVoiceHeld.hash!=sixVoices.hash,
                "30 physical original source-bank voices not emitted");
        require(refusedThirtyFirst.hash==thirtyVoiceHeld.hash,
                "unverified 31st SF2 voice illegally stole original hardware");
        require(thirtyVoiceLift.hashBeforeBlock22==
                thirtyVoiceHeld.hashBeforeBlock22 &&
                thirtyVoiceLift.hash!=thirtyVoiceHeld.hash,
                "all 30 source bank voices not released by original CC64");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_ROM_BANK_PATH",
                           sevenZoneSf2Path.c_str())==0,
                "select seven layers with invalid seventh EOS");
        const auto sevenCapped=renderRealBankNotes(factory,1u,false);
        require(sevenCapped.hash==sixVoices.hash,
                "original 6-layer cap incorrectly started seventh EOS");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_ROM_BANK_PATH",
                           invalidLayerSf2Path.c_str())==0,
                "select source bank with invalid third zone");
        const auto badThirdZone=renderRealBankNotes(factory,1u,false);
        require(badThirdZone.nonzero==0u,
                "invalid third ROM SF2 layer partially started hardware");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_ROM_BANK_PATH",
                           twoProgramSf2Path.c_str())==0,
                "set multi-program actual ROM SF2 bank");
        const auto defaultProgram7=renderBankProgramChange(factory,-1);
        const auto program9=renderBankProgramChange(factory,9);
        const auto program9AtSameSample=renderBankProgramChange(
            factory,9,true,true);
        const auto nonexistentProgram0=renderBankProgramChange(factory,0);
        const auto noSecondNoSwitch=renderBankProgramChange(
            factory,-1,false);
        const auto noSecondButSwitch=renderBankProgramChange(
            factory,9,false);
        require(defaultProgram7.nonzero>100u &&
                program9.nonzero>100u &&
                defaultProgram7.hashBeforeBlock2==
                program9.hashBeforeBlock2 &&
                defaultProgram7.hash!=program9.hash,
                "ROM SF2 actual second preset did not change chip PCM");
        require(program9AtSameSample.hash==program9.hash,
                "same-sample VST3 program selection was not applied before MIDI");
        require(noSecondNoSwitch.hash==noSecondButSwitch.hash,
                "program change incorrectly modified sounding original voice");
        require(nonexistentProgram0.hash!=defaultProgram7.hash,
                "unselected SF2 program unexpectedly played old preset");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_ROM_BANK_PATH",L"")==0 &&
                _wputenv_s(L"AWE64_P002_PRIVATE_VXD_RESOURCES_PATH",L"")==0,
                "clear private SF2 research mode");
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           badMarkerFixture.c_str())==0,
                "set invalid source note identity fixture");
        {Instance rejectBadMarker(factory,false);}
        require(_wputenv_s(L"AWE64_P002_PRIVATE_NOTE_FIXTURE",
                           mutant.c_str())==0,"set negative fixture");
        {Instance reject(factory,false);}
        factory->release();factory=nullptr;
        FreeLibrary(module);module=nullptr;
        std::error_code ec;std::filesystem::remove(fixture,ec);
        std::filesystem::remove(mutant,ec);
        std::filesystem::remove(releaseFixture,ec);
        std::filesystem::remove(sustainedFixture,ec);
        std::filesystem::remove(badMarkerFixture,ec);
        std::filesystem::remove(sourceDerivedFixture,ec);
        std::filesystem::remove(derivedSustainFixture,ec);
        std::filesystem::remove(twoSourceNotesFixture,ec);
        std::filesystem::remove(duplicateVoiceFixture,ec);
        std::filesystem::remove(multiSourceFixture,ec);
        std::filesystem::remove(duplicateExtraKeyFixture,ec);
        std::filesystem::remove(duplicateExtraVoiceFixture,ec);
        std::filesystem::remove(sf2BankPath,ec);
        std::filesystem::remove(ramSf2Path,ec);
        std::filesystem::remove(mixedSf2Path,ec);
        std::filesystem::remove(layeredSf2Path,ec);
        std::filesystem::remove(sixZoneSf2Path,ec);
        std::filesystem::remove(sevenZoneSf2Path,ec);
        std::filesystem::remove(invalidLayerSf2Path,ec);
        std::filesystem::remove(twoProgramSf2Path,ec);
        std::filesystem::remove(vxdResourcePath,ec);
        std::filesystem::remove(badVxdResourcePath,ec);
        std::cout<<"P002_EXPORTED_REAL_VST3_MIDI_TO_PINNED_EMU8000_PCM_PASS"
                 <<" original_ROM_from_hash_locked_external_source=TRUE"
                 <<" synthetic_original_x86_GM0_fixture=TRUE"
                 <<" true_P002_SF2_61_OUT_VST3_noteon=TRUE"
                 <<" nonzero="<<correct.nonzero
                 <<" reproducible_pcm=TRUE"
                 <<" wrong_key_velocity_source_rejected=TRUE"
                 <<" corrupted_source_identity_rejected=TRUE"
                 <<" source_VXD_encoded_NoteOff_changes_actual_PCM=TRUE"
                 <<" source_VXD_sustain_defers_HW_release=TRUE"
                 <<" source_original_SF2_gen30_gen38_NoteOff_v3_PCM_parity=TRUE"
                 <<" sustain_from_channel_plus4_NOT_voice_plus4=TRUE"
                 <<" original_CC64_pedal_up_releases_deferred_VST3_PCM=TRUE"
                 <<" CC64_sample_accurate_VST3_parameter_queue=TRUE"
                 <<" wrong_NoteOff_channel_and_key_ignored=TRUE"
                 <<" invalid_original_voice_marker_rejected=TRUE"
                 <<" original_Win95_driver_pcm_parity=NOT_PROVEN"
                 <<" source_verified_two_sequential_keys_actual_VST3_PCM=TRUE"
                 <<" source_CC64_two_physical_voices_actual_VST3_PCM=TRUE"
                 <<" unverified_second_voice_rejected=TRUE"
                 <<" v5_eight_source_bound_MIDI_keys_real_VST3_PCM=TRUE"
                 <<" v5_eight_voice_original_CC64_release_real_VST3_PCM=TRUE"
                 <<" v5_wrong_velocity_duplicate_voice_and_key_rejected=TRUE"
                 <<" source_bank_file_loaded_inside_exported_VST3=TRUE"
                 <<" real_bank_midi_three_key_EMU8000_PCM=TRUE"
                 <<" real_bank_original_CC64_releases_three_source_voices=TRUE"
                 <<" private_VXD_source_identity_mutation_rejected=TRUE"
                 <<" unsupported_RAM_SF2_bank_fail_closed=TRUE"
                 <<" genuine_RAM_SF2_smpl_rejected_at_activation=TRUE"
                 <<" mixed_ROM_first_RAM_later_rejected_at_activation=TRUE"
                 <<" original_three_zone_ROM_SF2_vst3_PCM=TRUE"
                 <<" original_six_zone_ROM_SF2_vst3_PCM=TRUE"
                 <<" original_seventh_zone_cap_and_bad_third_fail_closed=TRUE"
                 <<" original_layered_SF2_NoteOff_CC64=TRUE"
                 <<" original_30_physical_SF2_bank_voices_real_VST3_PCM=TRUE"
                 <<" original_30_voice_CC64_full_release=TRUE"
                 <<" original_unproven_31st_voice_stealing_forbidden=TRUE"
                 <<" actual_source_ROM_SF2_program_7_to_9_changes_PCM=TRUE"
                 <<" program_change_existing_voices_unaffected=TRUE"
                 <<" program_change_same_sample_precedes_NoteOn=TRUE"

#ifdef AWE64_P002_RT_HEAP_AUDIT
                 <<" P002_original_bank_30voice_program_CC64_rt_heap_calls="
                 <<originalBankAllocations
                 <<" rt_heap_bytes="<<originalBankAllocatedBytes
                 <<" positive_allocation_hook=TRUE"
#endif
                 <<" unrestricted_RAM_SF2_and_Win95_PCM_PARITY=NOT_PROVEN\n";
        return 0;
    }catch(const std::exception& e){
        if(factory)factory->release();
        if(module)FreeLibrary(module);
        std::error_code ec;std::filesystem::remove(fixture,ec);
        std::filesystem::remove(mutant,ec);
        std::filesystem::remove(releaseFixture,ec);
        std::filesystem::remove(sustainedFixture,ec);
        std::filesystem::remove(badMarkerFixture,ec);
        std::filesystem::remove(sourceDerivedFixture,ec);
        std::filesystem::remove(derivedSustainFixture,ec);
        std::filesystem::remove(twoSourceNotesFixture,ec);
        std::filesystem::remove(duplicateVoiceFixture,ec);
        std::filesystem::remove(multiSourceFixture,ec);
        std::filesystem::remove(duplicateExtraKeyFixture,ec);
        std::filesystem::remove(duplicateExtraVoiceFixture,ec);
        std::filesystem::remove(sf2BankPath,ec);
        std::filesystem::remove(ramSf2Path,ec);
        std::filesystem::remove(mixedSf2Path,ec);
        std::filesystem::remove(layeredSf2Path,ec);
        std::filesystem::remove(sixZoneSf2Path,ec);
        std::filesystem::remove(sevenZoneSf2Path,ec);
        std::filesystem::remove(invalidLayerSf2Path,ec);
        std::filesystem::remove(twoProgramSf2Path,ec);
        std::filesystem::remove(vxdResourcePath,ec);
        std::filesystem::remove(badVxdResourcePath,ec);
        std::cerr<<"P002_EXPORTED_VST3_DIAGNOSTIC_FAIL "<<e.what()<<"\n";
        return 1;
    }
}
