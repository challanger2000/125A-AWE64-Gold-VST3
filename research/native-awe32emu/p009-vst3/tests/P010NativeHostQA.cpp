// P010 actual Windows x64 Steinberg VST3 original PCM owner acceptance.
// No guest Win95, 86Box VM, fabricated tones, Studio One, or driver DLL.
// --public with NO private files expects SILENCE, not a claimed audio pass.
// --private with original ROM + Creative SBK + owner SF2s tests REAL AUDIO.
#define NOMINMAX
#include <windows.h>
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
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
#include <fstream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using namespace Steinberg;
using namespace Steinberg::Vst;
namespace {
constexpr int frames=1024;
void check(bool v,const char* why){
 if(!v){std::fprintf(stderr,"P010_FAIL %s\n",why);throw std::runtime_error(why);}
}
struct Saved {
 uint32 magic=0x39303050u,version=4u,program=0u,bank=0u;
 float master=1.f;
 std::array<std::uint8_t,15> otherPrograms{},otherBanks{};
 std::array<std::uint8_t,2> reserved{};
 std::array<std::uint16_t,16*8> midiStates{};
 std::array<std::uint16_t,16*8> advancedStates{};
 std::array<std::uint8_t,16> parameterModes{};
};
static_assert(sizeof(Saved)==580,"P013 MIDI v4 state size");
struct Plugin{
 IPtr<IComponent> component;
 IPtr<IEditController> controller;
 IPtr<IAudioProcessor> processor;
 bool initialized=false,active=false,processing=false;
 Plugin(const VST3::Hosting::PluginFactory& factory,const VST3::UID& id,FUnknown* host){
   component=factory.createInstance<IComponent>(id);
   check(bool(component),"VST3 factory IComponent");
   check(component->initialize(host)==kResultOk,"VST3 initialize");
   initialized=true;
   IAudioProcessor* rawAudio=nullptr;
   check(component->queryInterface(IAudioProcessor::iid,reinterpret_cast<void**>(&rawAudio))==kResultOk&&rawAudio,"IAudioProcessor");
   processor=owned(rawAudio);
   IEditController* rawEdit=nullptr;
   check(component->queryInterface(IEditController::iid,reinterpret_cast<void**>(&rawEdit))==kResultOk&&rawEdit,"IEditController");
   controller=owned(rawEdit);
 }
 ~Plugin(){
   if(processing)(void)processor->setProcessing(false);
   if(active)(void)component->setActive(false);
   if(initialized)(void)component->terminate();
 }
 Plugin(const Plugin&)=delete;
 Plugin& operator=(const Plugin&)=delete;
 void start(int prog,int bank,double rate){
   Saved saved{};saved.program=static_cast<uint32>(prog);saved.bank=static_cast<uint32>(bank);
   saved.midiStates.fill(0xFFFFu);
   saved.advancedStates.fill(0xFFFFu);
   // P011: program/bank on the ACTUAL MIDI channel, not merely the global
   // CH1 browser. Run every channel with the requested note's instrument.
   for(unsigned ch=1;ch<16;++ch){
      saved.otherPrograms[ch-1]=static_cast<std::uint8_t>(prog);
      saved.otherBanks[ch-1]=static_cast<std::uint8_t>(bank==1?1:0);
   }
   MemoryStream stream;int32 written=0;
   check(stream.write(&saved,sizeof(saved),&written)==kResultOk&&written==sizeof(saved),"write VST3 saved state");
   check(stream.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,"rewind VST3 saved state");
   check(component->setState(&stream)==kResultOk,"actual VST3 restore state");
   MemoryStream roundtrip;
   check(component->getState(&roundtrip)==kResultOk&&
       roundtrip.getSize()==sizeof(saved)&&
       std::memcmp(roundtrip.getData(),&saved,sizeof(saved))==0,
       "actual VST3 state roundtrip");
   ProcessSetup setup{};
   setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;
   setup.sampleRate=rate;setup.maxSamplesPerBlock=frames;
   check(processor->canProcessSampleSize(kSample32)==kResultTrue,"VST3 float32");
   check(processor->setupProcessing(setup)==kResultOk,"VST3 process setup");
   check(component->setActive(true)==kResultOk,"VST3 activation");
   active=true;
   check(processor->setProcessing(true)==kResultOk,"VST3 audio start");
   processing=true;
 }
};
// Integrated final native owner GUI -> VST3 state regression, real Steinberg
// Windows host. Works publicly WITHOUT any protected ROM/SBK/SF2 files:
// no audible claims; proves all 12 P165 control edits, backward-compatible
// V4 bytes and full V5 roundtrip/corruption rejection.
void verifyP165GuiV5OwnerAssetState(
   const VST3::Hosting::PluginFactory& factory,
   const VST3::UID& id,FUnknown* host){
 auto unit=Plugin(factory,id,host);
 MemoryStream initial;
 check(unit.component->getState(&initial)==kResultOk &&
       initial.getSize()==sizeof(Saved),
       "P165 unedited host project must preserve exact V4 580-byte format");
 auto* original=static_cast<const std::uint8_t*>(initial.getData());
 check(original && std::memcmp(original+4,"\004\000\000\000",4)==0,
       "P165 unedited project must use v4, not fake new format");
 // GUI gestures must be real host automation parameter IDs as in P007BW.
 for(unsigned i=0;i<12;++i){
    const auto norm=double(i+1)/20.;
    check(unit.controller->setParamNormalized(1100u+i,norm)==kResultOk,
          "P165 actual host GUI control could not be set");
 }
 MemoryStream v5;
 check(unit.component->getState(&v5)==kResultOk &&
       v5.getSize()==644u,"P165 12 GUI edits did not persist in V5 644-byte owner state");
 const auto* bytes=static_cast<const std::uint8_t*>(v5.getData());
 check(bytes && bytes[4]==5u&&bytes[5]==0u&&bytes[6]==0u&&bytes[7]==0u,
       "P165 GUI control state must be exact V5 header");
 // Verify extension 64B magic, empty path and all 12 quantized basis points.
 check(bytes[580]==0x50&&bytes[581]==0x41&&bytes[582]==0x53&&bytes[583]==0x35,
       "P165 V5 state exact PAS5 extension marker");
 for(unsigned i=0;i<12;++i){
    unsigned v=unsigned(bytes[588+i*2])|unsigned(bytes[589+i*2])<<8;
    check(v==static_cast<unsigned>((i+1)*500),
          "P165 control parameter did not roundtrip exactly");
 }
 auto reopen=Plugin(factory,id,host);
 MemoryStream request;
 int32 n=0;
 check(request.write(v5.getData(),static_cast<int32>(v5.getSize()),&n)==kResultOk,
       "P165 prepare valid V5 state");
 check(request.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,
       "P165 rewind valid V5 state");
 check(reopen.component->setState(&request)==kResultOk,
       "P165 host cannot restore all 12 GUI settings");
 MemoryStream roundtrip;
 check(reopen.component->getState(&roundtrip)==kResultOk &&
       roundtrip.getSize()==v5.getSize()&&
       std::memcmp(roundtrip.getData(),v5.getData(),v5.getSize())==0,
       "P165 12 controls V5 state differs after real host reopen");
 // An external SoundFont path with an invalid/missing SHA MUST fail without
 // changing any existing instrument, GUI, controller, or sound identity.
 auto corrupt=std::vector<std::uint8_t>(bytes,bytes+v5.getSize());
 corrupt[584]=10u; // 10-byte invalid path length; no appended path data
 MemoryStream invalid;
 check(invalid.write(corrupt.data(),static_cast<int32>(corrupt.size()),&n)==kResultOk,
       "P165 construct invalid owner SF2 state");
 check(invalid.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,
       "P165 rewind invalid SF2 state");
 check(reopen.component->setState(&invalid)!=kResultOk,
       "P165 missing owner SF2 must not silently select an unrelated file");
 MemoryStream unaffected;
 check(reopen.component->getState(&unaffected)==kResultOk &&
       unaffected.getSize()==v5.getSize() &&
       std::memcmp(unaffected.getData(),v5.getData(),v5.getSize())==0,
       "P165 missing owner file must leave prior original ROM/GUI state intact");
 std::puts("P165_REAL_VST3_GUI_12_CONTROLS_V5_PROJECT_REOPEN_SHA_FAIL_CLOSED_PASS");
}
void verifyP011MultichannelProjectRecall(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host) {
 auto original=Plugin(factory,id,host);
 Saved expected{};expected.midiStates.fill(0xFFFFu);
 expected.advancedStates.fill(0xFFFFu);
 expected.program=7;expected.bank=1;expected.master=.625f;
 for(unsigned channel=1;channel<16;++channel){
     expected.otherPrograms[channel-1]=static_cast<std::uint8_t>(
         (channel*11u)%128u);
     expected.otherBanks[channel-1]=static_cast<std::uint8_t>(
         channel%2u);
 }
 const auto restore=[&](Plugin& p,const void* bytes,int32 size,bool mustPass){
     MemoryStream stream;int32 n=0;
     check(stream.write(const_cast<void*>(bytes),size,&n)==kResultOk &&
           n==size,"P011 host write state");
     check(stream.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,
           "P011 host rewind state");
     const auto result=p.component->setState(&stream);
     check((result==kResultOk)==mustPass,"P011 project acceptance mismatch");
 };
 const auto snapshot=[&](Plugin& p){
     Saved value{};
     MemoryStream stream;
     check(p.component->getState(&stream)==kResultOk &&
           stream.getSize()==sizeof(value),
           "P013 580-byte VST3 state output");
     std::memcpy(&value,stream.getData(),sizeof(value));
     return value;
 };
 restore(original,&expected,static_cast<int32>(sizeof(expected)),true);
 auto out=snapshot(original);
 check(std::memcmp(&out,&expected,sizeof(expected))==0,
       "P011 unique 16 MIDI channel patches lost on state restore");
 auto reopened=Plugin(factory,id,host);
 restore(reopened,&out,static_cast<int32>(sizeof(out)),true);
 auto equal=snapshot(reopened);
 check(std::memcmp(&out,&equal,sizeof(out))==0,
       "P011 new VST3 instance cannot restore separate MIDI patches");
 auto invalid=out;
 invalid.otherPrograms[4]=255;
 restore(reopened,&invalid,static_cast<int32>(sizeof(invalid)),false);
 equal=snapshot(reopened);
 check(std::memcmp(&out,&equal,sizeof(out))==0,
       "P011 invalid program corrupted active VST3 state");
 invalid=out;
 invalid.otherBanks[8]=3;
 restore(reopened,&invalid,static_cast<int32>(sizeof(invalid)),false);
 equal=snapshot(reopened);
 check(std::memcmp(&out,&equal,sizeof(out))==0,
       "P011 invalid bank corrupted active VST3 state");
 // P009 v1 old projects contain only 20 bytes. They must still open.
 struct Legacy {
     uint32_t magic=0x39303050u,version=1u,program=12u,bank=1u;
     float master=.5f;
 } old{};
 static_assert(sizeof(Legacy)==20,"P009 legacy state bytes");
 auto older=Plugin(factory,id,host);
 restore(older,&old,static_cast<int32>(sizeof(old)),true);
 const auto upgraded=snapshot(older);
 check(upgraded.version==4u&&upgraded.program==12u&&
       upgraded.bank==1u&&upgraded.master==.5f,
       "P011 legacy P009 project upgrade failed");
 for(unsigned n=0;n<15;++n)
     check(upgraded.otherPrograms[n]==0u&&upgraded.otherBanks[n]==0u,
           "P011 legacy project invented channel programs");
 for(const auto value:upgraded.midiStates)
     check(value==0xFFFFu,"P012 v1 legacy project invented a MIDI CC");
 // Existing P011 version2 52-byte state also upgrades without
 // dispatching CC commands not actually recorded by the user.
 auto v2=out;v2.version=2u;
 auto prev=Plugin(factory,id,host);
 restore(prev,&v2,52,true);
 const auto upgraded2=snapshot(prev);
 check(upgraded2.version==4u && upgraded2.program==out.program &&
       upgraded2.bank==out.bank,"P012 version2 patch upgrade");
 for(const auto value:upgraded2.midiStates)
     check(value==0xFFFFu,"P012 version2 invented a MIDI CC");
 // An illegal MIDI value must NOT change any pre-existing channel state.
 invalid=out;invalid.midiStates[2*8+4]=200u;
 restore(reopened,&invalid,static_cast<int32>(sizeof(invalid)),false);
 equal=snapshot(reopened);
 check(std::memcmp(&out,&equal,sizeof(out))==0,
       "P012 invalid sustain value corrupted VST3 project");
 invalid=out;invalid.midiStates[15*8+7]=20000u;
 restore(reopened,&invalid,static_cast<int32>(sizeof(invalid)),false);
 equal=snapshot(reopened);
 check(std::memcmp(&out,&equal,sizeof(out))==0,
       "P012 invalid pitchbend value corrupted VST3 project");
 // Native P012 308-byte v3 must remain readable without inventing CCs.
 auto v3=out;v3.version=3u;
 auto oldV3=Plugin(factory,id,host);
 restore(oldV3,&v3,308,true);
 const auto upgraded3=snapshot(oldV3);
 check(upgraded3.version==4u&&
       std::memcmp(upgraded3.midiStates.data(),v3.midiStates.data(),
                   sizeof(v3.midiStates))==0,
       "P013 project reopen P012 v3");
 for(auto value:upgraded3.advancedStates)
     check(value==0xFFFFu,"P013 v3 invented advanced CC");
 invalid=out;invalid.advancedStates[2*8+4]=200u;
 restore(reopened,&invalid,static_cast<int32>(sizeof(invalid)),false);
 equal=snapshot(reopened);
 check(std::memcmp(&out,&equal,sizeof(out))==0,
       "P013 invalid RPN value changed project");
 invalid=out;invalid.parameterModes[15]=3u;
 restore(reopened,&invalid,static_cast<int32>(sizeof(invalid)),false);
 equal=snapshot(reopened);
 check(std::memcmp(&out,&equal,sizeof(out))==0,
       "P013 invalid NRPN mode changed project");
 std::puts("P013_REAL_VST3_PROJECT_STATE_V1_V2_V3_V4_PASS");
 std::puts("P012_REAL_VST3_16_CHANNEL_CONTROLLER_STATE_V1_V2_V3_BACKWARD_RECALL_PASS");
 std::puts("P011_REAL_VST3_16_CHANNEL_PROJECT_RECALL_CORRUPT_STATE_REJECT_LEGACY_V1_PASS");
}

void verifyP012MidiMapping(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host){
    auto p=Plugin(factory,id,host);
    IMidiMapping* pointer=nullptr;
    check(p.controller->queryInterface(IMidiMapping::iid,
        reinterpret_cast<void**>(&pointer))==kResultOk && pointer,
        "P012 actual VST3 IEditController missing IMidiMapping");
    const IPtr<IMidiMapping> mapping=owned(pointer);
    constexpr std::array<CtrlNumber,8> controllers{
        kCtrlModWheel,kCtrlVolume,kCtrlPan,kCtrlExpression,kCtrlSustainOnOff,
        kCtrlEff1Depth,kCtrlEff3Depth,kPitchBend
    };
    std::set<ParamID> unique;
    for(int ch=0;ch<16;++ch)
        for(unsigned i=0;i<8;++i){
            ParamID idOut=0;
            check(mapping->getMidiControllerAssignment(0,static_cast<int16>(ch),
                controllers[i],idOut)==kResultTrue,
                "P012 actual VST3 one of 128 MIDI channel assignments missing");
            check(idOut==2000u+static_cast<unsigned>(ch)*8+i,
                "P012 unexpected MIDI mapping parameter ID");
            unique.insert(idOut);
            ParameterInfo info{};
            bool found=false;
            for(int j=0;j<p.controller->getParameterCount();++j){
                if(p.controller->getParameterInfo(j,info)==kResultOk &&
                   info.id==idOut){found=true;break;}
            }
            check(found,"P012 real VST3 MIDI mapping targets unregistered parameter");
        }
    check(unique.size()==128u,"P012 MIDI CC parameters have duplicate IDs");
    constexpr std::array<CtrlNumber,8> advanced{
        kCtrlBankSelectMSB,kCtrlBankSelectLSB,kCtrlRPNSelectMSB,
        kCtrlRPNSelectLSB,kCtrlDataEntryMSB,kCtrlDataEntryLSB,
        kCtrlNRPNSelectMSB,kCtrlNRPNSelectLSB
    };
    for(int ch=0;ch<16;++ch)
        for(unsigned slot=0;slot<8;++slot){
            ParamID mapped=0;
            check(mapping->getMidiControllerAssignment(0,static_cast<int16>(ch),
                  advanced[slot],mapped)==kResultTrue,
                  "P013 RPN/NRPN/Bank MIDI assignment missing");
            check(mapped==2400u+ch*8u+slot&&unique.insert(mapped).second,
                  "P013 MIDI assignment ID duplicate");
        }
    check(unique.size()==256u,"P013 256 MIDI mappings incomplete");
    std::puts("P013_REAL_VST3_256_CHANNEL_MIDI_CONTROLLER_ASSIGNMENTS_PASS");
    ParamID bad=0;
    check(mapping->getMidiControllerAssignment(1,0,kCtrlModWheel,bad)
          !=kResultTrue,"P012 rejects nonexistent MIDI bus");
    check(mapping->getMidiControllerAssignment(0,16,kCtrlModWheel,bad)
          !=kResultTrue,"P012 rejects invalid MIDI channel");
    check(mapping->getMidiControllerAssignment(0,0,kCtrlBreath,bad)
          !=kResultTrue,"P012 rejects unsupported CC");

    p.start(7,0,44100.);
    ParameterChanges params(16);
    EventList eventList;
    std::array<float,1024> l{},r{};
    float* channels[2]={l.data(),r.data()};
    AudioBusBuffers output{};output.numChannels=2;output.channelBuffers32=channels;
    ProcessData data{};
    data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
    data.numSamples=1024;data.numOutputs=1;data.outputs=&output;
    data.inputEvents=&eventList;data.inputParameterChanges=&params;
    const auto put=[&](int ch,CtrlNumber cc,int32 offset,ParamValue value){
        ParamID target=0;int32 idx=0;
        check(mapping->getMidiControllerAssignment(0,static_cast<int16>(ch),cc,target)
              ==kResultTrue,"P012 lookup before adding MIDI control point");
        auto* q=params.addParameterData(target,idx);
        check(q!=nullptr,"P012 add mapped controller queue");
        check(q->addPoint(offset,value,idx)==kResultOk,
              "P012 mapped controller sample point");
    };
    // Two nontrivial values of the SAME controller at different sample
    // offsets and pitchbend on ANOTHER channel in the SAME callback.
    put(2,kCtrlSustainOnOff,21,1.);
    put(2,kCtrlSustainOnOff,877,0.);
    put(15,kPitchBend,511,1.);
    put(0,kCtrlModWheel,60,.6);
    put(4,kCtrlRPNSelectMSB,3,0.);
    put(4,kCtrlRPNSelectLSB,4,0.);
    put(4,kCtrlDataEntryMSB,5,12./127.);
    put(6,kCtrlNRPNSelectMSB,10,1./127.);
    put(6,kCtrlNRPNSelectLSB,11,2./127.);
    put(6,kCtrlDataEntryMSB,12,99./127.);
    put(8,kCtrlBankSelectMSB,13,1./127.);
    put(8,kCtrlBankSelectLSB,14,0.);
    check(p.processor->process(data)==kResultOk,
          "P012 actual VST3 sample-offset MIDI controller processing");
    MemoryStream state;
    check(p.component->getState(&state)==kResultOk &&
          state.getSize()==sizeof(Saved),
          "P012 controller state getState output size");
    Saved saved{};
    std::memcpy(&saved,state.getData(),sizeof(saved));
    check(saved.midiStates[2*8+4]==0u,
          "P012 final CC64 value not preserved in project state");
    check(saved.midiStates[15*8+7]==16383u,
          "P012 14-bit pitchbend control not preserved");
    check(saved.midiStates[0*8+0]==76u,
          "P012 CC1 modulation value not preserved");
    check(saved.midiStates[1*8+0]==0xFFFFu,
          "P012 unrelated channel modulation was accidentally modified");
    check(saved.advancedStates[4*8+2]==0u &&
          saved.advancedStates[4*8+3]==0u &&
          saved.advancedStates[4*8+4]==12u &&
          saved.parameterModes[4]==1u,
          "P013 RPN pitchbend range state");
    check(saved.advancedStates[6*8+6]==1u &&
          saved.advancedStates[6*8+7]==2u &&
          saved.parameterModes[6]==2u,
          "P013 NRPN selector state");
    check(saved.advancedStates[8*8+0]==1u &&
          saved.advancedStates[8*8+1]==0u,
          "P013 CC0/CC32 bank state");
    auto reopened=Plugin(factory,id,host);
    MemoryStream savedStream;int32 bytes=0;
    check(savedStream.write(&saved,sizeof(saved),&bytes)==kResultOk&&
          bytes==sizeof(saved),"P012 write reopened controller state");
    check(savedStream.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,
          "P012 rewind reopened controller state");
    check(reopened.component->setState(&savedStream)==kResultOk,
          "P012 MIDI controller state reopening failed");
    MemoryStream reproduced;
    check(reopened.component->getState(&reproduced)==kResultOk &&
          reproduced.getSize()==sizeof(saved) &&
          std::memcmp(reproduced.getData(),&saved,sizeof(saved))==0,
          "P012 project lost per-channel CC or pitchbend on reopen");
    std::puts("P013_REAL_VST3_BANK_RPN_NRPN_CONTROLLER_STATE_PASS");
    std::puts("P012_REAL_STEINBERG_VST3_IMIDIMAPPING_128_ASSIGNMENTS_SAMPLE_OFFSETS_STATE_RECALL_PASS");
}

// P014: actual VST3 host automation queue, including multiple patch points
// in the SAME audio block. This public gate needs no fake or private ROM.
void verifyP014PatchAutomationState(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host){
 auto p=Plugin(factory,id,host);p.start(7,0,44100.);
 ParameterChanges params(8);
 EventList notes;
 std::array<float,frames> l{},r{};
 float* ptrs[2]={l.data(),r.data()};
 AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=ptrs;
 ProcessData data{};
 data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
 data.numSamples=frames;data.numOutputs=1;data.outputs=&out;
 data.inputEvents=&notes;data.inputParameterChanges=&params;
 const auto write=[&](ParamID param,int at,ParamValue value){
    int32 queue=0,point=0;
    auto* q=params.addParameterData(param,queue);
    check(q && q->addPoint(at,value,point)==kResultOk,
          "P014 host patch automation enqueue");
 };
 // Before first note: CH1 program7. Later in SAME block: 3, then 0.
 // Bank1 and CH2/CH16 are independently automatable at other offsets.
 write(1000u,311,3./127.);
 write(1000u,900,0.);
 write(1002u,880,.5);
 write(1201u,120,11./127.);
 write(1201u,870,15./127.);
 write(1301u,650,1.);
 write(1215u,490,27./127.);
 Event e{};e.type=Event::kNoteOnEvent;e.sampleOffset=40;
 e.noteOn.channel=0;e.noteOn.pitch=60;e.noteOn.velocity=.75f;
 check(notes.addEvent(e)==kResultOk,"P014 MIDI note with automation");
 check(p.processor->process(data)==kResultOk,
       "P014 actual VST3 sample-offset bank/program change");
 MemoryStream stream;
 check(p.component->getState(&stream)==kResultOk &&
       stream.getSize()==sizeof(Saved),"P014 post-block VST3 state");
 Saved state{};std::memcpy(&state,stream.getData(),sizeof(state));
 check(state.program==0u&&state.bank==1u,
       "P014 CH1 multiple program/bank points not applied");
 check(state.otherPrograms[0]==15u&&state.otherBanks[0]==1u,
       "P014 CH2 patch automation changed a different channel");
 check(state.otherPrograms[14]==27u&&state.otherBanks[14]==0u,
       "P014 CH16 automation lost or leaked bank");
 std::puts("P014_PUBLIC_REAL_WINDOWS_VST3_TIMED_16_CHANNEL_PATCH_AUTOMATION_STATE_PASS");
}

void verifyP020True64FrameHostLatency(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host){
 // P020: production-prototype chip window is now 64 44.1k frames.
 // Do NOT report zero or retain the old 980-frame P019 host value.
 constexpr double realChipFifoFrames=64.;
 for(const double hz:{8000.,44100.,48000.,96000.,192000.}){
    auto p=Plugin(factory,id,host);
    p.start(7,0,hz);
    const uint32 expected=static_cast<uint32>(
        std::ceil(realChipFifoFrames*hz/44100.));
    const uint32 actual=p.processor->getLatencySamples();
    check(actual==expected,
       "P020 actual Win64 VST3 latency is not its real 64-frame chip FIFO");
    std::printf("P020_PUBLIC_ACTUAL_WIN64_VST3_64_FRAME_LATENCY_PASS"
                " sample_rate=%.0f latency_samples=%u\n",hz,actual);
 }
}

void verifyP015PublicActiveRestore(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host){
 // The real Windows VST3 host must support setState on an active processor
 // and re-upgrade an old v1 project without inventing CC/NRPN settings.
 Plugin p(factory,id,host);p.start(7,0,44100.);
 ParameterChanges changes(1);
 EventList notes;
 std::array<float,frames> l{},right{};
 float* ptrs[2]={l.data(),right.data()};
 AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=ptrs;
 ProcessData data{};data.processMode=kRealtime;
 data.symbolicSampleSize=kSample32;data.numSamples=frames;
 data.numOutputs=1;data.outputs=&bus;data.inputParameterChanges=&changes;
 data.inputEvents=&notes;
 int32 qi=0,pi=0;
 auto* q=changes.addParameterData(2000u+1u,qi); // CH1 CC7
 check(q&&q->addPoint(0,0.,pi)==kResultOk,
       "P015 active host CC7=0 before switching projects");
 check(p.processor->process(data)==kResultOk,
       "P015 active host mapped mute");
 const auto snapshot=[&](){
   MemoryStream saved;Saved value{};
   check(p.component->getState(&saved)==kResultOk &&
         saved.getSize()==sizeof(value),
         "P015 get active v4 state");
   std::memcpy(&value,saved.getData(),sizeof(value));
   return value;
 };
 const auto dirty=snapshot();
 check(dirty.midiStates[1]==0u,"P015 active old project is not muted");
 struct Legacy {
   uint32 magic=0x39303050u,version=1u,program=7u,bank=0u;
   float master=1.f;
 } legacy{};
 const auto restore=[&](const void* source,int32 bytes,bool expected){
   MemoryStream in;int32 wrote=0;
   check(in.write(const_cast<void*>(source),bytes,&wrote)==kResultOk &&
         wrote==bytes,"P015 live project state write");
   check(in.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,
         "P015 live project state seek");
   check((p.component->setState(&in)==kResultOk)==expected,
         "P015 active project accept/reject");
 };
 restore(&legacy,sizeof(legacy),true);
 auto clean=snapshot();
 check(clean.version==4u && clean.midiStates[1]==0xFFFFu &&
       clean.program==7u && clean.bank==0u,
       "P015 active legacy reopen retained previously muted CC7");
 for(auto val:clean.advancedStates)
   check(val==0xFFFFu,"P015 active legacy invented an NRPN");
 restore(&dirty,sizeof(dirty),true);
 check(snapshot().midiStates[1]==0u,
       "P015 active v4 state lost old CC7");
 auto corrupt=dirty;corrupt.midiStates[1]=256u;
 restore(&corrupt,sizeof(corrupt),false);
 check(snapshot().midiStates[1]==0u,
       "P015 corrupt live state unexpectedly modified processor");
 restore(&legacy,sizeof(legacy),true);
 check(snapshot().midiStates[1]==0xFFFFu,
       "P015 second legacy restore retained ghost CC7");
 std::puts("P015_PUBLIC_REAL_WIN64_VST3_ACTIVE_V1_V4_PROJECT_RECALL_PASS");
}

struct Stats {
 double rms=0,peak=0;
 unsigned clippedSamples=0;
 uint64_t hash=14695981039346656037ull;
 std::vector<int16_t> audio;
};
Stats render(const VST3::Hosting::PluginFactory& factory,
             const VST3::UID& id,FUnknown* host,
             const char* label,int hz,int ch,int key,int velocity,
             int program,int bank,bool save=false,
             int mappedController=-1,int mappedChannel=0,
             double mappedValue=0.,int mappedFrame=400,int rpnRange=-1) {
 Plugin p(factory,id,host);p.start(program,bank,hz);
 Stats s;
 std::array<float,frames> left{},right{};
 float* chan[2]={left.data(),right.data()};
 AudioBusBuffers output{};output.numChannels=2;output.channelBuffers32=chan;
 EventList ev;
 ParameterChanges midiChanges(5);
 ParamID mappedID=0;
 if(mappedController>=0){
    IMidiMapping* raw=nullptr;
    check(p.controller->queryInterface(IMidiMapping::iid,
       reinterpret_cast<void**>(&raw))==kResultOk && raw,
       "P012 real host pitch bend mapping query");
    const IPtr<IMidiMapping> mapping=owned(raw);
    check(mapping->getMidiControllerAssignment(0,
       static_cast<int16>(mappedChannel),
       static_cast<CtrlNumber>(mappedController),mappedID)==kResultTrue,
       "P012 original audio controller not mapped");
 }
 ProcessData data{};
 data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
 data.numSamples=frames;data.numOutputs=1;data.outputs=&output;data.inputEvents=&ev;
 data.inputParameterChanges=&midiChanges;
 double sum=0;unsigned count=0;
 for(int b=0;b<34;++b){
   ev.clear();
   midiChanges.clearQueue();
   if(b==0 && mappedController>=0){
      int32 parameterIndex=0,pointIndex=0;
      auto* queue=midiChanges.addParameterData(mappedID,parameterIndex);
      check(queue && queue->addPoint(mappedFrame,mappedValue,pointIndex)==kResultOk,
            "P012 original audio mapped controller point was lost");
   }
   if(b==0 && rpnRange>=0){
      IMidiMapping* raw=nullptr;
      check(p.controller->queryInterface(IMidiMapping::iid,
            reinterpret_cast<void**>(&raw))==kResultOk && raw,
            "P013 genuine host cannot query RPN mapping");
      const IPtr<IMidiMapping> mapping=owned(raw);
      constexpr std::array<CtrlNumber,3> rpn{
          kCtrlRPNSelectMSB,kCtrlRPNSelectLSB,kCtrlDataEntryMSB
      };
      for(unsigned i=0;i<3;++i){
          ParamID mapped=0;int32 queueIndex=0,pointIndex=0;
          check(mapping->getMidiControllerAssignment(0,static_cast<int16>(ch),
                  rpn[i],mapped)==kResultTrue,
                  "P013 missing authentic RPN mapped controller");
          auto* q=midiChanges.addParameterData(mapped,queueIndex);
          check(q && q->addPoint(static_cast<int32>(i),
                          i==2?static_cast<double>(rpnRange)/127.:0.,
                          pointIndex)==kResultOk,
               "P013 failed to queue RPN data before original ROM NoteOn");
      }
   }
   if(b==0||b==13){
     Event e{};e.sampleOffset=b==0?12:511;e.busIndex=0;
     if(b==0){e.type=Event::kNoteOnEvent;e.noteOn.channel=ch;
         e.noteOn.pitch=key;e.noteOn.velocity=velocity/127.f;}
     else{e.type=Event::kNoteOffEvent;e.noteOff.channel=ch;e.noteOff.pitch=key;}
     check(ev.addEvent(e)==kResultOk,"VST3 real MIDI event submission");
   }
   left.fill(-999.f);right.fill(999.f);
   check(p.processor->process(data)==kResultOk,"VST3 real process failed");
   for(int k=0;k<frames;++k){
     for(const float sample :{left[k],right[k]}){
       check(std::isfinite(sample)&&std::abs(sample)<=1.01f,"nonfinite/clipped PCM");
       sum+=static_cast<double>(sample)*sample;
       s.peak=std::max(s.peak,std::abs(static_cast<double>(sample)));
       if(std::abs(sample)>=0.999999f)++s.clippedSamples;
       uint32_t bits=0;std::memcpy(&bits,&sample,4);
       s.hash=(s.hash^bits)*1099511628211ull;
       if(save) s.audio.push_back(static_cast<int16_t>(std::clamp(
           std::lround(sample*32768.f),-32768l,32767l)));
       ++count;
     }
   }
 }
 s.rms=std::sqrt(sum/count);
 std::printf("P010_ACTUAL_VST3_PCM %s hz=%d ch=%d bank=%d program=%d note=%d"
             " velocity=%d rms=%.9f peak=%.9f hash=%016llX latency=%u clipped=%u\n",
             label,hz,ch+1,bank,program,key,velocity,s.rms,s.peak,
             static_cast<unsigned long long>(s.hash),p.processor->getLatencySamples(),
             s.clippedSamples);
 std::fflush(stdout);
 return s;
}
// P014 private ONLY: original EMU8000 voice starts with the patch in
// effect at NoteOn (sample 12), NOT the parameter's block-final value.
// No synthetic tone or code shortcut. Both instances must actually render.
uint64_t p014TimedOriginalPCM(const VST3::Hosting::PluginFactory& factory,
       const VST3::UID& id,FUnknown* host,int program,int bank,
       ParamID automationId,ParamValue target,int automationFrame,
       bool automate,int noteChannel=0){
 Plugin p(factory,id,host);p.start(program,bank,44100.);
 ParameterChanges params(3);
 EventList notes;
 std::array<float,frames> l{},r{};
 float* ptrs[2]={l.data(),r.data()};
 AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=ptrs;
 ProcessData data{};
 data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
 data.numSamples=frames;data.numOutputs=1;data.outputs=&out;
 data.inputEvents=&notes;data.inputParameterChanges=&params;
 uint64_t hash=14695981039346656037ull;
 double sum=0.;
 for(int block=0;block<21;++block){
     notes.clear();params.clearQueue();
     if(block==0){
         Event e{};e.type=Event::kNoteOnEvent;e.sampleOffset=12;
         e.noteOn.channel=noteChannel;e.noteOn.pitch=60;
         e.noteOn.velocity=100.f/127.f;
         check(notes.addEvent(e)==kResultOk,"P014 genuine MIDI NoteOn");
         if(automate){
             int32 qIndex=0,pIndex=0;
             auto* q=params.addParameterData(automationId,qIndex);
             check(q && q->addPoint(automationFrame,target,pIndex)==kResultOk,
                   "P014 real VST3 host automated patch point");
         }
     }
     if(block==13){
         Event off{};off.type=Event::kNoteOffEvent;off.sampleOffset=511;
         off.noteOff.channel=noteChannel;off.noteOff.pitch=60;
         check(notes.addEvent(off)==kResultOk,"P014 genuine MIDI NoteOff");
     }
     check(p.processor->process(data)==kResultOk,
           "P014 original chip timed patch audio callback");
     for(int i=0;i<frames;++i){
         for(float sample:{l[i],r[i]}){
             check(std::isfinite(sample),"P014 nonfinite original PCM");
             sum+=static_cast<double>(sample)*sample;
             uint32 bits=0;std::memcpy(&bits,&sample,sizeof(bits));
             hash=(hash^bits)*1099511628211ull;
         }
     }
 }
 check(sum>0.00001,"P014 original chip silent during timed patch test");
 return hash;
}
void verifyP014PrivateTimedPCM(const VST3::Hosting::PluginFactory& factory,
       const VST3::UID& id,FUnknown* host){
 // Program 7 is selected when NoteOn begins; the Program 0 command
 // at frame 800 MUST NOT retroactively change that voice.
 const auto unchanged=p014TimedOriginalPCM(factory,id,host,
     7,0,1000u,0.,800,false);
 const auto later=p014TimedOriginalPCM(factory,id,host,
     7,0,1000u,0.,800,true);
 const auto earlier=p014TimedOriginalPCM(factory,id,host,
     7,0,1000u,0.,0,true);
 check(later==unchanged,
       "P014 late ProgramChange retroactively changed an active voice");
 check(earlier!=unchanged,
       "P014 program change before NoteOn did not change ORIGINAL PCM");
 // Same proof for owner SF2 bank1 versus authentic ROM bank0.
 const auto rom=p014TimedOriginalPCM(factory,id,host,
     7,0,1002u,.5,800,false);
 const auto bankLater=p014TimedOriginalPCM(factory,id,host,
     7,0,1002u,.5,800,true);
 const auto bankEarlier=p014TimedOriginalPCM(factory,id,host,
     7,0,1002u,.5,0,true);
 check(bankLater==rom,
       "P014 late bank1 automation retroactively changed ROM voice");
 check(bankEarlier!=rom,
       "P014 bank1 before NoteOn not audible via original owner SF2");
 // A CH2 ProgramChange must not modify CH1's already sounding voice.
 const auto unrelated=p014TimedOriginalPCM(factory,id,host,
     7,0,1201u,0.,800,true);
 check(unrelated==rom,
       "P014 CH2 patch event changed CH1 original ROM PCM");
 std::puts("P014_PRIVATE_ORIGINAL_EMU8000_SF2_ROM_SAMPLE_OFFSET_PATCH_PCM_PASS");
}

// P015 real owner-ROM audio gate: restoring a project into an ALREADY
// active VST3 must sound identical whether that instance was freshly
// initialized or previously held unrelated controllers from another song.
// Public CI runs with no owner's licensed ROM; this gate is PRIVATE only.
void p015Restore(Plugin& p,const Saved& saved){
 MemoryStream stream;int32 size=0;
 const int32 bytes=saved.version==1u?20:static_cast<int32>(sizeof(saved));
 check(stream.write(const_cast<Saved*>(&saved),bytes,&size)==kResultOk&&size==bytes,
       "P015 write owner real VST3 project state");
 check(stream.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,
       "P015 seek owner project");
 check(p.component->setState(&stream)==kResultOk,
       "P015 live VST3 project restore");
}
uint64_t p015Play(Plugin& p,int channel,int key){
 EventList notes;
 std::array<float,frames> l{},r{};
 float* buffers[2]={l.data(),r.data()};
 AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=buffers;
 ProcessData data{};data.processMode=kRealtime;
 data.symbolicSampleSize=kSample32;data.numSamples=frames;
 data.numOutputs=1;data.outputs=&bus;data.inputEvents=&notes;
 uint64_t hash=14695981039346656037ull;
 double signalEnergy=0.;
 for(int block=0;block<29;++block){
    notes.clear();
    if(block==0||block==12){
        Event event{};event.busIndex=0;event.sampleOffset=block?511:12;
        if(block==0){
            event.type=Event::kNoteOnEvent;
            event.noteOn.channel=channel;event.noteOn.pitch=key;
            event.noteOn.velocity=100.f/127.f;
        }else{
            event.type=Event::kNoteOffEvent;
            event.noteOff.channel=channel;event.noteOff.pitch=key;
        }
        check(notes.addEvent(event)==kResultOk,"P015 actual owner-ROM MIDI");
    }
    check(p.processor->process(data)==kResultOk,
          "P015 real owner-ROM process after project reopen");
    for(int n=0;n<frames;++n)
       for(const float v:{l[n],r[n]}){
          check(std::isfinite(v),"P015 nonfinite original PCM");
          signalEnergy+=static_cast<double>(v)*v;
          uint32 bits=0;std::memcpy(&bits,&v,sizeof(bits));
          hash=(hash^bits)*1099511628211ull;
       }
 }
 check(signalEnergy>0.00001,"P015 silent real owner-ROM after project recall");
 return hash;
}
void verifyP015PrivateProjectPCM(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,int selectedBank){
 Saved target{};target.version=4u;target.program=7u;
 target.bank=static_cast<uint32_t>(selectedBank);
 target.master=.875f;
 target.midiStates.fill(0xFFFFu);target.advancedStates.fill(0xFFFFu);
 target.otherPrograms[14]=7u;
 target.otherBanks[14]=static_cast<std::uint8_t>(selectedBank);
 // Preserve a nontrivial RPN bend range and mod/volume on CH1; CH16
 // deliberately has UNSET volume so stale CC7=0 would cause silence.
 target.midiStates[0]=52u;
 target.midiStates[7]=10300u;
 target.advancedStates[2]=0u;
 target.advancedStates[3]=0u;
 target.advancedStates[4]=12u;
 target.parameterModes[0]=1u;
 Saved dirty=target;
 dirty.program=0u;dirty.otherPrograms[14]=0u;
 dirty.bank=static_cast<uint32_t>(1-selectedBank);
 dirty.otherBanks[14]=static_cast<std::uint8_t>(1-selectedBank);
 dirty.midiStates[1]=0u;                 // CH1 volume MUTE
 dirty.midiStates[4]=127u;               // sustain on
 dirty.midiStates[5]=127u;               // reverb send
 dirty.midiStates[6]=127u;               // chorus send
 dirty.midiStates[7]=16383u;             // full positive bend
 dirty.midiStates[15*8+1]=0u;            // CH16 independently MUTE
 dirty.advancedStates[4]=24u;            // bend range left by old song
 auto fresh=Plugin(factory,id,host);fresh.start(7,selectedBank,44100.);
 auto used=Plugin(factory,id,host);used.start(7,selectedBank,44100.);
 p015Restore(fresh,target);
 p015Restore(used,dirty);
 p015Restore(used,target);
 for(const int channel:{0,15}){
     const auto expected=p015Play(fresh,channel,60);
     const auto got=p015Play(used,channel,60);
     check(expected==got,
           "P015 RECALL PCM DIFFERENT: stale old-project CC or RPN survived");
 }
 std::printf("P015_PRIVATE_ORIGINAL_EMU8000_ACTIVE_V4_BANK%d_PROJECT_RECALL_PCM_EQUAL_PASS\n",selectedBank);
 // Legacy 20-byte state has NO controller data. It must start clean even
 // after a modern v4 project has previously muted the active synth.
 Saved legacy{};legacy.version=1u;legacy.program=7u;legacy.bank=0u;
 legacy.master=.75f;
 auto oldFresh=Plugin(factory,id,host);oldFresh.start(7,0,44100.);
 auto oldDirty=Plugin(factory,id,host);oldDirty.start(7,0,44100.);
 p015Restore(oldFresh,legacy);
 p015Restore(oldDirty,dirty);
 p015Restore(oldDirty,legacy);
 check(p015Play(oldFresh,0,60)==p015Play(oldDirty,0,60),
       "P015 legacy V1 project did not clear previous modern controller state");
 std::puts("P015_PRIVATE_ORIGINAL_EMU8000_LIVE_LEGACY_V1_PCM_EQUAL_PASS");
}

void wav(const char* path,const Stats& s,int sampleRate=44100){
 check(!s.audio.empty(),"empty real audio WAV");
 std::ofstream f(path,std::ios::binary);
 check(bool(f),"open original PCM WAV");
 const auto w16=[&](uint16_t v){const char b[2]={char(v),char(v>>8)};f.write(b,2);};
 const auto w32=[&](uint32_t v){const char b[4]={char(v),char(v>>8),char(v>>16),char(v>>24)};f.write(b,4);};
 const uint32_t n=static_cast<uint32_t>(s.audio.size()*2);
 f.write("RIFF",4);w32(n+36);f.write("WAVEfmt ",8);
 w32(16);w16(1);w16(2);w32(sampleRate);w32(sampleRate*4);w16(4);w16(16);
 f.write("data",4);w32(n);
 f.write(reinterpret_cast<const char*>(s.audio.data()),n);
 check(bool(f),"write original PCM WAV");
}
void audible(const Stats& s,const char* name){
 if(s.rms<=0.00005||s.peak<=0.0001){
   std::fprintf(stderr,"P010_ROM_OR_SF2_SILENT %s rms=%.9f peak=%.9f\n",name,s.rms,s.peak);
   throw std::runtime_error("original PCM missing or below signal threshold");
 }
}
// P018: Read exact real SF2 phdr MIDI program IDs from the owner bank.
 // Never assume a 77-preset bank occupies MIDI programs 0..76: that would
 // accept a completely unrelated ROM fallback and miss the true last patch.
std::set<int> exactOwnerSf2Programs(const fs::path& file){
    const auto n=fs::file_size(file);
    check(n>=12 && n<=64u*1024u*1024u,
          "P018 owner SoundFont file size invalid");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(n));
    std::ifstream stream(file,std::ios::binary);
    check(bool(stream) && bool(stream.read(
          reinterpret_cast<char*>(bytes.data()),
          static_cast<std::streamsize>(bytes.size()))),
          "P018 cannot read full owner SoundFont");
    const auto tag=[&](std::size_t at,const char* match){
        return at+4<=bytes.size() &&
               std::memcmp(bytes.data()+at,match,4)==0;
    };
    const auto u16=[&](std::size_t at)->std::uint16_t {
        check(at+2<=bytes.size(),"P018 SF2 truncated u16");
        return static_cast<std::uint16_t>(bytes[at]|
               (static_cast<unsigned>(bytes[at+1])<<8));
    };
    const auto u32=[&](std::size_t at)->std::uint32_t {
        check(at+4<=bytes.size(),"P018 SF2 truncated u32");
        return static_cast<std::uint32_t>(bytes[at]|
               (static_cast<unsigned>(bytes[at+1])<<8)|
               (static_cast<unsigned>(bytes[at+2])<<16)|
               (static_cast<unsigned>(bytes[at+3])<<24));
    };
    check(tag(0,"RIFF") && tag(8,"sfbk"),
          "P018 private owner file is not RIFF SoundFont2");
    const std::size_t riffEnd=8u+static_cast<std::size_t>(u32(4));
    check(riffEnd>=12u && riffEnd<=bytes.size(),
          "P018 malformed RIFF size");
    std::set<int> result;
    bool foundPhdr=false;
    for(std::size_t outer=12;outer+8<=riffEnd;){
        const std::size_t size=u32(outer+4);
        check(size<=riffEnd-(outer+8u),"P018 malformed RIFF chunk");
        const std::size_t next=outer+8+size;
        if(tag(outer,"LIST") && size>=4 && tag(outer+8,"pdta")){
            for(std::size_t inner=outer+12;inner+8<=next;){
                const std::size_t sub=u32(inner+4);
                check(sub<=next-(inner+8u),"P018 malformed pdta chunk");
                if(tag(inner,"phdr")){
                    check(!foundPhdr && sub>=76 && sub%38u==0,
                          "P018 invalid or duplicate SF2 phdr table");
                    foundPhdr=true;
                    const std::size_t count=sub/38u;
                    const std::size_t end=inner+8u;
                    for(std::size_t i=0;i<count;++i){
                        const std::size_t row=end+i*38u;
                        const auto preset=u16(row+20u);
                        const auto bank=u16(row+22u);
                        if(i==count-1){
                            check(std::memcmp(bytes.data()+row,"EOP",3)==0,
                                  "P018 SF2 phdr terminal EOP missing");
                            continue;
                        }
                        check(bank==0u && preset<=127u,
                              "P018 unexpected owner SF2 native bank/program");
                        check(result.insert(static_cast<int>(preset)).second,
                              "P018 duplicate owner SF2 program");
                    }
                }
                inner+=8u+sub+(sub&1u);
                check(inner<=next,"P018 malformed pdta padding");
            }
        }
        outer=next+(size&1u);
        check(outer<=riffEnd,"P018 malformed RIFF padding");
    }
    check(foundPhdr && result.size()==77u,
          "P018 owner SF2 does not contain exactly 77 real presets");
    return result;
}
void ownerBank(const fs::path& folder,const char* name){
 const auto dest=folder/"user.sf2";
 std::error_code ec;
 if(!name){fs::remove(dest,ec);return;}
 fs::copy_file(folder/name,dest,fs::copy_options::overwrite_existing,ec);
 if(ec)throw std::runtime_error("owner SF2 hot replacement failed "+ec.message());
}
}
int main(int argc,char** argv){
 try{
  check(argc==3&&(std::strcmp(argv[2],"--public")==0||
      std::strcmp(argv[2],"--private")==0),
      "Usage: P010NativeHostQA.exe <P009-vst3-bundle> --public|--private");
  const bool priv=std::strcmp(argv[2],"--private")==0;
  const fs::path bundle=fs::absolute(argv[1]);
  check(fs::is_directory(bundle),"actual native VST3 bundle missing");
  const auto resources=bundle/"Contents"/"Resources"/"P009-Private";
  if(priv){
    for(const char* file:{"awe32.raw","SYNTHGM.SBK","77-presets.sf2","vintage.sf2"})
      check(fs::is_regular_file(resources/file),"original ROM/SBK/2 owner SF2 missing");
    check(fs::file_size(resources/"awe32.raw")==1048576u,"ROM must be genuine 1MiB");
  }else check(!fs::exists(resources/"awe32.raw"),
              "public QA must NEVER use a real owner's licensed ROM");
  std::string err;
  auto module=VST3::Hosting::Module::create(bundle.string(),err);
  if(!module)std::fprintf(stderr,"P010_VST3_MODULE_ERROR %s\n",err.c_str());
  check(bool(module),"failed loading ACTUAL native VST3");
  const auto& factory=module->getFactory();
  const auto classes=factory.classInfos();
  const auto cls=std::find_if(classes.begin(),classes.end(),
       [](const auto& c){return c.category()==kVstAudioEffectClass;});
  check(cls!=classes.end(),"no actual VST3 instrument class");
  auto host=owned(new HostApplication());
  verifyP011MultichannelProjectRecall(factory,cls->ID(),host.get());
  verifyP165GuiV5OwnerAssetState(factory,cls->ID(),host.get());
  verifyP012MidiMapping(factory,cls->ID(),host.get());
  verifyP014PatchAutomationState(factory,cls->ID(),host.get());
  verifyP015PublicActiveRestore(factory,cls->ID(),host.get());
  verifyP020True64FrameHostLatency(factory,cls->ID(),host.get());
  if(!priv){
    const auto x=render(factory,cls->ID(),host.get(),"NO_ROM",
                              44100,0,60,90,7,0);
    check(x.rms==0&&x.peak==0,"no genuine ROM must always be silent");
    std::puts("P010_PUBLIC_REAL_WINDOWS_VST3_HOST_SILENCE_ONLY_PASS");
    return 0;
  }
  ownerBank(resources,nullptr);
  const auto melody=render(factory,cls->ID(),host.get(),
            "ORIGINAL_ROM_MELODIC",44100,0,60,100,7,0,true);
  audible(melody,"ROM melodic");wav("P010-ROM-MELODIC.wav",melody);
  // The actual Host -> IMidiMapping -> VST3 process -> reconstructed
  // SBAWE.VXD -> original 86Box EMU8000 audio path must respond to
  // nontrivial controller values at a precise offset INSIDE the note.
  const auto bent=render(factory,cls->ID(),host.get(),
       "P012_REAL_ROM_PITCHBEND_CH1",44100,0,60,100,7,0,false,
       kPitchBend,0,1.,400);
  audible(bent,"P012 original ROM pitchbend");
  check(bent.hash!=melody.hash,
       "P012 pitchbend mapping produced identical original ROM PCM");
  const auto quieted=render(factory,cls->ID(),host.get(),
       "P012_REAL_ROM_CC7_ZERO_CH1",44100,0,60,100,7,0,false,
       kCtrlVolume,0,0.,400);
  audible(quieted,"P012 original ROM CC7");
  check(quieted.hash!=melody.hash,
       "P012 CC7 volume zero did not affect original VST3 PCM");
  std::puts("P012_PRIVATE_REAL_WIN64_VST3_PITCHBEND_AND_CC7_PCM_CHANGED_PASS");
  // Real owner-ROM waveform pitchbend RPN 0,0 range12 vs default range2,
  // both applied before the note. Never synthesize a pretend emulator tone.
  const auto bendDefault=render(factory,cls->ID(),host.get(),
      "P013_ROM_BEND_RANGE_DEFAULT2",44100,0,60,100,7,0,false,
      kPitchBend,0,1.,11);
  const auto bendRpn12=render(factory,cls->ID(),host.get(),
      "P013_ROM_BEND_RANGE_RPN12",44100,0,60,100,7,0,false,
      kPitchBend,0,1.,11,12);
  audible(bendDefault,"P013 original-ROM bend two semitones");
  audible(bendRpn12,"P013 original-ROM bend twelve semitones");
  check(bendDefault.hash!=bendRpn12.hash,
      "P013 RPN bend range12 did not change actual original ROM PCM");
  std::puts("P013_PRIVATE_ORIGINAL_ROM_RPN_PITCHBEND_RANGE_2_VS_12_PCM_PASS");
  const auto kick=render(factory,cls->ID(),host.get(),
            "ORIGINAL_ROM_KICK_CH10",44100,9,36,100,0,0,true);
  audible(kick,"original ROM kick");wav("P010-ROM-KICK.wav",kick);
  const auto snare=render(factory,cls->ID(),host.get(),
            "ORIGINAL_ROM_SNARE_CH10",44100,9,38,100,0,0,true);
  audible(snare,"original ROM snare");wav("P010-ROM-SNARE.wav",snare);
  const auto browser=render(factory,cls->ID(),host.get(),
            "CH1_AUTO_DRUM_BROWSER",44100,0,36,100,0,2);
  audible(browser,"CH1 drum browser");
  check(browser.hash==kick.hash,"CH1 drum browser and original CH10 PCM do not match");
  for(int ch=0;ch<16;++ch){
    const auto x=render(factory,cls->ID(),host.get(),
       "ROM_16_MIDI_CHANNELS",44100,ch,ch==9?36:60,100,ch==9?0:7,0);
    audible(x,"16-channel MIDI/ROM");
  }
  // Previously the P010 note/channel test could sound on all channels
  // while only CH1 had the chosen program. Explicitly prove CH2 and CH16
  // actually switch between two distinct authentic ROM programs.
  for(int ch:{1,15}){
     const auto seven=render(factory,cls->ID(),host.get(),
         "P011_CHANNEL_PROGRAM7",44100,ch,60,100,7,0);
     const auto zero=render(factory,cls->ID(),host.get(),
         "P011_CHANNEL_PROGRAM0",44100,ch,60,100,0,0);
     audible(seven,"independent Creative CH program7");
     audible(zero,"independent Creative CH program0");
     check(seven.hash!=zero.hash,
         "P011 independent per-channel ROM program selection did not change PCM");
  }
  std::puts("P011_ACTUAL_VST3_CH2_CH16_INDEPENDENT_PROGRAM_CHANGE_PASS");
  std::puts("P010_ORIGINAL_ROM_GM_KICK_SNARE_16_MIDI_CHANNELS_PASS");
  ownerBank(resources,"vintage.sf2");
  const auto first=render(factory,cls->ID(),host.get(),
             "VINTAGE_FIRST",44100,0,60,100,7,1,true);
  audible(first,"Vintage Dreams initial");wav("P010-SF2-VINTAGE-FIRST.wav",first);
  ownerBank(resources,"77-presets.sf2");
  const auto other=render(factory,cls->ID(),host.get(),
             "77SF2_PROGRAM7",44100,0,60,100,7,1,true);
  audible(other,"77 preset SF2");wav("P010-SF2-77-PRESETS.wav",other);
  verifyP014PrivateTimedPCM(factory,cls->ID(),host.get());
  verifyP015PrivateProjectPCM(factory,cls->ID(),host.get(),0);
  verifyP015PrivateProjectPCM(factory,cls->ID(),host.get(),1);
  // Per-channel CC0 should select the *same owner SF2* as the browser's
  // bank1, before the first NoteOn at sample12.
  const auto viaBankCC=render(factory,cls->ID(),host.get(),
      "P013_OWNER_SF2_BANK1_VIA_CC0",44100,0,60,100,7,0,false,
      kCtrlBankSelectMSB,0,1./127.,0);
  audible(viaBankCC,"P013 owner SF2 via bank CC0");
  check(viaBankCC.hash==other.hash,
      "P013 bank CC0 and SF2 browser bank1 produce different original PCM");
  std::puts("P013_PRIVATE_OWNER_SF2_BANK_SELECT_CC0_MATCHES_BANK1_PCM_PASS");
  check(first.hash!=other.hash,"both owner SF2s share the same PCM fingerprint");
  // P018: enumerate the ACTUAL phdr entries, never a fabricated contiguous
  // 0..76 range. This owner SF2 has program 77 but NO program 63.
  const auto actualPrograms=exactOwnerSf2Programs(resources/"77-presets.sf2");
  check(actualPrograms.count(63)==0u && actualPrograms.count(77)==1u,
        "P018 original owner SF2's program hole 63/last 77 altered");
  std::printf("P018_OWNER_SF2_EXACT_PHDR_77_REAL_PROGRAMS"
              " missing63=%d includes77=%d\n",
              int(actualPrograms.count(63)==0u),
              int(actualPrograms.count(77)==1u));
  std::set<uint64_t> unique;
  unsigned clippedTotal=0;
  unsigned affectedPresets=0;
  for(const auto prog:actualPrograms){
    const auto result=render(factory,cls->ID(),host.get(),
        "P018_REAL_SF2_PHDR_PRESET",44100,0,60,100,prog,1);
    audible(result,"P018 exact original SF2 preset");
    check(unique.insert(result.hash).second,
          "P018 real owner SF2 two programs share identical PCM at C3");
    if(result.clippedSamples){
        ++affectedPresets;
        clippedTotal+=result.clippedSamples;
        const auto v80=render(factory,cls->ID(),host.get(),
            "P018_HEADROOM_VELOCITY80",44100,0,60,80,prog,1);
        const auto v60=render(factory,cls->ID(),host.get(),
            "P018_HEADROOM_VELOCITY60",44100,0,60,60,prog,1);
        std::printf("P018_OWNER_SF2_PCM_LIMIT program=%d"
           " vel100=%u vel80=%u vel60=%u\n",
           prog,result.clippedSamples,v80.clippedSamples,v60.clippedSamples);
    }
  }
  check(unique.size()==actualPrograms.size(),
        "P018 not all genuine owner SF2 programs were verified");
  std::printf("P018_77_EXACT_SF2_PRESETS_REAL_VST3_PCM_PASS"
              " tested=%u clips=%u affected=%u\n",
              static_cast<unsigned>(unique.size()),clippedTotal,affectedPresets);
  // Explicitly distinguish ROM fallback for missing SF2 program63 from a
  // genuine owner SF2 patch. Never count a fallback toward the 77 presets.
  const auto holeBank=render(factory,cls->ID(),host.get(),
      "P018_MISSING_PROGRAM63_BANK1",44100,0,60,100,63,1);
  const auto holeROM=render(factory,cls->ID(),host.get(),
      "P018_PROGRAM63_ROM_BANK0",44100,0,60,100,63,0);
  check(holeBank.hash==holeROM.hash,
        "P018 missing SF2 program63 is not expected ROM fallback");
  std::puts("P018_MISSING_SF2_PROGRAM63_IS_ROM_FALLBACK_NOT_USER_PRESET_PASS");
  ownerBank(resources,"vintage.sf2");
  const auto returned=render(factory,cls->ID(),host.get(),
             "VINTAGE_RETURN",44100,0,60,100,7,1,true);
  audible(returned,"Vintage Dreams return");
  check(first.hash==returned.hash,"original SF2 A/B/A PCM not reproducible");
  wav("P010-SF2-VINTAGE-RETURN.wav",returned);
  for(int rate:{48000,96000}){
    const auto s=render(factory,cls->ID(),host.get(),
         "ORIGINAL_SF2_RATE_TEST",rate,0,60,100,7,1);
    audible(s,"VST3 multi-rate resampler");
    // P021 NEGATIVE CONTROL: P017/P018/P020 played the secondary,
    // *approximate* synth DSP for 48/96k, not genuine 86Box EMU8000!
    // Assert the new actual Windows VST3 no longer returns those known,
    // stable (but false-core) original-bank PCM hashes. This is a
    // regression check, not a full external hardware PCM reference.
    const std::uint64_t oldFalseCoreHash=(rate==48000)?
        0x48B50DC86A2BC325ULL:0x7E939E3BECD86B25ULL;
    check(s.hash!=oldFalseCoreHash,
       "P021 48/96k VST3 still plays SECONDARY approximate DSP, NOT 86Box");
    std::printf("P021_PRIVATE_REAL_VST3_NON_NATIVE_ORIGINAL_CHIP_AUDIO"
                "_NOT_APPROXIMATE_DSP_PASS hz=%d hash=%016llX\n",
            rate,static_cast<unsigned long long>(s.hash));
  }
  ownerBank(resources,nullptr);
  const auto romAfter=render(factory,cls->ID(),host.get(),
        "ORIGINAL_ROM_KICK_AFTER_ABA",44100,9,36,100,0,0);
  audible(romAfter,"ROM kick after user A/B/A");
  check(kick.hash==romAfter.hash,"ROM kick changed after two SF2 swaps");
  std::puts("P010_PRIVATE_ACTUAL_WIN64_VST3_ORIGINAL_ROM_SBK_TWO_SF2_77_PRESETS_ABA_PASS");
  return 0;
 }catch(const std::exception& e){
   std::fprintf(stderr,"P010_ACTUAL_WIN64_VST3_ORIGINAL_AUDIO_FAIL %s\n",e.what());
   return 1;
 }
}
