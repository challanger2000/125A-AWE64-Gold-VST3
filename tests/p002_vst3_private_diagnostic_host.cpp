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
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
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
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
void require(const bool p,const char* msg){if(!p)throw std::runtime_error(msg);}
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
                      bool wrongOriginalMarker=false){
    std::ofstream o(p,std::ios::binary|std::ios::trunc);
    require(bool(o),"cannot open synthetic P002 host fixture");
    auto raw=[&](std::uint8_t b){o.put(static_cast<char>(b));};
    for(const char c:std::string("125AP2D1"))o.put(c);
    const std::array<std::uint8_t,6> head{{
        static_cast<std::uint8_t>(releaseV2?2u:1u),0u,0u,60u,90u,1u}};
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
struct Audio {std::uint64_t nonzero=0;std::uint64_t hash=0xcbf29ce484222325ull;};
Audio renderNote(IPluginFactory* factory,float velocity,
                 int noteOffBlock=-1,int noteOffChannel=0,
                 int noteOffPitch=60){
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
        ProcessData data{};
        data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
        data.numSamples=512;data.outputs=&bus;data.numOutputs=1;
        data.inputEvents=&list;
        require(instance.audio->process(data)==kResultOk,
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
    try{
        writeDiagFixture(fixture,false);
        writeDiagFixture(mutant,true);
        writeDiagFixture(releaseFixture,false,true,false,false);
        writeDiagFixture(sustainedFixture,false,true,true,false);
        writeDiagFixture(badMarkerFixture,false,true,false,true);
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
                 <<" wrong_NoteOff_channel_and_key_ignored=TRUE"
                 <<" invalid_original_voice_marker_rejected=TRUE"
                 <<" original_Win95_driver_pcm_parity=NOT_PROVEN"
                 <<" full_SF2_bank_polyphony_CC64_lifecycle=NOT_IMPLEMENTED\n";
        return 0;
    }catch(const std::exception& e){
        if(factory)factory->release();
        if(module)FreeLibrary(module);
        std::error_code ec;std::filesystem::remove(fixture,ec);
        std::filesystem::remove(mutant,ec);
        std::filesystem::remove(releaseFixture,ec);
        std::filesystem::remove(sustainedFixture,ec);
        std::filesystem::remove(badMarkerFixture,ec);
        std::cerr<<"P002_EXPORTED_VST3_DIAGNOSTIC_FAIL "<<e.what()<<"\n";
        return 1;
    }
}
