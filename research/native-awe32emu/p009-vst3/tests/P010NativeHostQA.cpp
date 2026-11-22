// P010 actual Windows x64 Steinberg VST3 original PCM owner acceptance.
// No guest Win95, 86Box VM, fabricated tones, Studio One, or driver DLL.
// --public with NO private files expects SILENCE, not a claimed audio pass.
// --private with original ROM + Creative SBK + owner SF2s tests REAL AUDIO.
#define NOMINMAX
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "P009AssetFingerprint.hpp"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <atomic>
#include <thread>
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
#include <windows.h>
#include <psapi.h>
#include <utility>
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
void verifyZeroFrameParameterFlush(const VST3::Hosting::PluginFactory& factory,
                                   const VST3::UID& id,FUnknown* host){
  Plugin plugin(factory,id,host);
  plugin.start(0,0,44100.);
  const auto mapping=FUnknownPtr<IMidiMapping>(plugin.controller);
  check(bool(mapping),"flush: MIDI mapping unavailable");
  ParamID sustain=0,bankMsb=0;
  check(mapping->getMidiControllerAssignment(0,15,kCtrlSustainOnOff,sustain)==kResultTrue &&
        mapping->getMidiControllerAssignment(0,14,kCtrlBankSelectMSB,bankMsb)==kResultTrue,
        "flush: channel-local MIDI parameter mapping missing");
  ParameterChanges changes(5);
  AudioBusBuffers output{}; // SDK parameter flush: zero channels, no sample buffers
  ProcessData data{};
  data.numOutputs=1;
  data.outputs=&output;
  data.inputParameterChanges=&changes;
  const auto add=[&](ParamID param,int32 frame,double value){
    int32 queueIndex=0,pointIndex=0;
    auto* queue=changes.addParameterData(param,queueIndex);
    check(queue && queue->addPoint(frame,value,pointIndex)==kResultOk,
          "flush: cannot enqueue parameter point");
  };
  add(1001u,0,.25);  // master
  add(1202u,0,39./127.); // MIDI channel 3 program
  add(1108u,0,.5);  // filter cutoff
  add(sustain,0,1.);
  add(bankMsb,0,1./127.);
  check(plugin.processor->process(data)==kResultOk,"flush: valid callback rejected");
  const auto save=[](Plugin& unit){
    MemoryStream stream;
    check(unit.component->getState(&stream)==kResultOk,"flush: project save failed");
    const auto* bytes=reinterpret_cast<const std::uint8_t*>(stream.getData());
    check(bytes && stream.getSize()>=sizeof(Saved),"flush: missing saved project");
    return std::vector<std::uint8_t>(bytes,bytes+stream.getSize());
  };
  const auto accepted=save(plugin);
  Saved header{};
  std::memcpy(&header,accepted.data(),sizeof(header));
  check(header.master==.25f && header.otherPrograms[1]==39 &&
        header.midiStates[15*8+4]==127 && header.midiStates[14*8+4]==0xFFFFu &&
        header.advancedStates[14*8]==1 && header.advancedStates[15*8]==0xFFFFu,
        "flush: accepted master, program or channel-local MIDI did not reach project state");
  check(plugin.controller->getParamNormalized(1108u)==.5 &&
        plugin.controller->getParamNormalized(1001u)==.25,
        "flush: controller readback differs from accepted controls");
  changes.clearQueue();
  add(1001u,1,.75); // outside the legal flush position
  add(1108u,0,1.25); // invalid normalized value
  check(plugin.processor->process(data)==kResultOk && save(plugin)==accepted,
        "flush: rejected points changed the accepted project");
  Plugin reopened(factory,id,host);
  MemoryStream state;
  int32 written=0;
  check(state.write(const_cast<std::uint8_t*>(accepted.data()),
                    static_cast<int32>(accepted.size()),&written)==kResultOk &&
        written==static_cast<int32>(accepted.size()) &&
        state.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
        reopened.controller->setComponentState(&state)==kResultOk &&
        save(reopened)==accepted &&
        reopened.controller->getParamNormalized(1108u)==.5,
        "flush: fresh controller/processor lost flushed project state");
  std::puts("AWE64_ZERO_FRAME_PARAMETER_FLUSH_STATE_RECALL_PASS");
}

// P169: Genuine real Win64 VST3 controller-side component restore,
// NOT the same interface as IComponent::setState. Verify the source V4
// stream can be applied ONLY via IEditController::setComponentState;
// the single-component processor must then report exact MIDI/RPN and
// program/bank/master state, including a fresh GUI-controller value.
// Negatives must reject and leave all state bit-exact. This would FAIL
// if the handler merely returned kResultOk without applying the project.
void verifyP169ControllerComponentStateAppliedAndRejectsCorrupt(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host){
  Plugin unit(factory,id,host);
  Saved expected{};
  expected.program=39u;expected.bank=1u;expected.master=.625f;
  expected.otherPrograms[0]=17u;expected.otherBanks[0]=1u;
  expected.midiStates.fill(0xFFFFu);
  expected.advancedStates.fill(0xFFFFu);
  expected.advancedStates[2]=0u;
  expected.advancedStates[3]=0u;
  expected.advancedStates[4]=12u;
  expected.parameterModes[0]=1u;
  const auto send=[&](const Saved& state){
    MemoryStream stream;int32 written=0;
    check(stream.write(const_cast<Saved*>(&state),sizeof(state),&written)==kResultOk &&
          written==sizeof(state) &&
          stream.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,
          "P169 source V4 controller component state not transportable");
    return unit.controller->setComponentState(&stream);
  };
  check(send(expected)==kResultOk,
        "P169 actual VST3 controller refused fully valid component state");
  MemoryStream accepted;
  check(unit.component->getState(&accepted)==kResultOk &&
        accepted.getSize()==sizeof(expected) &&
        std::memcmp(accepted.getData(),&expected,sizeof(expected))==0,
        "P169 controller reported success but did not apply original MIDI/RPN/project");
  check(std::abs(unit.controller->getParamNormalized(1000u)-39./127.)<.00001 &&
        std::abs(unit.controller->getParamNormalized(1002u)-.5)<.00001 &&
        std::abs(unit.controller->getParamNormalized(1001u)-.625)<.00001,
        "P169 GUI/controller readback does not match component-only V4 restore");
  Saved invalid=expected;
  invalid.magic^=0x00100000u;
  check(send(invalid)!=kResultOk,
        "P169 corrupt controller component state was falsely accepted");
  MemoryStream unaffected;
  check(unit.component->getState(&unaffected)==kResultOk &&
        unaffected.getSize()==sizeof(expected) &&
        std::memcmp(unaffected.getData(),&expected,sizeof(expected))==0,
        "P169 rejected corrupt project mutated valid prior controller state");
  // A second identical accepted transfer must not destroy a valid
  // project, and must preserve the 16ch RPN set on first application.
  check(send(expected)==kResultOk,
        "P169 idempotent controller component-state reload failed");
  MemoryStream again;
  check(unit.component->getState(&again)==kResultOk &&
        again.getSize()==sizeof(expected) &&
        std::memcmp(again.getData(),&expected,sizeof(expected))==0,
        "P169 duplicate controller restore lost a previous MIDI RPN value");
  std::puts("P169_ACTUAL_WIN64_VST3_CONTROLLER_SET_COMPONENT_STATE_POSITIVE_NEGATIVE_GUI_MIDI_RECALL_PASS");
}

// P166: a stopped DAW may save a project immediately after selecting a
// program/bank/master value, before *any* audio callback or host automation
// delivery. A green VST3 validator/GUI knob test does not exercise this.
// Additionally, same-component automation reaches process() directly; real
// controller readback must show what the chip actually received.
void verifyP166StoppedHostEditsAndAudioReadback(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host){
  Plugin p(factory,id,host); // deliberately INACTIVE
  const auto edit=[&](ParamID param,double normalized){
    check(p.controller->setParamNormalized(param,normalized)==kResultOk,
          "P166 stopped-host parameter edit rejected");
  };
  edit(1000u,7./127.);edit(1002u,.5);edit(1001u,.3125);
  edit(1201u,17./127.);edit(1301u,1.);
  MemoryStream saved;
  check(p.component->getState(&saved)==kResultOk&&
        saved.getSize()==sizeof(Saved),
        "P166 stopped controller edits unexpectedly discarded or changed V4 layout");
  Saved initial{};
  std::memcpy(&initial,saved.getData(),sizeof(initial));
  check(initial.program==7u&&initial.bank==1u&&
        initial.master==.3125f&&
        initial.otherPrograms[0]==17u&&initial.otherBanks[0]==1u,
        "P166 host project save lost program/bank/master when no audio ran");
  auto reopened=Plugin(factory,id,host);
  MemoryStream stream;int32 written=0;
  check(stream.write(&initial,sizeof(initial),&written)==kResultOk &&
        written==sizeof(initial)&&
        stream.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
        reopened.component->setState(&stream)==kResultOk,
        "P166 stopped host project reopen rejected");
  MemoryStream restored;
  check(reopened.component->getState(&restored)==kResultOk &&
        restored.getSize()==sizeof(initial)&&
        std::memcmp(restored.getData(),&initial,sizeof(initial))==0,
        "P166 stopped edits not byte-exact after reopen");
  check(std::abs(reopened.controller->getParamNormalized(1000u)-7./127.)<.001 &&
        std::abs(reopened.controller->getParamNormalized(1002u)-.5)<.001 &&
        std::abs(reopened.controller->getParamNormalized(1001u)-.3125)<.001,
        "P166 controller readback does not reflect freshly restored project");
  // Start actual native VST3 processing; no ROM is required for the
  // MIDI/control readback contract. The audio path is intentionally silent.
  ProcessSetup setup{};setup.processMode=kRealtime;
  setup.symbolicSampleSize=kSample32;setup.sampleRate=44100.;
  setup.maxSamplesPerBlock=frames;
  check(reopened.processor->setupProcessing(setup)==kResultOk &&
        reopened.component->setActive(true)==kResultOk,
        "P166 real Windows VST3 setup after stopped edit failed");
  reopened.active=true;
  check(reopened.processor->setProcessing(true)==kResultOk,
        "P166 real Windows VST3 processing after stopped edit failed");
  reopened.processing=true;
  std::array<float,frames> left{},right{};
  float* channels[2]={left.data(),right.data()};
  AudioBusBuffers output{};output.numChannels=2;output.channelBuffers32=channels;
  ParameterChanges params(8);
  ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
  data.numSamples=frames;data.numOutputs=1;data.outputs=&output;
  data.inputParameterChanges=&params;
  const auto add=[&](ParamID param,double normalized){
    int32 index=0,point=0;
    auto* queue=params.addParameterData(param,index);
    check(queue&&queue->addPoint(10,normalized,point)==kResultOk,
          "P166 host audio-point injection failed");
  };
  add(1000u,21./127.);add(1002u,0.);add(1001u,.75);
  add(1201u,23./127.);add(1301u,0.);
  add(1100u,.45);add(1400u,6./7.);add(1420u,1.);
  check(reopened.processor->process(data)==kResultOk,
        "P166 actual Windows VST3 audio automation rejected");
  check(std::abs(reopened.controller->getParamNormalized(1000u)-21./127.)<.001&&
        std::abs(reopened.controller->getParamNormalized(1002u))<.001&&
        std::abs(reopened.controller->getParamNormalized(1001u)-.75)<.001&&
        std::abs(reopened.controller->getParamNormalized(1201u)-23./127.)<.001&&
        std::abs(reopened.controller->getParamNormalized(1301u))<.001&&
        std::abs(reopened.controller->getParamNormalized(1100u)-.45)<.001&&
        std::abs(reopened.controller->getParamNormalized(1400u)-6./7.)<.001&&
        std::abs(reopened.controller->getParamNormalized(1420u)-1.)<.001,
        "P166 GUI/controller readback is stale after real processor automation");
  MemoryStream processed;
  check(reopened.component->getState(&processed)==kResultOk&&
        processed.getSize()==648u,
        "P166 combined processor automation and GUI control not V5-serializable");
  Saved actual{};std::memcpy(&actual,processed.getData(),sizeof(actual));
  check(actual.program==21u&&actual.bank==0u&&
        actual.master==.75f&&actual.otherPrograms[0]==23u&&
        actual.otherBanks[0]==0u,
        "P166 obsolete controller request overrode later audio automation");
  std::puts("P166_REAL_WIN64_VST3_STOPPED_HOST_PATCH_PROJECT_RECALL_AND_LIVE_GUI_AUTOMATION_READBACK_PASS");
}

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
 auto* original=reinterpret_cast<const std::uint8_t*>(initial.getData());
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
       v5.getSize()==648u,"P165 12 GUI edits did not persist in V5 648-byte owner state");
 const auto* bytes=reinterpret_cast<const std::uint8_t*>(v5.getData());
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
 check(bytes[644]==4u&&bytes[645]==1u &&
       bytes[646]==2u&&bytes[647]==1u,
       "P165 V5 must persist two explicit SEND hardware modes with FX types");
 auto zeroSendProject=Plugin(factory,id,host);
 check(zeroSendProject.controller->setParamNormalized(1110u,0.)==kResultOk &&
       zeroSendProject.controller->setParamNormalized(1111u,0.)==kResultOk,
       "P165 explicit 0% SEND is not an addressable VST3 parameter");
 MemoryStream zeroState;
 check(zeroSendProject.component->getState(&zeroState)==kResultOk &&
       zeroState.getSize()==648u,
       "P165 manual 0% needs V5 explicit SEND-mode state, not V4 DEFAULT");
 auto* z=reinterpret_cast<const std::uint8_t*>(zeroState.getData());
 check(z && z[644]==4u && z[645]==1u &&
       z[646]==2u && z[647]==1u &&
       z[608]==0u && z[609]==0u &&
       z[610]==0u && z[611]==0u,
       "P165 V5 zero SEND register override missing despite manual mode");
 // REAL original hardware FX menus are VST3-automatable, not decorative.
 // Test both menu modes and V5 exact recall independently of any private ROM.
 auto fxUnit=Plugin(factory,id,host);
 check(fxUnit.controller->setParamNormalized(1400u,5./7.)==kResultOk &&
       fxUnit.controller->setParamNormalized(1401u,6./7.)==kResultOk,
       "P165 actual original reverb/chorus TYPE menus not automatable");
 MemoryStream fxState;
 check(fxUnit.component->getState(&fxState)==kResultOk &&
       fxState.getSize()==648u,
       "P165 real FX-type automation not persisted in DAW state");
 const auto* fxBytes=reinterpret_cast<const std::uint8_t*>(fxState.getData());
 check(fxBytes && fxBytes[644]==5u && fxBytes[645]==0u &&
       fxBytes[646]==6u && fxBytes[647]==0u,
       "P165 original hardware FX type values wrong after automation");
 auto fxReopen=Plugin(factory,id,host);
 MemoryStream fxRestore;int32 fxBytesWritten=0;
 check(fxRestore.write(fxState.getData(),648,&fxBytesWritten)==kResultOk &&
       fxBytesWritten==648,"P165 copy hardware FX state");
 check(fxRestore.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
       fxReopen.component->setState(&fxRestore)==kResultOk,
       "P165 actual hardware FX-mode project recall failed");
 MemoryStream fxRoundTrip;
 check(fxReopen.component->getState(&fxRoundTrip)==kResultOk &&
       fxRoundTrip.getSize()==648u &&
       std::memcmp(fxRoundTrip.getData(),fxState.getData(),648u)==0,
       "P165 hardware FX types differ after project reopening");
 std::vector<std::uint8_t> invalidFx(fxBytes,fxBytes+648u);
 invalidFx[644]=8u;
 MemoryStream invalidFxRestore;
 check(invalidFxRestore.write(invalidFx.data(),648,&fxBytesWritten)==kResultOk,
       "P165 construct malformed hardware FX mode");
 check(invalidFxRestore.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
       fxReopen.component->setState(&invalidFxRestore)!=kResultOk,
       "P165 out-of-range original 86Box FX mode wrongly accepted");
 MemoryStream safeFx;
 check(fxReopen.component->getState(&safeFx)==kResultOk &&
       safeFx.getSize()==648u &&
       std::memcmp(safeFx.getData(),fxState.getData(),648u)==0,
       "P165 invalid hardware FX mode corrupted the original state");
 std::puts("P165_REAL_VST3_ORIGINAL_EMU8000_8X_REVERB_CHORUS_TYPE_MENUS_V5_RECALL_PASS");
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
// Independent adversarial V5 contract: V4 MIDI RPN/NRPN state MUST be
// preserved when ANY P165 knob forces V5 serialization. Existing tests
// exercise V4 advanced state OR V5 GUI controls, but never their conjunction.
// No protected ROM or SoundFont assets are needed to observe the failure.
void auditV5DoesNotEraseAdvancedMidiControllers(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host) {
 auto source=Plugin(factory,id,host);
 Saved state{};
 state.midiStates.fill(0xFFFFu);
 state.advancedStates.fill(0xFFFFu);
 state.program=7u;state.bank=1u;
 state.advancedStates[2]=0u; // CH1 RPN MSB
 state.advancedStates[3]=0u; // CH1 RPN LSB
 state.advancedStates[4]=12u; // CH1 Data Entry MSB => bend range12
 state.parameterModes[0]=1u; // RPN mode
 MemoryStream oldState;int32 n=0;
 check(oldState.write(&state,static_cast<int32>(sizeof(state)),&n)==kResultOk &&
       n==static_cast<int32>(sizeof(state)) &&
       oldState.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,
       "AUDIT V4 initial RPN fixture invalid");
 check(source.component->setState(&oldState)==kResultOk,
       "AUDIT V4 accepted RPN fixture must roundtrip before GUI editing");
 MemoryStream control;check(source.component->getState(&control)==kResultOk &&
       control.getSize()==sizeof(Saved) &&
       std::memcmp(control.getData(),&state,sizeof(state))==0,
       "AUDIT positive control: V4 CH1 RPN state does not roundtrip");
 check(source.controller->setParamNormalized(1100u,.375)==kResultOk,
       "AUDIT initial P165 AMP attack gesture rejected");
 MemoryStream saved;
 check(source.component->getState(&saved)==kResultOk && saved.getSize()==648u,
       "AUDIT V5 with P165 plus CH1 RPN needs 648-byte project state");
 auto dest=Plugin(factory,id,host);
 MemoryStream incoming;
 check(incoming.write(saved.getData(),static_cast<int32>(saved.getSize()),&n)==kResultOk &&
       incoming.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
       dest.component->setState(&incoming)==kResultOk,
       "AUDIT V5 restore must accept valid original ROM+RPN metadata");
 MemoryStream actual;
 check(dest.component->getState(&actual)==kResultOk &&
       actual.getSize()==saved.getSize(),"AUDIT restored V5 state missing");
 const bool identical=std::memcmp(
       actual.getData(),saved.getData(),saved.getSize())==0;
 if(!identical){
   const auto* expected=reinterpret_cast<const std::uint8_t*>(saved.getData());
   const auto* got=reinterpret_cast<const std::uint8_t*>(actual.getData());
   unsigned first=0;
   while(first<saved.getSize()&&expected[first]==got[first])++first;
   std::fprintf(stderr,
     "AUDIT_NEGATIVE_V5_RPN_RECALL_LOSS first_differing_byte=%u"
     " input=%u restored=%u\n",first,
     first<saved.getSize()?unsigned(expected[first]):0,
     first<saved.getSize()?unsigned(got[first]):0);
 }
 check(identical,"P165 V5 lost advanced MIDI RPN/NRPN controller state after GUI gesture");
 std::puts("AUDIT_V5_GUI_AND_RPN_INDEPENDENT_PROJECT_RECALL_PASS");
}
// ADVERSE actual Win64 VST3 same-instance concurrency challenge.
// One real processor audio thread calls process at 128 frames continuously;
// the DAW host thread alternates full MIDI/RPN V4 projects while invoking
// getState concurrently. Output must be A or B, never a torn hybrid.
// A public ROM-less instrument is intentional: tests the *host/runtime
// ownership and serialization* without claiming any original PCM fidelity.
void verifyP165ConcurrentHostStateWithLiveProcess(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host){
  Plugin plugin(factory,id,host);
  plugin.start(0,0,48000.);
  Saved a{},b{};
  a.program=7u;a.bank=1u;a.master=.375f;
  b.program=91u;b.bank=0u;b.master=.8125f;
  a.midiStates.fill(0xFFFFu);b.midiStates.fill(0xFFFFu);
  a.advancedStates.fill(0xFFFFu);b.advancedStates.fill(0xFFFFu);
  a.advancedStates[2]=0u;a.advancedStates[3]=0u;
  a.advancedStates[4]=12u;a.parameterModes[0]=1u;
  b.advancedStates[2]=0u;b.advancedStates[3]=0u;
  b.advancedStates[4]=2u;b.parameterModes[0]=1u;
  for(unsigned i=0;i<15u;++i){
    a.otherPrograms[i]=static_cast<std::uint8_t>((i*7u)%128u);
    b.otherPrograms[i]=static_cast<std::uint8_t>((i*11u+9u)%128u);
    a.otherBanks[i]=static_cast<std::uint8_t>(i%2u);
    b.otherBanks[i]=static_cast<std::uint8_t>((i+1u)%2u);
  }
  std::atomic<bool> keepAudio{true},audioOk{true};
  std::atomic<unsigned> audioBlocks{0};
  std::thread audio([&](){
    std::array<float,128> left{},right{};
    float* ptr[2]={left.data(),right.data()};
    AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=ptr;
    ProcessData pd{};
    pd.processMode=kRealtime;pd.symbolicSampleSize=kSample32;
    pd.numSamples=128;pd.numOutputs=1;pd.outputs=&out;
    while(keepAudio.load(std::memory_order_acquire) ||
          audioBlocks.load(std::memory_order_relaxed)<5000u){
        if(plugin.processor->process(pd)!=kResultOk){
            audioOk.store(false,std::memory_order_release);
            break;
        }
        audioBlocks.fetch_add(1u,std::memory_order_relaxed);
    }
  });
  auto pass=true;
  for(int trial=0;trial<400 && pass;++trial){
    auto& expected=trial%2?b:a;
    MemoryStream source;
    int32 bytes=0;
    if(source.write(&expected,sizeof(expected),&bytes)!=kResultOk ||
       bytes!=sizeof(expected) ||
       source.seek(0,IBStream::kIBSeekSet,nullptr)!=kResultOk ||
       plugin.component->setState(&source)!=kResultOk){
       pass=false;break;
    }
    MemoryStream saved;
    if(plugin.component->getState(&saved)!=kResultOk ||
       saved.getSize()!=sizeof(expected)){
       pass=false;break;
    }
    const bool matchesA=std::memcmp(saved.getData(),&a,sizeof(a))==0;
    const bool matchesB=std::memcmp(saved.getData(),&b,sizeof(b))==0;
    if(!matchesA&&!matchesB){pass=false;break;}
  }
  keepAudio.store(false,std::memory_order_release);
  audio.join();
  check(pass,"P165 concurrent getState/setState returned corrupted/torn MIDI/RPN project");
  check(audioOk.load(std::memory_order_acquire),
        "P165 native audio thread failed concurrent full 16-channel host-state restore");
  check(audioBlocks.load(std::memory_order_acquire)>=5000u,
        "P165 same-instance audio-thread scheduler stress was not executed");
  std::puts("P165_REAL_WIN64_VST3_SAME_INSTANCE_5000_BLOCKS_400_PROJECT_STATES_LOCKFREE_AUDIO_PASS");
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
// True private real-asset POSITIVE V5 file identity + original-chip PCM
// across project reopen. NOT a copy-back of bundle-local user.sf2.
void verifyP165PrivateExternalSf2V5OriginalPCM(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources,
    int program=7) {
  const auto vintage=resources/"vintage.sf2";
  const auto user77=resources/"77-presets.sf2";
  check(fs::is_regular_file(vintage)&&fs::is_regular_file(user77),
        "P165 actual original owner SF2 positive test assets missing");
  // First obtain an independent source PCM fingerprint from a genuine
  // ORIGINAL Creative chip with bundle-local vintage SF2, as V4 always did.
  ownerBank(resources,"vintage.sf2");
  auto oldProject=Plugin(factory,id,host);
  oldProject.start(program,1,44100.);
  const auto originalPCM=p015Play(oldProject,0,60);
  // Now intentionally alter the default bank on disk to DIFFERENT SF2.
  // Independent negative control: the wrong bank must produce a different
  // real 86Box chip waveform, or our positive V5 test could pass vacuously.
  ownerBank(resources,"77-presets.sf2");
  auto wrongBank=Plugin(factory,id,host);
  wrongBank.start(program,1,44100.);
  check(p015Play(wrongBank,0,60)!=originalPCM,
        "P165 owner A/B fixture has identical original chip PCM");
  std::array<std::uint8_t,32> digest{};
  check(p009::sha256FileNonRealtime(vintage,digest),
        "P165 cannot hash real original owner SoundFont");
  const auto utf8=vintage.u8string();
  const std::string path(utf8.begin(),utf8.end());
  struct Extension {
     std::uint32_t magic=0x35534150u,bytes=0u;
     std::array<std::uint16_t,12> controls{};
     std::array<std::uint8_t,32> sha{};
     std::array<std::uint16_t,2> fx{{4u,2u}};
  };
  static_assert(sizeof(Extension)==68u);
  Saved state{};
  state.version=5u;state.program=static_cast<uint32_t>(program);state.bank=1u;
  state.midiStates.fill(0xFFFFu);
  state.advancedStates.fill(0xFFFFu);
  Extension extension{};
  extension.bytes=static_cast<std::uint32_t>(path.size());
  extension.sha=digest;
  check(!path.empty()&&path.size()<4096u,"P165 owner SF2 path invalid");
  std::vector<std::uint8_t> project(sizeof(state)+sizeof(extension)+path.size());
  std::memcpy(project.data(),&state,sizeof(state));
  std::memcpy(project.data()+sizeof(state),&extension,sizeof(extension));
  std::memcpy(project.data()+sizeof(state)+sizeof(extension),
              path.data(),path.size());
  const auto restore=[&](Plugin& plugin,std::vector<std::uint8_t>& payload){
      MemoryStream stream;int32 bytes=0;
      check(stream.write(payload.data(),static_cast<int32>(payload.size()),
                         &bytes)==kResultOk&&
            bytes==static_cast<int32>(payload.size()),
            "P165 private owner V5 state write");
      check(stream.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,
            "P165 private owner V5 state rewind");
      return plugin.component->setState(&stream);
  };
  auto reopen=Plugin(factory,id,host);
  reopen.start(program,1,44100.);
  check(restore(reopen,project)==kResultOk,
        "P165 positive owner V5 external SF2 SHA restore failed");
  MemoryStream stored;
  check(reopen.component->getState(&stored)==kResultOk &&
        stored.getSize()==project.size()&&
        std::memcmp(stored.getData(),project.data(),project.size())==0,
        "P165 selected real SF2 file identity not preserved bit-exact");
  const auto actualPCM=p015Play(reopen,0,60);
  check(actualPCM==originalPCM,
        "P165 private V5 reopened original chip PCM differs from selected SF2");
  // Independent fresh host instance (project reopens after DAW restart).
  auto fresh=Plugin(factory,id,host);
  fresh.start(program,1,44100.);
  check(restore(fresh,project)==kResultOk,
        "P165 fresh project did not load owner SF2 by full path");
  check(p015Play(fresh,0,60)==originalPCM,
        "P165 fresh project PCM wrong bank despite SHA-positive SF2 path");
  // Negative control: a changed real file identity must FAIL WITHOUT
  // destroying the loaded original instrument or V5 project content.
  auto invalid=project;
  invalid[sizeof(state)+8u+24u]^=0x55u;
  check(restore(reopen,invalid)!=kResultOk,
        "P165 corrupted owner SF2 SHA was incorrectly accepted");
  MemoryStream unchanged;
  check(reopen.component->getState(&unchanged)==kResultOk &&
        unchanged.getSize()==project.size() &&
        std::memcmp(unchanged.getData(),project.data(),project.size())==0,
        "P165 rejected changed SF2 corrupted the current project state");
  std::puts("P165_REAL_OWNER_V5_EXTERNAL_SHA_SF2_POSITIVE_REOPEN_ORIGINAL_PCM_PASS");
  ownerBank(resources,nullptr);
}
void verifyTailAndBlockPartitions(const VST3::Hosting::PluginFactory& factory,
                                  const VST3::UID& id,FUnknown* host,
                                  const fs::path& resources){
  ownerBank(resources,"expressive.sf2");
  for(const int rate:{44100,48000,96000}){
    const int onset=23,release=rate*8/5+127,end=release+rate/4;
    const auto renderTimeline=[&](Plugin& plugin,bool irregular,bool playNote,
                                  Plugin* peer=nullptr){
      std::vector<float> pcm;
      pcm.reserve(2*end);
      std::array<float,frames> left{},right{},peerLeft{},peerRight{};
      float* channels[2]={left.data(),right.data()};
      float* peerChannels[2]={peerLeft.data(),peerRight.data()};
      AudioBusBuffers output{},peerOutput{};
      output.numChannels=peerOutput.numChannels=2;
      output.channelBuffers32=channels;
      peerOutput.channelBuffers32=peerChannels;
      EventList events,peerEvents;
      ProcessData data{},peerData{};
      data.numOutputs=peerData.numOutputs=1;
      data.outputs=&output;
      data.inputEvents=&events;
      peerData.outputs=&peerOutput;
      peerData.inputEvents=&peerEvents;
      double peerEnergy=0.;
      constexpr std::array<int,8> sizes{{1,17,63,64,65,127,257,511}};
      for(int position=0,block=0;position<end;++block){
        const int count=std::min(end-position,
                                 irregular?sizes[block%sizes.size()]:frames);
        events.clear();
        for(const bool on:{true,false}){
          const int time=on?onset:release;
          if(!playNote || time<position || time>=position+count)continue;
          Event event{};
          event.sampleOffset=time-position;
          if(on){
            event.type=Event::kNoteOnEvent;
            event.noteOn.pitch=60;event.noteOn.velocity=100.f/127.f;
          }else{
            event.type=Event::kNoteOffEvent;event.noteOff.pitch=60;
          }
          check(events.addEvent(event)==kResultOk,"tail: could not enqueue note");
        }
        if(peer){
          peerEvents.clear();
          if(block==0){
            Event event{};
            event.type=Event::kNoteOnEvent;
            event.noteOn.pitch=72;event.noteOn.velocity=1.f;
            check(peerEvents.addEvent(event)==kResultOk,"tail: peer note missing");
          }
          peerData.numSamples=count;
          check(peer->processor->process(peerData)==kResultOk,"tail: peer callback failed");
          for(int i=0;i<count;++i)
            peerEnergy+=double(peerLeft[i])*peerLeft[i]+double(peerRight[i])*peerRight[i];
        }
        data.numSamples=count;
        check(plugin.processor->process(data)==kResultOk,"tail: callback failed");
        for(int i=0;i<count;++i){
          check(std::isfinite(left[i]) && std::isfinite(right[i]),"tail: nonfinite PCM");
          pcm.push_back(left[i]);pcm.push_back(right[i]);
        }
        position+=count;
      }
      if(peer)check(peerEnergy>0.,"tail: isolation control never played");
      return pcm;
    };
    Plugin fixed(factory,id,host),variable(factory,id,host),peer(factory,id,host);
    fixed.start(0,1,rate);
    variable.start(0,1,rate);
    peer.start(0,1,rate==48000?44100:48000);
    const auto expected=renderTimeline(fixed,false,true);
    const auto actual=renderTimeline(variable,true,true,&peer);
    const auto tailPeak=[&](const std::vector<float>& pcm){
      float peak=0.f;
      for(int i=2*(release+rate/10);i<2*end;++i)
        peak=std::max(peak,std::abs(pcm[i]));
      return peak;
    };
    Plugin silent(factory,id,host);
    silent.start(0,1,rate);
    const auto tail=tailPeak(expected);
    const auto idle=tailPeak(renderTimeline(silent,false,false));
    const auto reported=fixed.processor->getTailSamples();
    std::printf("AWE64_TAIL rate=%d postReleasePeak=%.9f noNotePeak=%.9f reported=%u\n",
                rate,tail,idle,reported);
    check(tail>1.f/32768.f && idle==0.f,"tail: post-release audio oracle is invalid");
    check(reported==kInfiniteTail,"tail: host limit may cut off original release/FX");
    const auto mismatch=std::mismatch(expected.begin(),expected.end(),actual.begin());
    if(mismatch.first!=expected.end())
      std::fprintf(stderr,"AWE64_BLOCK_MISMATCH rate=%d frame=%td channel=%td expected=%.9f actual=%.9f\n",
                   rate,(mismatch.first-expected.begin())/2,(mismatch.first-expected.begin())%2,
                   *mismatch.first,*mismatch.second);
    check(mismatch.first==expected.end(),"tail: partition or another instance changed stereo PCM");
  }
  ownerBank(resources,nullptr);
  std::puts("AWE64_TAIL_VARIABLE_BLOCKS_MULTI_INSTANCE_PCM_PASS");
}

void verifyDrumReleaseAfterBankChange(const VST3::Hosting::PluginFactory& factory,
                                      const VST3::UID& id,FUnknown* host,
                                      const fs::path& resources){
  ownerBank(resources,"drums.sf2");
  enum class Release { explicitOff, zeroVelocity, none };
  const auto render=[&](Release release){
    Plugin plugin(factory,id,host);
    plugin.start(0,2,44100.); // browser drum lane redirects CH1 to CH10
    std::array<float,frames> left{},right{};
    float* channels[2]={left.data(),right.data()};
    AudioBusBuffers output{};
    output.numChannels=2;
    output.channelBuffers32=channels;
    EventList events;
    ParameterChanges changes(1);
    ProcessData data{};
    data.numSamples=frames;
    data.numOutputs=1;
    data.outputs=&output;
    data.inputEvents=&events;
    data.inputParameterChanges=&changes;
    std::vector<float> pcm;
    for(int block=0;block<100;++block){
      events.clear();
      changes.clearQueue();
      if(block==0){
        Event event{};
        event.type=Event::kNoteOnEvent;
        event.noteOn.pitch=60;event.noteOn.velocity=1.f;
        check(events.addEvent(event)==kResultOk,"drum release: missing note");
      }
      if(block==64){
        int32 queue=0,point=0;
        auto* bank=changes.addParameterData(1002u,queue);
        check(bank && bank->addPoint(1,0.,point)==kResultOk,
              "drum release: missing change to melodic bank");
      }
      if(block==70 && release!=Release::none){
        Event event{};
        event.sampleOffset=15;
        if(release==Release::explicitOff){
          event.type=Event::kNoteOffEvent;
          event.noteOff.pitch=60;
        }else{
          event.type=Event::kNoteOnEvent;
          event.noteOn.pitch=60;event.noteOn.velocity=0.f;
        }
        check(events.addEvent(event)==kResultOk,"drum release: missing release");
      }
      check(plugin.processor->process(data)==kResultOk,"drum release: callback failed");
      for(int i=0;i<frames;++i){
        check(std::isfinite(left[i]) && std::isfinite(right[i]),
              "drum release: nonfinite PCM");
        pcm.push_back(left[i]);pcm.push_back(right[i]);
      }
    }
    return pcm;
  };
  const auto explicitOff=render(Release::explicitOff);
  const auto zeroVelocity=render(Release::zeroVelocity);
  const auto held=render(Release::none);
  check(explicitOff!=held,"drum release: explicit NoteOff did not change held PCM");
  check(zeroVelocity==explicitOff,
        "drum release: zero-velocity NoteOn lost the original drum route");
  ownerBank(resources,nullptr);
  std::puts("AWE64_ZERO_VELOCITY_RELEASE_ORIGINAL_DRUM_ROUTE_PCM_PASS");
}

// P168 adversarial original-chip per-knob audio contract: every one of
// the 10 non-FX faceplate controls must change REAL rendered EMU8000 PCM on
// a deliberately expressive SoundFont. A stored V5 project must produce
// exactly the same audio in a completely fresh actual Windows VST3 instance.
// Using a default fast-envelope one-shot for this test would falsely treat
// an inaudible-but-correct envelope knob as broken.
void verifyP168AllTenHardwareKnobsOriginalPCMAndReopen(
     const VST3::Hosting::PluginFactory& factory,
     const VST3::UID& id,FUnknown* host,const fs::path& resources) {
    ownerBank(resources,"expressive.sf2");
    struct Extension {
        std::uint32_t magic=0x35534150u,bytes=0u;
        std::array<std::uint16_t,12> controls{};
        std::array<std::uint8_t,32> sha{};
        std::array<std::uint16_t,2> fx{{4u,2u}};
    };
    static_assert(sizeof(Extension)==68u);
    struct AudioResult {
        std::uint64_t hash=14695981039346656037ull;
        double energy=0.;
        std::vector<std::uint8_t> state;
    };
    const auto play=[&](int knob,const std::vector<std::uint8_t>* project){
        Plugin p(factory,id,host);
        p.start(0,1,44100.);
        if(project){
            MemoryStream read;int32 written=0;
            check(read.write(const_cast<std::uint8_t*>(project->data()),
                         static_cast<int32>(project->size()),&written)==kResultOk &&
                  written==static_cast<int32>(project->size()) &&
                  read.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
                  p.component->setState(&read)==kResultOk,
                  "P168 V5 faceplate project reload rejected in real VST3");
        }
        std::array<float,frames> left{},right{};
        float* channels[2]={left.data(),right.data()};
        AudioBusBuffers output{};
        output.numChannels=2;output.channelBuffers32=channels;
        EventList events;
        ParameterChanges params(1);
        ProcessData pd{};pd.processMode=kRealtime;
        pd.symbolicSampleSize=kSample32;pd.numSamples=frames;
        pd.numOutputs=1;pd.outputs=&output;
        pd.inputEvents=&events;pd.inputParameterChanges=&params;
        AudioResult result{};
        // Hold for >1s to reach the deliberately long envelopes; then
        // release and measure the real original-chip tail for both AMP and
        // FILTER release controls. No fake DSP or synthetic PCM substitution.
        for(int block=0;block<112;++block){
            events.clear();params.clearQueue();
            if(block==0){
                if(knob>=0){
                    int32 qi=0,pi=0;
                    auto* q=params.addParameterData(1100u+knob,qi);
                    check(q&&q->addPoint(0,1.,pi)==kResultOk,
                          "P168 actual VST3 host faceplate automation absent");
                }
                Event on{};on.type=Event::kNoteOnEvent;
                on.sampleOffset=12;on.noteOn.channel=0;
                on.noteOn.pitch=60;on.noteOn.velocity=100.f/127.f;
                check(events.addEvent(on)==kResultOk,"P168 genuine NoteOn missing");
            }
            if(block==68){
                Event off{};off.type=Event::kNoteOffEvent;
                off.sampleOffset=511;off.noteOff.channel=0;off.noteOff.pitch=60;
                check(events.addEvent(off)==kResultOk,"P168 genuine NoteOff missing");
            }
            check(p.processor->process(pd)==kResultOk,
                  "P168 actual original-chip MIDI+hardware GUI process failed");
            for(int i=0;i<frames;++i){
                for(float sample:{left[i],right[i]}){
                    check(std::isfinite(sample),"P168 original chip produced nonfinite PCM");
                    result.energy+=double(sample)*double(sample);
                    std::uint32_t bits=0;std::memcpy(&bits,&sample,sizeof(bits));
                    result.hash=(result.hash^bits)*1099511628211ull;
                }
            }
        }
        MemoryStream state;
        check(p.component->getState(&state)==kResultOk,
              "P168 actual VST3 project state unavailable after audio");
        const auto* bytes=reinterpret_cast<const std::uint8_t*>(state.getData());
        check(bytes!=nullptr && state.getSize()>0,
              "P168 host returned empty project state after real PCM");
        result.state.assign(bytes,bytes+state.getSize());
        return result;
    };
    const auto baseline=play(-1,nullptr);
    check(baseline.energy>0.01,
          "P168 invalid adversarial control oracle: no expressive original PCM");
    for(int knob=0;knob<10;++knob){
        const auto edited=play(knob,nullptr);
        check(edited.energy>0.001,
              "P168 edited native SoundFont was accidentally muted");
        check(edited.hash!=baseline.hash,
              "P168 hardware faceplate knob changed state but not original-chip audio");
        check(edited.state.size()==648u,
              "P168 genuine faceplate control not serialized as V5 state");
        // std::vector<uint8_t> has no contractual alignment for Extension.
        // Decode the wire format via memcpy, never an unaligned cast.
        Extension ext{};
        std::memcpy(&ext,edited.state.data()+sizeof(Saved),sizeof(ext));
        check(ext.magic==0x35534150u && ext.controls[knob]==10000u,
              "P168 sound-producing control missing from saved project");
        const auto recalled=play(-1,&edited.state);
        check(recalled.hash==edited.hash &&
              std::abs(recalled.energy-edited.energy)<1e-7,
              "P168 reloaded V5 hardware control produces DIFFERENT genuine PCM");
        check(recalled.hash!=baseline.hash,
              "P168 negative control: audio reverted to unedited SoundFont after recall");
        std::printf("P168_REAL_86BOX_GUI_KNOB_PCM_ORIGINAL_REOPEN_PASS control=%d base=%016llX changed=%016llX rms2=%.7f\n",
              knob,static_cast<unsigned long long>(baseline.hash),
              static_cast<unsigned long long>(edited.hash),edited.energy);
    }
    std::puts("P168_ACTUAL_WINDOWS_VST3_TEN_HARDWARE_KNOBS_AUDIBLE_V5_SAVE_REOPEN_PCM_PASS");
    ownerBank(resources,nullptr);
}

// P167: two genuinely independent original 86Box chip instances start
// with the same sustained sample and note. Master is *post-chip* linear
// output scaling, so an independent reference waveform predicts the exact
// piecewise PCM of two host parameter points inside ONE audio callback.
// A broken block-global "last master point" implementation fails the
// early, middle and final PCM windows even though final state would pass.
void verifyP167TimedMasterOriginalPCM(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"sustain.sf2");
  Plugin ref(factory,id,host);ref.start(0,1,44100.);
  Plugin stepped(factory,id,host);stepped.start(0,1,44100.);
  EventList notes1,notes2;
  Event on{};on.type=Event::kNoteOnEvent;on.sampleOffset=0;
  on.noteOn.channel=0;on.noteOn.pitch=60;on.noteOn.velocity=1.f;
  check(notes1.addEvent(on)==kResultOk && notes2.addEvent(on)==kResultOk,
        "P167 original-chip master independent NoteOn fixture");
  ParameterChanges master(1);int32 queue=0,index=0;
  auto* points=master.addParameterData(1001u,queue);
  check(points&&points->addPoint(192,.25,index)==kResultOk&&
        points->addPoint(700,.75,index)==kResultOk,
        "P167 actual Steinberg host cannot enqueue two timed master points");
  std::array<float,frames> baseL{},baseR{},stepL{},stepR{};
  float* b[2]={baseL.data(),baseR.data()};
  float* s[2]={stepL.data(),stepR.data()};
  AudioBusBuffers out1{},out2{};
  out1.numChannels=out2.numChannels=2;
  out1.channelBuffers32=b;out2.channelBuffers32=s;
  ProcessData d1{},d2{};
  d1.processMode=d2.processMode=kRealtime;
  d1.symbolicSampleSize=d2.symbolicSampleSize=kSample32;
  d1.numSamples=d2.numSamples=frames;
  d1.numOutputs=d2.numOutputs=1;d1.outputs=&out1;d2.outputs=&out2;
  d1.inputEvents=&notes1;
  d2.inputEvents=&notes2;d2.inputParameterChanges=&master;
  check(ref.processor->process(d1)==kResultOk &&
        stepped.processor->process(d2)==kResultOk,
        "P167 real Windows 86Box VST3 dual chip master automation callback");
  std::array<double,3> referenceEnergy{},sampleDifference{};
  for(int i=0;i<frames;++i){
     const int window=i<192?0:i<700?1:2;
     const double gain=window==0?1.:window==1?.25:.75;
     const double base=double(baseL[i])*baseL[i]+double(baseR[i])*baseR[i];
     referenceEnergy[window]+=base;
     const double dL=double(stepL[i])-double(baseL[i])*gain;
     const double dR=double(stepR[i])-double(baseR[i])*gain;
     sampleDifference[window]+=dL*dL+dR*dR;
     check(std::abs(dL)<1e-6&&std::abs(dR)<1e-6,
           "P167 sample-offset master output not piecewise original-chip PCM");
  }
  for(int i=0;i<3;++i)
     check(referenceEnergy[i]>.001,
           "P167 invalid timing oracle: no original chip audio in a PCM window");
  // Negative control: a last-point (.75) applied to the ENTIRE block
  // would produce a different early/middle PCM. A fake silence PASS fails.
  double wrongLastPoint=0.;
  for(int i=0;i<700;++i){
    const double errL=double(stepL[i])-double(baseL[i])*.75;
    const double errR=double(stepR[i])-double(baseR[i])*.75;
    wrongLastPoint+=errL*errL+errR*errR;
  }
  check(wrongLastPoint>.001,
        "P167 negative control did not catch last-point block-global master");
  MemoryStream saved;
  check(stepped.component->getState(&saved)==kResultOk &&
        saved.getSize()==sizeof(Saved),
        "P167 master timing unexpectedly changed V4 saved-state layout");
  Saved finalState{};
  std::memcpy(&finalState,saved.getData(),sizeof(finalState));
  check(finalState.master==.75f &&
        std::abs(stepped.controller->getParamNormalized(1001u)-.75)<.0001,
        "P167 final master state/controller did not agree with last sample point");
  std::printf("P167_ORIGINAL_86BOX_MASTER_PCM_WINDOWS energy=%.6f/%.6f/%.6f error=%.10f/%.10f/%.10f wrongLastPoint=%.6f\n",
      referenceEnergy[0],referenceEnergy[1],referenceEnergy[2],
      sampleDifference[0],sampleDifference[1],sampleDifference[2],wrongLastPoint);
  std::puts("P167_ACTUAL_WINDOWS_VST3_SAMPLE_ACCURATE_MASTER_AUTOMATION_ORIGINAL_PCM_PASS");
  ownerBank(resources,nullptr);
}

// F-09: Run the genuinely loaded Win64 original-chip VST3 with two
// deliberately saturated host inputs. The public SoundFont used here is
// *sustained*, not a self-decaying one-shot (which could falsely pass a
// stuck-note test with no CC120 voice cleanup).
// F-05 negative control: MIDI CC0 selects bank1 in the actual chip, then
// a later HOST bank0 edit (including a same-value GUI bank reselection)
// must defeat old CC0 in both the actual PCM and V4/V5 project state.
// A false PASS is prevented by a separately audible bank0/bank1 oracle.
void verifyP165HostBankSupersedesOldCC0(
     const VST3::Hosting::PluginFactory& factory,
     const VST3::UID& id,FUnknown* host,const fs::path& resources){
   ownerBank(resources,"77-presets.sf2"); // unlike base SBK's waveform
   auto replay=Plugin(factory,id,host);replay.start(0,0,44100.);
   IMidiMapping* raw=nullptr;
   check(replay.controller->queryInterface(IMidiMapping::iid,
         reinterpret_cast<void**>(&raw))==kResultOk&&raw,
         "F05 no actual VST3 MIDI mapping interface");
   const IPtr<IMidiMapping> mapping=owned(raw);
   ParamID cc0=0;
   check(mapping->getMidiControllerAssignment(0,0,kCtrlBankSelectMSB,cc0)
         ==kResultTrue,"F05 mapped CC0 unavailable in actual VST3");
   ParameterChanges params(2);
   std::array<float,frames> l{},r{};
   float* channels[2]={l.data(),r.data()};
   AudioBusBuffers output{};
   output.numChannels=2;output.channelBuffers32=channels;
   ProcessData pd{};pd.processMode=kRealtime;pd.symbolicSampleSize=kSample32;
   pd.numSamples=frames;pd.numOutputs=1;pd.outputs=&output;
   pd.inputParameterChanges=&params;
   int32 qIndex=0,point=0;
   auto* q=params.addParameterData(cc0,qIndex);
   check(q && q->addPoint(10,1./127.,point)==kResultOk,
         "F05 original MIDI CC0 bank1 event missing");
   check(replay.processor->process(pd)==kResultOk,
         "F05 CC0 original-chip callback rejected");
   MemoryStream first;
   check(replay.component->getState(&first)==kResultOk&&
         first.getSize()==sizeof(Saved),"F05 CC0 state lost");
   Saved initially{};
   std::memcpy(&initially,first.getData(),sizeof(initially));
   check(initially.advancedStates[0]==1u&&initially.bank==1u&&
         replay.controller->getParamNormalized(1002u)==.5,
         "F05 MIDI CC0 bank1 not reflected in controller and engine state");
   // An explicit same-bank host edit must win over this prior CC0.
   params.clearQueue();qIndex=0;point=0;
   q=params.addParameterData(1002u,qIndex); // host Bank CH1
   check(q && q->addPoint(0,0.,point)==kResultOk,
         "F05 actual host same-bank reassert automation missing");
   check(replay.processor->process(pd)==kResultOk,
         "F05 actual host bank0 did not execute after CC0");
   MemoryStream after;
   check(replay.component->getState(&after)==kResultOk&&
         after.getSize()==sizeof(Saved),"F05 state after reassert unavailable");
   Saved selected{};
   std::memcpy(&selected,after.getData(),sizeof(selected));
   check(selected.bank==0u&&selected.advancedStates[0]==0xFFFFu&&
         selected.advancedStates[1]==0xFFFFu,
         "F05 previous CC0/CC32 would override the ROM bank after reopening");
   auto reopened=Plugin(factory,id,host);
   reopened.start(0,0,44100.);
   p015Restore(reopened,selected);
   auto reference0=Plugin(factory,id,host);
   reference0.start(0,0,44100.);
   auto reference1=Plugin(factory,id,host);
   reference1.start(0,1,44100.);
   const auto romPCM=p015Play(reference0,0,60);
   const auto sf2PCM=p015Play(reference1,0,60);
   check(romPCM!=sf2PCM,
         "F05 invalid PCM oracle: bank0 and bank1 fixtures identical");
   check(p015Play(reopened,0,60)==romPCM,
         "F05 VST3 project recall replayed stale MIDI CC0 instead of GUI ROM");
   std::puts("P165_F05_ACTUAL_WINDOWS_VST3_CC0_HOST_BANK_RESELECT_V4_RECALL_PCM_PASS");
   ownerBank(resources,nullptr);
}
void verifyP165HeldNoteOverflowRecovery(
     const VST3::Hosting::PluginFactory& factory,
     const VST3::UID& id,FUnknown* host,const fs::path& resources){
   ownerBank(resources,"sustain.sf2");
   for(const bool eventOverflow:{true,false}){
     Plugin p(factory,id,host);
     p.start(0,1,44100.);
     // Steinberg's *example host* EventList defaults to maxSize=50.
     // We deliberately need 513 genuine ABI events to challenge the
     // actual plugin limit of 512. Preallocate on the HOST/test thread.
     EventList notes(514);
     ParameterChanges params(4);
     std::array<float,frames> left{},right{};
     float* channels[2]={left.data(),right.data()};
     AudioBusBuffers output{};
     output.numChannels=2;output.channelBuffers32=channels;
     ProcessData data{};
     data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
     data.numSamples=frames;data.numOutputs=1;data.outputs=&output;
     data.inputEvents=&notes;data.inputParameterChanges=&params;
     const auto energy=[&](){
       double sum=0.;
       for(int i=0;i<frames;++i)
         for(float sample:{left[i],right[i]}){
           check(std::isfinite(sample),"F09 original chip nonfinite PCM");
           sum+=double(sample)*double(sample);
         }
       return sum;
     };
     Event on{};on.type=Event::kNoteOnEvent;on.sampleOffset=12;
     on.noteOn.channel=0;on.noteOn.pitch=60;on.noteOn.velocity=1.f;
     check(notes.addEvent(on)==kResultOk,"F09 sustained NoteOn fixture");
     // The original Creative/SF2 instrument sends some signal to hardware
     // FX by default. CC120 must stop the voice, but must NOT artificially
     // clear a legitimately ringing reverb/chorus tail. Disable both
     // hardware sends *before* this note to make post-CC120 PCM a valid
     // sustained-VOICE (not FX-tail) oracle. This is genuine 86Box chip DSP.
     for(const auto [id,value]:std::array<std::pair<ParamID,ParamValue>,4>{{
          {1420u,1.},{1421u,1.},{1110u,0.},{1111u,0.}}}){
       int32 idx=0,point=0;
       auto* queue=params.addParameterData(id,idx);
       check(queue && queue->addPoint(0,value,point)==kResultOk,
             "F09 original chip hardware sends could not be set DRY");
     }
     double heldEnergy=0.,earlyEnergy=0.;
     for(int block=0;block<6;++block){
       if(block){notes.clear();params.clearQueue();}
       check(p.processor->process(data)==kResultOk,
             "F09 held synthetic SF2 normal audio");
       if(block==1)earlyEnergy=energy();
       if(block==5)heldEnergy=energy();
     }
     std::printf("P165_F09_SUSTAIN_ORACLE first=%.9f held=%.9f\n",
                 earlyEnergy,heldEnergy);
     check(earlyEnergy>0.001,
           "F09 test not valid: actual synthetic VST3 produced no initial note");
     check(heldEnergy>0.001,
           "F09 test not valid: synthetic SoundFont did not sustain a note");
     if(eventOverflow){
       // No matching CH1 NoteOff is present. This must not leave the
       // original EMU8000 voice held once the VST3 input is rejected.
       for(int i=0;i<513;++i){
         Event off{};off.type=Event::kNoteOffEvent;
         off.sampleOffset=0;off.noteOff.channel=15;
         off.noteOff.pitch=i%128;
         check(notes.addEvent(off)==kResultOk,
               "F09 actual host cannot enqueue >512 MIDI events");
       }
     }else{
       int32 qIndex=0,point=0;
       auto* q=params.addParameterData(2000u,qIndex); // CH1 CC1
       check(q!=nullptr,"F09 actual host CC queue missing");
       for(int i=0;i<513;++i)
         check(q->addPoint(i%frames,double(i%127)/127.,point)==kResultOk,
               "F09 actual host cannot enqueue >512 CC points");
     }
     left.fill(-1.f);right.fill(1.f);
     check(p.processor->process(data)==kResultFalse,
           "F09 input saturation must be explicit fail-closed");
     for(int i=0;i<frames;++i)
       check(left[i]==0.f&&right[i]==0.f,
             "F09 rejected block leaked original chip audio");
     check(output.silenceFlags==3u,
           "F09 rejected block did not advertise actual zero output");
     notes.clear();params.clearQueue();
     double after=0.;
     for(int block=0;block<8;++block){
       output.silenceFlags=0;
       check(p.processor->process(data)==kResultOk,
             "F09 original synth stuck in overload failure state");
       if(block==7)after=energy();
     }
     std::printf("P165_F09_VOICE_EXTINCTION path=%s held=%.9f after=%.9f ratio=%.9f\n",
                 eventOverflow?"events":"CC",heldEnergy,after,
                 heldEnergy>0.?after/heldEnergy:0.);
     check(after<heldEnergy*0.001,
           "F09 dry original chip voice survived rejected NoteOff block");
     // The safety event must kill the old note, NOT permanently mute the
     // device. A fresh legitimate NoteOn must still play.
     check(notes.addEvent(on)==kResultOk,
           "F09 post-recovery legitimate NoteOn enqueue");
     double retrigger=0.;
     for(int block=0;block<4;++block){
       if(block)notes.clear();
       check(p.processor->process(data)==kResultOk,
             "F09 post-recovery original chip callback failed");
       if(block==3)retrigger=energy();
     }
     check(retrigger>heldEnergy*0.01,
           "F09 overflow recovery permanently muted new original-chip notes");
   }
   std::puts("P165_F09_ACTUAL_WINDOWS_VST3_SUSTAINED_VOICE_EVENT_AND_CC_OVERFLOW_RECOVERY_PCM_PASS");
   ownerBank(resources,nullptr);
}

// P173 source-level defect counterfactual: the pre-fix implementation
// immediately stopped rendering the old sustained 86Box EMU8000 chip at
// setState + first process block. This actual VST3 test makes an audible
// sustained original-chip PCM fixture, swaps the user SF2 while it rings,
// and asserts the exact 15-ms old->new sample ramp from an *independent*
// still-playing original chip. An instant replacement MUST fail this gate.
// The new engine has no NoteOn, so only the old fading note may be audible.
void verifyP173LiveSf2SwitchOriginalChipFade(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"sustain.sf2");
  struct Extension {
    std::uint32_t magic=0x35534150u,bytes=0u;
    std::array<std::uint16_t,12> controls{};
    std::array<std::uint8_t,32> sha{};
    std::array<std::uint16_t,2> fx{{4u,2u}};
  };
  static_assert(sizeof(Extension)==68u,"P173 exact V5 format");
  const fs::path replacement=resources/"vintage.sf2";
  std::array<std::uint8_t,32> digest{};
  check(p009::sha256FileNonRealtime(replacement,digest),
        "P173 cannot hash alternate SF2");
  const auto utf8=replacement.u8string();
  const std::string selected(utf8.begin(),utf8.end());
  Saved saved{};
  saved.version=5u;saved.bank=1u;saved.program=0u;saved.master=1.f;
  saved.midiStates.fill(0xFFFFu);saved.advancedStates.fill(0xFFFFu);
  Extension ext{};ext.sha=digest;
  ext.bytes=static_cast<std::uint32_t>(selected.size());
  std::vector<std::uint8_t> v5(sizeof(saved)+sizeof(ext)+selected.size());
  std::memcpy(v5.data(),&saved,sizeof(saved));
  std::memcpy(v5.data()+sizeof(saved),&ext,sizeof(ext));
  std::memcpy(v5.data()+sizeof(saved)+sizeof(ext),
              selected.data(),selected.size());

  for(const int rate:{44100,48000,96000}){
    for(const int blockSize:{64,128,512}){
      Plugin reference(factory,id,host),switching(factory,id,host);
      reference.start(0,1,rate);switching.start(0,1,rate);
      std::array<float,frames> oldL{},oldR{},newL{},newR{};
      float* channelsOld[2]={oldL.data(),oldR.data()};
      float* channelsNew[2]={newL.data(),newR.data()};
      AudioBusBuffers outOld{},outNew{};
      outOld.numChannels=outNew.numChannels=2;
      outOld.channelBuffers32=channelsOld;outNew.channelBuffers32=channelsNew;
      EventList oldNotes,newNotes;
      Event on{};on.type=Event::kNoteOnEvent;
      on.noteOn.channel=0;on.noteOn.pitch=60;on.noteOn.velocity=1.f;
      check(oldNotes.addEvent(on)==kResultOk &&
            newNotes.addEvent(on)==kResultOk,
            "P173 sustained original chip NoteOn not available");
      ProcessData od{},nd{};
      od.processMode=nd.processMode=kRealtime;
      od.symbolicSampleSize=nd.symbolicSampleSize=kSample32;
      od.numSamples=nd.numSamples=frames;
      od.numOutputs=nd.numOutputs=1;
      od.outputs=&outOld;nd.outputs=&outNew;
      od.inputEvents=&oldNotes;nd.inputEvents=&newNotes;
      double ringEnergy=0.;
      for(int j=0;j<8;++j){
        if(j){oldNotes.clear();newNotes.clear();}
        check(reference.processor->process(od)==kResultOk &&
              switching.processor->process(nd)==kResultOk,
              "P173 sustained original chip initial playback failed");
        if(j==7){
          for(int i=0;i<frames;++i)
            ringEnergy+=double(oldL[i])*oldL[i]+double(oldR[i])*oldR[i];
        }
      }
      check(ringEnergy>.001,
            "P173 negative-control invalid: sustained old SF2 was silent");
      MemoryStream state;int32 wrote=0;
      check(state.write(v5.data(),static_cast<int32>(v5.size()),&wrote)==kResultOk &&
            wrote==static_cast<int32>(v5.size()) &&
            state.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
            switching.component->setState(&state)==kResultOk,
            "P173 actual VST3 live SF2 replacement refused valid state");
      od.numSamples=nd.numSamples=blockSize;
      check(reference.processor->process(od)==kResultOk &&
            switching.processor->process(nd)==kResultOk,
            "P173 original-chip crossfade first audio callback failed");
      const auto fadeFrames=static_cast<int>(std::ceil(rate*.015));
      double oldEnergy=0.,actualEnergy=0.,error=0.;
      for(int i=0;i<blockSize;++i){
        const double oldWeight=double(fadeFrames-i)/fadeFrames;
        for(const auto pair:std::array<std::pair<float,float>,2>{{
              {oldL[i],newL[i]},{oldR[i],newR[i]}}}){
          check(std::isfinite(pair.first)&&std::isfinite(pair.second),
                "P173 original EMU8000 transition emitted nonfinite PCM");
          const double wanted=double(pair.first)*oldWeight;
          const double delta=double(pair.second)-wanted;
          oldEnergy+=double(pair.first)*pair.first;
          actualEnergy+=double(pair.second)*pair.second;
          error+=delta*delta;
          check(std::abs(delta)<1e-4,
                "P173 SF2 swap cut off old voice/FX or applied wrong fade");
        }
      }
      check(oldEnergy>1e-6&&actualEnergy>oldEnergy*.1,
            "P173 fade negative control: abrupt old-chip removal went undetected");
      // Do not let old bank tails or a stale sustain note live forever.
      // The finite 15-ms policy ends even when the old SF2 loops indefinitely.
      const int tailBlocks=fadeFrames/blockSize+4;
      for(int j=0;j<tailBlocks;++j){
        check(switching.processor->process(nd)==kResultOk,
              "P173 original chip crossfade did not finish");
        for(int i=0;i<blockSize;++i)
          check(std::isfinite(newL[i])&&std::isfinite(newR[i]),
                "P173 bank fade produced nonfinite PCM during tail");
      }
      double afterPeak=0.;
      for(int i=0;i<blockSize;++i)
        afterPeak=std::max({afterPeak,std::abs(double(newL[i])),
                            std::abs(double(newR[i]))});
      check(afterPeak<1e-6,
            "P173 old sustained voice did not retire after bounded fade");
      newNotes.clear();
      check(newNotes.addEvent(on)==kResultOk,
            "P173 new SF2 NoteOn unavailable");
      nd.numSamples=frames;
      check(switching.processor->process(nd)==kResultOk,
            "P173 new bank became unplayable after old voice retirement");
      double freshEnergy=0.;
      for(int i=0;i<frames;++i){
        check(std::isfinite(newL[i])&&std::isfinite(newR[i]),
              "P173 new SoundFont PCM nonfinite");
        freshEnergy+=double(newL[i])*newL[i]+double(newR[i])*newR[i];
      }
      check(freshEnergy>.0001,
            "P173 valid new SF2 did not produce audible original-chip audio");
      std::printf("P173_LIVE_SF2_FADE rate=%d block=%d old=%.7f swapped=%.7f error=%.10f new=%.7f\n",
                  rate,blockSize,oldEnergy,actualEnergy,error,freshEnergy);
    }
  }
  ownerBank(resources,nullptr);
  std::puts("P173_ACTUAL_WINDOWS_VST3_ORIGINAL_CHIP_LIVE_SF2_FADE_9X_PCM_PASS");
}

// F-07: Interleave multiple full original-chip SoundFont swaps with
// stopped audio and normal audio, and close the *actual VST3 instance*
// while a 28 MiB replacement has not yet reached an audio callback.
// Each restored V5 file identity and SHA must remain exact. This is a
// lifecycle stress contract, NOT a sanitizer or proof of every host race.
void verifyP165InterruptedAssetLifecycle(
     const VST3::Hosting::PluginFactory& factory,
     const VST3::UID& id,FUnknown* host,const fs::path& resources){
   ownerBank(resources,"vintage.sf2");
   struct Extension {
     std::uint32_t magic=0x35534150u,bytes=0u;
     std::array<std::uint16_t,12> controls{};
     std::array<std::uint8_t,32> sha{};
     std::array<std::uint16_t,2> fx{{4u,2u}};
   };
   static_assert(sizeof(Extension)==68u);
   const auto projectFor=[&](const fs::path& sf2){
     std::array<std::uint8_t,32> digest{};
     check(p009::sha256FileNonRealtime(sf2,digest),
           "F07 interrupted asset has no verifiable SF2 SHA");
     const auto utf8=sf2.u8string();
     const std::string path(utf8.begin(),utf8.end());
     Saved state{};state.version=5u;state.bank=1u;
     state.midiStates.fill(0xFFFFu);
     state.advancedStates.fill(0xFFFFu);
     Extension ext{};ext.sha=digest;
     ext.bytes=static_cast<std::uint32_t>(path.size());
     std::vector<std::uint8_t> payload(sizeof(state)+sizeof(ext)+path.size());
     std::memcpy(payload.data(),&state,sizeof(state));
     std::memcpy(payload.data()+sizeof(state),&ext,sizeof(ext));
     std::memcpy(payload.data()+sizeof(state)+sizeof(ext),path.data(),path.size());
     return payload;
   };
   const auto a=projectFor(resources/"vintage.sf2");
   const auto b=projectFor(resources/"77-presets.sf2");
   for(int cycle=0;cycle<6;++cycle){
     Plugin p(factory,id,host);p.start(0,1,44100.);
     for(int swap=0;swap<3;++swap){
       const auto& target=(swap&1)?b:a;
       MemoryStream stream;int32 written=0;
       check(stream.write(const_cast<std::uint8_t*>(target.data()),
                 static_cast<int32>(target.size()),&written)==kResultOk &&
             written==static_cast<int32>(target.size()) &&
             stream.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
             p.component->setState(&stream)==kResultOk,
             "F07 actual VST3 interrupted 28 MiB asset swap rejected");
       MemoryStream saved;
       check(p.component->getState(&saved)==kResultOk &&
             saved.getSize()==target.size() &&
             std::memcmp(saved.getData(),target.data(),target.size())==0,
             "F07 queued asset swap corrupted serialized SHA/path");
       if(cycle%2==0 && swap==1){
         std::array<float,frames> l{},r{};
         float* lr[2]={l.data(),r.data()};
         AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=lr;
         ProcessData data{};data.processMode=kRealtime;
         data.symbolicSampleSize=kSample32;data.numSamples=frames;
         data.numOutputs=1;data.outputs=&out;
         check(p.processor->process(data)==kResultOk,
               "F07 staged synth could not transition at audio boundary");
       }
     }
     // Deliberately no final process(): odd cycles die with the newest
     // replacement staged; even cycles have an older chip retired.
     // Plugin destructor performs normal processing-stop/deactivation/
     // termination; each must release unused synths outside audio.
   }
   std::puts("P165_F07_ACTUAL_WINDOWS_VST3_INTERRUPTED_SF2_SWAP_CLOSE_REOPEN_STATE_PASS");
   ownerBank(resources,nullptr);
}

// In the exact native EMU8000 instrument: 0% SEND means zero ORIGINAL
// hardware reverb/chorus register after an explicit human/host edit.
// Untouched "DEFAULT" sends MUST preserve the original SoundFont PCM.
// The public --wet SoundFont has real send generators; the chip/FX
// conversion is real and no mock DSP or owner ROM enters the public build.
void verifyP165RealChipFxSendAndPresetPCM(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"vintage.sf2");
  const auto pcm=[&](bool manual,double send,int rev,int chorus,
                     bool resetToOriginal=false){
    Plugin p(factory,id,host);p.start(0,1,44100.);
    EventList events;
    ParameterChanges params(10);
    std::array<float,frames> left{},right{};
    float* ptr[2]={left.data(),right.data()};
    AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=ptr;
    ProcessData data{};data.processMode=kRealtime;
    data.symbolicSampleSize=kSample32;data.numSamples=frames;
    data.numOutputs=1;data.outputs=&out;
    data.inputEvents=&events;data.inputParameterChanges=&params;
    std::uint64_t hash=14695981039346656037ull;
    double energy=0.;
    for(int block=0;block<66;++block){
      events.clear();params.clearQueue();
      if(block==0){
        auto add=[&](ParamID id,ParamValue value){
            int32 idx=0,point=0;
            auto* queue=params.addParameterData(id,idx);
            check(queue && queue->addPoint(0,value,point)==kResultOk,
                  "P165 real chip FX SEND automation queue not accepted");
        };
        if(manual){
           add(1420u,1.);add(1421u,1.);
           add(1110u,send);add(1111u,send);
        }
        if(resetToOriginal){
           // DELIBERATELY supply mode-before-knob at same sample to
           // adversarially challenge host queue enumeration order.
           add(1420u,0.);add(1421u,0.);
           add(1110u,0.);add(1111u,0.);
        }
        if(rev>=0)add(1400u,static_cast<double>(rev)/7.);
        if(chorus>=0)add(1401u,static_cast<double>(chorus)/7.);
        Event note{};note.type=Event::kNoteOnEvent;note.busIndex=0;
        note.sampleOffset=12;note.noteOn.channel=0;
        note.noteOn.pitch=60;note.noteOn.velocity=100.f/127.f;
        check(events.addEvent(note)==kResultOk,
              "P165 original chip FX NoteOn generator failed");
      }
      if(block==26){
        Event off{};off.type=Event::kNoteOffEvent;off.busIndex=0;
        off.sampleOffset=400;off.noteOff.channel=0;off.noteOff.pitch=60;
        check(events.addEvent(off)==kResultOk,
              "P165 original chip FX NoteOff generator failed");
      }
      check(p.processor->process(data)==kResultOk,
            "P165 actual ORIGINAL 86Box chip/FX VST3 process failed");
      for(unsigned i=0;i<frames;++i){
         for(float v:{left[i],right[i]}){
           check(std::isfinite(v),"P165 original chip FX PCM not finite");
           energy+=double(v)*double(v);
           std::uint32_t bits=0;std::memcpy(&bits,&v,4u);
           hash=(hash^bits)*1099511628211ull;
         }
      }
    }
    check(energy>0.001,"P165 synthetic wet 86Box original chip produced silence");
    return hash;
  };
  const auto original=pcm(false,0.,-1,-1);
  const auto sendsZero=pcm(true,0.,-1,-1);
  const auto sendsHundred=pcm(true,1.,-1,-1);
  const auto backToOriginal=pcm(false,0.,-1,-1,true);
  const auto differentTypes=pcm(false,0.,7,7);
  check(original!=sendsZero,
        "P165 explicit 0% SEND produced original wet PCM: hardware sends not zero");
  check(sendsZero!=sendsHundred,
        "P165 explicit 0 and 100% original chip send values have no real PCM effect");
  check(original==backToOriginal,
        "P165 SEND RESET did not recover exact original SF2/Creative PCM after mode-before-value host events");
  check(original!=differentTypes,
        "P165 reverb/chorus type selectors changed parameters but not real chip PCM");
  std::puts("P165_ACTUAL_ORIGINAL_EMU8000_VST3_DEFAULT_VS_ZERO_VS_100_SEND_PCM_PASS");
  std::puts("P165_ACTUAL_ORIGINAL_EMU8000_VST3_CHORUS_REVERB_TYPES_CHANGE_PCM_PASS");
  ownerBank(resources,nullptr);
}

}

// P175 genuine original Win95 EMU8000 32-hardware-voice oracle (not 32 MIDI
// keys through a fake/simulated mixer). A dedicated public SF2 has exactly ONE
// active sample region and a loop per key: all 32 hardware voices can therefore
// be driven independently. Each of 33 different notes is released in an
// independent fresh VST3, then compared SAMPLE-BY-SAMPLE to an identically
// timed still-held reference. Exactly 32 keys MUST alter the 86Box chip PCM;
// exactly one stolen key MUST NOT. This would FAIL if the host only played
// 16 or 30 voices, or if it played 33.
void verifyP175ThirtyTwoOriginalChipVoicesAndSustain(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  check(fs::is_regular_file(resources/"voice32.sf2"),
        "P175 single-zone original-chip SF2 fixture missing");
  ownerBank(resources,"voice32.sf2");
  constexpr int kStartNote=36;
  constexpr int kProbeNotes=33;
  constexpr int kAudioFrames=512;
  constexpr int kWarmBlocks=8;
  constexpr int kObserveBlocks=8;
  constexpr float kVelocity=.18f;
  const auto makeNote=[&](bool on,int pitch,int offset){
    Event e{};e.sampleOffset=offset;e.busIndex=0;
    if(on){
      e.type=Event::kNoteOnEvent;e.noteOn.channel=0;
      e.noteOn.pitch=static_cast<int16>(pitch);
      e.noteOn.velocity=kVelocity;
    }else{
      e.type=Event::kNoteOffEvent;e.noteOff.channel=0;
      e.noteOff.pitch=static_cast<int16>(pitch);
    }
    return e;
  };
  const auto render=[&](int noteOffKey){
    Plugin unit(factory,id,host);
    unit.start(0,1,44100.);
    EventList events(64);
    std::array<float,512> left{},right{};
    float* ptr[2]={left.data(),right.data()};
    AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=ptr;
    ProcessData pd{};pd.processMode=kRealtime;pd.symbolicSampleSize=kSample32;
    pd.numSamples=kAudioFrames;pd.numOutputs=1;pd.outputs=&out;
    pd.inputEvents=&events;
    for(int block=0;block<kWarmBlocks;++block){
      events.clear();
      if(block==0)for(int i=0;i<kProbeNotes;++i){
        Event on=makeNote(true,kStartNote+i,8+8*i);
        check(events.addEvent(on)==kResultOk,
              "P175 host could not deliver 33 separately timed NoteOns");
      }
      check(unit.processor->process(pd)==kResultOk,
            "P175 original chip 33-note pre-roll callback failed");
    }
    std::vector<float> pcm;
    pcm.reserve(2u*kAudioFrames*kObserveBlocks);
    for(int block=0;block<kObserveBlocks;++block){
      events.clear();
      if(block==0 && noteOffKey>=0){
        Event off=makeNote(false,noteOffKey,3);
        check(events.addEvent(off)==kResultOk,
              "P175 host NoteOff voice counterfactual enqueue failed");
      }
      check(unit.processor->process(pd)==kResultOk,
            "P175 original chip voice-release callback failed");
      for(int i=0;i<kAudioFrames;++i){
        check(std::isfinite(left[i]) && std::isfinite(right[i]),
              "P175 actual 86Box chip produced non-finite PCM");
        pcm.push_back(left[i]);pcm.push_back(right[i]);
      }
    }
    return pcm;
  };
  const auto baseline=render(-1);
  double energy=0.;
  unsigned clips=0;
  for(float sample:baseline){
    energy+=double(sample)*sample;
    if(std::abs(sample)>=.9999f)++clips;
  }
  check(energy>0.001,
        "P175 33-note original chip counterfactual would be vacuous (silent)");
  check(clips<baseline.size()/50u,
        "P175 33-note original chip test invalid: excessive saturation");
  // Negative oracle: NoteOff for a pitch that never started MUST NOT
  // perturb original-chip PCM or the host parameters.
  check(render(kStartNote-1)==baseline,
        "P175 absent-note NoteOff wrongly changed original-chip sound");
  int audibleNotes=0,stolenNotes=0;
  for(int key=0;key<kProbeNotes;++key){
    const auto offPcm=render(kStartNote+key);
    check(offPcm.size()==baseline.size(),
          "P175 independent VST3 note-off comparison length mismatch");
    double difference=0.;
    for(std::size_t i=0;i<baseline.size();++i){
      const double d=double(offPcm[i])-double(baseline[i]);
      difference+=d*d;
    }
    if(difference>1e-10)++audibleNotes;
    else ++stolenNotes;
    std::printf("P175_ORIGINAL_CHIP_HW_VOICE pitch=%d note_off_difference=%.10f active=%d\n",
                kStartNote+key,difference,difference>1e-10);
  }
  check(audibleNotes==32 && stolenNotes==1,
        "P175 physical 32-voice limit or original Win95 voice stealing violated");
  std::printf("P175_32_HW_VOICES active=%d stolen=%d baseline_energy=%.8f clips=%u\n",
              audibleNotes,stolenNotes,energy,clips);

  // CC64 must gate NoteOff on ALL 32 voices, not merely one note or channel.
  // Two independent VST3 instances process identical NoteOns and exact
  // sample times. One gets Sustain Down + all NoteOffs; the other stays
  // physically held. PCM must remain bit-identical while the pedal is down;
  // pedal-up must release the actually held voices.
  Plugin held(factory,id,host),pedal(factory,id,host);
  held.start(0,1,44100.);pedal.start(0,1,44100.);
  const auto mapping=FUnknownPtr<IMidiMapping>(pedal.controller);
  check(bool(mapping),"P175 pedal controller interface absent");
  ParamID cc64=0;
  check(mapping->getMidiControllerAssignment(0,0,kCtrlSustainOnOff,cc64)==kResultTrue,
        "P175 genuine VST3 CC64 sustain controller not mapped");
  std::array<float,512> holdL{},holdR{},pedalL{},pedalR{};
  float* hch[2]={holdL.data(),holdR.data()};
  float* pch[2]={pedalL.data(),pedalR.data()};
  AudioBusBuffers outH{},outP{};
  outH.numChannels=outP.numChannels=2;
  outH.channelBuffers32=hch;outP.channelBuffers32=pch;
  EventList notesH(64),notesP(64);
  ParameterChanges changes(4);
  ProcessData hd{},pd{};
  hd.processMode=pd.processMode=kRealtime;
  hd.symbolicSampleSize=pd.symbolicSampleSize=kSample32;
  hd.numSamples=pd.numSamples=kAudioFrames;
  hd.numOutputs=pd.numOutputs=1;
  hd.outputs=&outH;pd.outputs=&outP;
  hd.inputEvents=&notesH;pd.inputEvents=&notesP;
  pd.inputParameterChanges=&changes;
  double heldEnergy=0.,afterPedalDifference=0.,afterReleaseEnergy=0.;
  for(int block=0;block<40;++block){
    notesH.clear();notesP.clear();changes.clearQueue();
    if(block==0)for(int i=0;i<32;++i){
      auto on=makeNote(true,kStartNote+i,8+8*i);
      check(notesH.addEvent(on)==kResultOk && notesP.addEvent(on)==kResultOk,
            "P175 32-voice sustain host NoteOn queue failed");
    }
    if(block==5 || block==16){
      int32 queue=0,point=0;
      auto* q=changes.addParameterData(cc64,queue);
      check(q && q->addPoint(0,block==5?1.:0.,point)==kResultOk,
            "P175 VST3 sustain-pedal sample-timed host queue failed");
    }
    if(block==6)for(int i=0;i<32;++i){
      auto off=makeNote(false,kStartNote+i,8+8*i);
      check(notesP.addEvent(off)==kResultOk,
            "P175 32-note physical pedal-held NoteOff enqueue failed");
    }
    check(held.processor->process(hd)==kResultOk &&
          pedal.processor->process(pd)==kResultOk,
          "P175 genuine original-chip 32-voice sustain callback failed");
    for(int i=0;i<kAudioFrames;++i){
      for(float sample:{holdL[i],holdR[i],pedalL[i],pedalR[i]})
        check(std::isfinite(sample),"P175 32-voice pedal PCM nonfinite");
      if(block>=7 && block<=15){
        check(holdL[i]==pedalL[i] && holdR[i]==pedalR[i],
              "P175 CC64 pedal-down changed held original-chip PCM after NoteOff");
        heldEnergy+=double(holdL[i])*holdL[i]+double(holdR[i])*holdR[i];
      }
      if(block>=20 && block<=38){
        const double dL=double(holdL[i])-double(pedalL[i]);
        const double dR=double(holdR[i])-double(pedalR[i]);
        afterPedalDifference+=dL*dL+dR*dR;
        if(block>=32)
          afterReleaseEnergy+=double(pedalL[i])*pedalL[i]+
                              double(pedalR[i])*pedalR[i];
      }
    }
  }
  check(heldEnergy>0.001,
        "P175 CC64 pedal-down control was silent, no valid hold oracle");
  check(afterPedalDifference>1e-7,
        "P175 sustain-pedal release did not change 32-note chip PCM");
  check(afterReleaseEnergy<heldEnergy*.05,
        "P175 voices remained stuck after releasing the actual CC64 pedal");
  ownerBank(resources,nullptr);
  std::printf("P175_32_VOICE_CC64 held=%.8f post_pedal_diff=%.8f post_release=%.8f\n",
              heldEnergy,afterPedalDifference,afterReleaseEnergy);
  std::puts("P175_ACTUAL_WIN64_VST3_ORIGINAL_EMU8000_EXACT_32_VOICE_STEAL_SUSTAIN_PASS");
}

// P174 exact EMU8000 physical sample RAM bounds and *active* rejection:
// Win95 original chip reserves 16 words, public SBK uses 1536 PCM words
// plus 8 guard words, and a newly loaded SF2 adds another 8 guard words.
// 28*1024*1024/2 - 16 - 1536 - 8 - 8 = 14678496 user PCM words.
// A valid SoundFont with 14678496 words MUST load; 14678497 MUST reject,
// without a changed running engine, active project identity or preset state.
// This is a hardware-word test, NOT an SF2 file-length approximation.
std::vector<std::uint8_t> p174V5State(const fs::path& path){
  struct Ext {
    std::uint32_t magic=0x35534150u,bytes=0u;
    std::array<std::uint16_t,12> controls{};
    std::array<std::uint8_t,32> sha{};
    std::array<std::uint16_t,2> effects{{4u,2u}};
  };
  static_assert(sizeof(Ext)==68u,"P174 state header layout drift");
  std::array<std::uint8_t,32> digest{};
  check(p009::sha256FileNonRealtime(path,digest),
        "P174 owner-independent SF2 hash read failed");
  const auto pathUtf8=path.u8string();
  const std::string name(pathUtf8.begin(),pathUtf8.end());
  check(!name.empty() && name.size()<4096u,"P174 path outside V5 limit");
  Saved state{};
  state.version=5u;state.bank=1u;
  state.midiStates.fill(0xFFFFu);
  state.advancedStates.fill(0xFFFFu);
  Ext ext{};ext.bytes=static_cast<std::uint32_t>(name.size());
  ext.sha=digest;
  std::vector<std::uint8_t> wire(sizeof(state)+sizeof(ext)+name.size());
  std::memcpy(wire.data(),&state,sizeof(state));
  std::memcpy(wire.data()+sizeof(state),&ext,sizeof(ext));
  std::memcpy(wire.data()+sizeof(state)+sizeof(ext),name.data(),name.size());
  return wire;
}
tresult p174ApplyV5(Plugin& plugin,const std::vector<std::uint8_t>& state){
  MemoryStream mem;int32 n=0;
  check(mem.write(const_cast<std::uint8_t*>(state.data()),
                  static_cast<int32>(state.size()),&n)==kResultOk &&
        n==static_cast<int32>(state.size()) &&
        mem.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,
        "P174 external SF2 V5 state stream invalid");
  return plugin.component->setState(&mem);
}
std::vector<std::uint8_t> p174Saved(Plugin& plugin){
  MemoryStream mem;
  check(plugin.component->getState(&mem)==kResultOk,
        "P174 native VST3 state save rejected");
  const auto* b=reinterpret_cast<const std::uint8_t*>(mem.getData());
  check(b && mem.getSize()>0,"P174 empty VST3 project save");
  return std::vector<std::uint8_t>(b,b+mem.getSize());
}
void verifyP174OneWordPhysicalRamBoundary(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  const auto exact=resources/"exact-fit.sf2";
  const auto over=resources/"one-word-over.sf2";
  constexpr std::uint64_t totalWords=28ull*1024ull*1024ull/2ull;
  constexpr std::uint64_t baseWords=16ull+1536ull+8ull;
  constexpr std::uint64_t userWords=totalWords-baseWords-8ull;
  check(fs::is_regular_file(exact) && fs::is_regular_file(over) &&
        fs::file_size(over)==fs::file_size(exact)+2ull &&
        userWords==14678496ull,
        "P174 28 MiB exact-fit/+one hardware boundary fixtures missing");
  ownerBank(resources,"vintage.sf2");
  Plugin running(factory,id,host),reference(factory,id,host);
  running.start(0,1,44100.);reference.start(0,1,44100.);
  const auto exactState=p174V5State(exact);
  const auto overState=p174V5State(over);
  check(p174ApplyV5(running,exactState)==kResultOk &&
        p174ApplyV5(reference,exactState)==kResultOk,
        "P174 exact 28 MiB EMU8000 DRAM bank incorrectly rejected");
  check(p174Saved(running)==exactState &&
        p174Saved(reference)==exactState,
        "P174 exact-fit SoundFont identity was not committed");
  // Both running Synth instances must render equal REAL chip PCM after
  // an accepted exact-capacity upload, not merely parse RIFF metadata.
  const auto expected=p015Play(reference,0,60);
  check(p015Play(running,0,60)==expected,
        "P174 accepted exact-fit SF2 failed genuine EMU8000 PCM parity");
  const auto saved=p174Saved(running);
  check(p174ApplyV5(running,overState)!=kResultOk,
        "P174 one-sample-word over 28 MiB was incorrectly accepted");
  check(p174Saved(running)==saved,
        "P174 rejected one-word-over SF2 changed accepted project identity");
  check(p015Play(running,0,60)==p015Play(reference,0,60),
        "P174 one-word-over rejection corrupted running EMU8000 PCM");
  // A small bank *after* the boundary rejection must still load and sound.
  const auto smaller=p174V5State(resources/"77-presets.sf2");
  check(p174ApplyV5(running,smaller)==kResultOk &&
        p174Saved(running)==smaller,
        "P174 valid small SoundFont cannot be loaded after RAM rejection");
  check(p015Play(running,0,60)!=0u,
        "P174 small bank after rejection did not render original-chip PCM");
  ownerBank(resources,nullptr);
  std::printf("P174_EMU8000_DRAM_BOUNDARY exact=%llu total=%llu over=%llu\n",
      static_cast<unsigned long long>(userWords),
      static_cast<unsigned long long>(totalWords),
      static_cast<unsigned long long>(userWords+1ull));
  std::puts("P174_ACTUAL_WINDOWS_VST3_28MIB_EXACT_FIT_ONE_WORD_OVER_REJECT_ROLLBACK_PCM_PASS");
}
std::size_t p174PrivateCommittedBytes() {
  PROCESS_MEMORY_COUNTERS_EX counters{};
  counters.cb=sizeof(counters);
  check(GetProcessMemoryInfo(GetCurrentProcess(),
    reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
    static_cast<DWORD>(sizeof(counters)))!=FALSE,
    "P174 process private memory counter unreadable");
  return static_cast<std::size_t>(counters.PrivateUsage);
}
// P174 60-cycle active VST3 SF2/host recall endurance: each cycle builds
// a new complete original-chip bank non-realtime, rejects a corrupted SHA
// without modifying any state, and plays audio through process(). Memory
// must not grow by even a handful of leaked 28-MiB chip instances.
void verifyP174SixtyActiveValidInvalidLoads(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"vintage.sf2");
  Plugin unit(factory,id,host);
  unit.start(0,1,44100.);
  const auto a=p174V5State(resources/"vintage.sf2");
  const auto b=p174V5State(resources/"77-presets.sf2");
  auto memAtTen=std::size_t{0};
  auto memAtEnd=std::size_t{0};
  for(int cycle=0;cycle<60;++cycle){
    const auto& good=cycle%2?a:b;
    check(p174ApplyV5(unit,good)==kResultOk,
          "P174 repeated valid SF2 rejected after an earlier swap");
    check(p174Saved(unit)==good,
          "P174 running project state failed to follow active SF2 selection");
    auto bad=good;
    constexpr std::size_t shaOffset=sizeof(Saved)+8u+24u;
    check(bad.size()>shaOffset,"P174 V5 SHA offset invalid");
    bad[shaOffset]^=0x40u;
    check(p174ApplyV5(unit,bad)!=kResultOk,
          "P174 malformed SHA was accepted during 60-cycle endurance");
    check(p174Saved(unit)==good,
          "P174 invalid owner SF2 mutated previous active project");
    // This real instrument/86Box chip generates and releases the note,
    // allowing the old 15-ms audible chip transition to retire normally.
    (void)p015Play(unit,0,60);
    if(cycle==9)memAtTen=p174PrivateCommittedBytes();
    if(cycle==59)memAtEnd=p174PrivateCommittedBytes();
  }
  const auto increase=memAtEnd>memAtTen?memAtEnd-memAtTen:0u;
  std::printf("P174_60_CYCLES_RAM privateMB_at10=%.2f privateMB_at60=%.2f growthMiB=%.2f\n",
     double(memAtTen)/1048576.,double(memAtEnd)/1048576.,
     double(increase)/1048576.);
  check(increase<192u*1024u*1024u,
        "P174 60 active SF2 swaps leaked multiple original 28-MiB chip heaps");
  ownerBank(resources,nullptr);
  std::puts("P174_ACTUAL_WINDOWS_VST3_SIXTY_ACCEPT_REJECT_AUDIO_RECALL_HEAP_BOUNDED_PASS");
}


// P176: A new V5 SF2 can arrive while a prior 15-ms fade is still
// rendering.  The real host is NOT obliged to call getState(), open GUI,
// or perform any other non-realtime action after the final setState().
// In particular the previous one-slot retiredSynth_ must not make the
// second staged asset wait forever after the first fade completes.
void verifyP176RapidSf2SwitchForwardProgress(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"vintage.sf2");
  const auto sustain=p174V5State(resources/"sustain.sf2");
  const auto latest=p174V5State(resources/"77-presets.sf2");
  Plugin active(factory,id,host);
  active.start(0,1,44100.);
  std::array<float,frames> left{},right{};
  float* outPtrs[2]={left.data(),right.data()};
  AudioBusBuffers out{};
  out.numChannels=2;out.channelBuffers32=outPtrs;
  EventList events(8);
  ProcessData data{};data.processMode=kRealtime;
  data.symbolicSampleSize=kSample32;data.numSamples=64;
  data.numOutputs=1;data.outputs=&out;data.inputEvents=&events;
  Event e{};e.type=Event::kNoteOnEvent;
  e.noteOn.channel=0;e.noteOn.pitch=60;e.noteOn.velocity=1.f;
  check(events.addEvent(e)==kResultOk,
        "P176 original-chip audible pre-fade note unavailable");
  double initial=0.;
  for(int i=0;i<24;++i){
    if(i)events.clear();
    check(active.processor->process(data)==kResultOk,
          "P176 original chip pre-fade playback callback failed");
    for(int n=0;n<64;++n)
      initial+=double(left[n])*left[n]+double(right[n])*right[n];
  }
  check(initial>0.001,
        "P176 test fixture had no audible outgoing original-chip note");
  check(p174ApplyV5(active,sustain)==kResultOk,
        "P176 first external SF2 project restore rejected");
  // Begin actual 15-ms A->B audible fading. ONLY a 64-sample block
  // elapsed, so the first engine is still rendering.
  check(active.processor->process(data)==kResultOk,
        "P176 first audible bank fade failed");
  check(p174ApplyV5(active,latest)==kResultOk,
        "P176 second quick external SF2 project restore rejected");
  // No getState(), createView(), GUI idle, setState(), or other
  // non-realtime callback is permitted after the second state restore.
  // A DAW can simply keep rendering until the next note arrives.
  for(int i=0;i<60;++i){
    check(active.processor->process(data)==kResultOk,
          "P176 original chip continued-audio callback failed");
    for(int n=0;n<64;++n)
      check(std::isfinite(left[n])&&std::isfinite(right[n]),
            "P176 rapid SF2 swap generated nonfinite audio");
  }
  // Distinct ORIGINAL-chip fixture controls. The exact selected C bank
  // must be audible without an editor or project-save callback.
  // An actual chip's silent oscillator/FX FIFO evolves with time.
  // C was installed only AFTER the remaining A->B fade completed:
  // ceil(44100*.015)=662; 64 fade samples in first callback; 10 further
  // 64-frame blocks to finish; C then rendered in the final 50 blocks.
  // An immediate NoteOn in a fresh C instance is NOT a bit-exact timing
  // oracle. Independently warm C for the same 50 blocks before comparing.
  const int fadeFrames=static_cast<int>(std::ceil(44100.*.015));
  const int blocksBeforeC=(fadeFrames-64+63)/64;
  const int latestBlocks=60-blocksBeforeC;
  check(latestBlocks==50,"P176 independent chip-timeline oracle drift");
  const auto warm=[&](Plugin& p){
    std::array<float,64> l{},r{};
    float* channels[2]={l.data(),r.data()};
    AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;
    ProcessData pd{};pd.processMode=kRealtime;
    pd.symbolicSampleSize=kSample32;pd.numSamples=64;
    pd.numOutputs=1;pd.outputs=&bus;
    for(int i=0;i<latestBlocks;++i)
      check(p.processor->process(pd)==kResultOk,
            "P176 independent post-swap chip idle preroll failed");
  };
  ownerBank(resources,"77-presets.sf2");
  Plugin expected(factory,id,host);expected.start(0,1,44100.);
  warm(expected);
  const auto wanted=p015Play(expected,0,60);
  ownerBank(resources,"sustain.sf2");
  Plugin wrong(factory,id,host);wrong.start(0,1,44100.);
  warm(wrong);
  const auto unwanted=p015Play(wrong,0,60);
  check(wanted!=unwanted,
        "P176 source SoundFonts have indistinguishable chip PCM");
  const auto actual=p015Play(active,0,60);
  std::printf("P176_RAPID_SF2_AUDIO target=%016llX latest=%016llX obsolete=%016llX\n",
     static_cast<unsigned long long>(actual),
     static_cast<unsigned long long>(wanted),
     static_cast<unsigned long long>(unwanted));
  check(actual==wanted,
        "P176 second SF2 never became active without host GUI/getState callback");
  // A host snapshot after audio is now safe, but it must not be needed
  // to make the second instrument audible. Exact V5 identity retained.
  check(p174Saved(active)==latest,
        "P176 two rapid V5 loads lost final original-bank file identity");
  ownerBank(resources,nullptr);
  std::puts("P176_ACTUAL_WIN64_VST3_RAPID_DOUBLE_SF2_SWAP_WITHOUT_GUI_GETSTATE_PCM_PASS");
}


// P177: A host may restore stale, partially saved or corrupt VST3 projects.
// No byte sequence is allowed to escape an exception into the DAW, to turn
// a truncated V5 stream into success, or to mutate a previously accepted
// external SF2. Test *every* partial length, all 32 hash bytes and malformed
// controller/path/FX metadata through the ACTUAL compiled Windows VST3.
void verifyP177CorruptV5StreamFailClosed(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"vintage.sf2");
  Plugin unit(factory,id,host);unit.start(0,1,44100.);
  const auto valid=p174V5State(resources/"77-presets.sf2");
  check(p174ApplyV5(unit,valid)==kResultOk,
        "P177 positive external SF2 fixture rejected");
  const auto stable=p174Saved(unit);
  check(stable==valid,"P177 positive original project identity not committed");
  int rejected=0;
  const auto invalid=[&](const std::vector<std::uint8_t>& bad){
    bool accepted=false,threw=false;
    try {
      accepted=p174ApplyV5(unit,bad)==kResultOk;
    }catch(...){
      threw=true;
    }
    if(threw)
      std::fprintf(stderr,
        "P177_UNCAUGHT_VST3_EXCEPTION badBytes=%llu pathFirst=%02X\n",
        static_cast<unsigned long long>(bad.size()),
        bad.size()>sizeof(Saved)+68u?unsigned(bad[sizeof(Saved)+68u]):0u);
    check(!threw,
          "P177 V5 restore threw across Steinberg plugin ABI (DAW crash risk)");
    check(!accepted,
          "P177 corrupt V5 project was accepted by the real Windows VST3");
    check(p174Saved(unit)==stable,
          "P177 corrupt V5 project modified previous accepted SHA/state");
    ++rejected;
  };
  for(std::size_t bytes=1;bytes<valid.size();++bytes)
    invalid(std::vector<std::uint8_t>(valid.begin(),valid.begin()+bytes));
  constexpr std::size_t shaOffset=sizeof(Saved)+8u+24u;
  for(std::size_t i=0;i<32;++i){
    auto b=valid;b[shaOffset+i]^=0x55u;invalid(b);
  }
  // Original file path length, byte patterns and available SHA may be
  // hostile. A bad UTF-8 sequence must return a normal failure result,
  // never throw std::filesystem_error or std::range_error at the host.
  for(const unsigned byte:{0u,0xFFu,0xFEu,0xC0u,0x80u}){
    auto b=valid;b[sizeof(Saved)+68u]=static_cast<std::uint8_t>(byte);
    invalid(b);
  }
  for(const std::uint32_t value:{128u,255u,0xffffffffu}){
    auto b=valid;
    std::memcpy(b.data()+8u,&value,sizeof(value));
    invalid(b); // invalid program
  }
  for(const std::uint32_t value:{3u,0xffffffffu}){
    auto b=valid;
    std::memcpy(b.data()+12u,&value,sizeof(value));
    invalid(b); // invalid bank
  }
  for(const std::uint32_t value:{4097u,0xffffffffu}){
    auto b=valid;
    std::memcpy(b.data()+sizeof(Saved)+4u,&value,sizeof(value));
    invalid(b); // bounded path length
  }
  for(const std::uint32_t invalidFloat:{0x7fc00000u,0x7f800000u,0xbf800000u}){
    auto b=valid;
    std::memcpy(b.data()+16u,&invalidFloat,sizeof(invalidFloat));
    invalid(b);
  }
  {
    auto b=valid;b[sizeof(Saved)]=0x21u;invalid(b); // V5 magic
  }
  {
    auto b=valid;b[sizeof(Saved)+64u]=0xFFu;invalid(b); // FX mode bits
  }
  {
    auto b=valid;b[sizeof(Saved)+8u]=0xFFu;
    b[sizeof(Saved)+9u]=0xFFu;
    invalid(b); // GUI control >10000
  }
  // Every accepted external file state must still make real ORIGINAL-chip
  // audio after all rejections; mere getState equality is not sufficient.
  const auto voice=p015Play(unit,0,60);
  check(voice!=0u,
        "P177 valid original-chip bank not playable after malformed stream flood");
  ownerBank(resources,nullptr);
  std::printf("P177_CORRUPT_V5_STATE rejected=%d playable_hash=%016llX\n",
              rejected,static_cast<unsigned long long>(voice));
  check(rejected>=650,
        "P177 negative controls did not cover all V5 truncation boundaries");
  std::puts("P177_ACTUAL_WIN64_VST3_FULL_V5_TRUNCATION_UTF8_SHA_FX_FAIL_CLOSED_PASS");
}


// P178: The host can serialize the VST3 project while a different NON-RT
// thread commits an external SoundFont and the audio callback is running.
// A component snapshot must match A or B exactly: never A's MIDI program
// and B's SF2 SHA/path, or vice versa. Public synthetic original-chip data.
void verifyP178ConcurrentOwnerSF2StateTransaction(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"vintage.sf2");
  Plugin unit(factory,id,host);
  unit.start(0,1,44100.);
  auto A=p174V5State(resources/"vintage.sf2");
  auto B=p174V5State(resources/"77-presets.sf2");
  Saved a{},b{};
  std::memcpy(&a,A.data(),sizeof(a));
  std::memcpy(&b,B.data(),sizeof(b));
  a.program=3u; a.master=.375f;
  b.program=87u; b.master=.875f;
  std::memcpy(A.data(),&a,sizeof(a));
  std::memcpy(B.data(),&b,sizeof(b));
  check(p174ApplyV5(unit,A)==kResultOk,
        "P178 initial valid owner A bank rejected");
  // Commit A to chip before starting concurrent getState snapshots.
  {
    std::array<float,64> l{},r{};
    float* ch[2]={l.data(),r.data()};
    AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=ch;
    ProcessData pd{};pd.processMode=kRealtime;
    pd.symbolicSampleSize=kSample32;pd.numSamples=64;
    pd.numOutputs=1;pd.outputs=&bus;
    check(unit.processor->process(pd)==kResultOk,
          "P178 initial A project callback failed");
  }
  std::atomic<bool> stop{false},ok{true};
  std::atomic<std::uint64_t> audioBlocks{0},observations{0},torn{0},saveErrors{0};
  std::array<std::uint8_t,32> mismatched{};
  std::atomic<bool> gotMismatch{false};
  std::thread audio([&](){
    std::array<float,64> l{},r{};
    float* ch[2]={l.data(),r.data()};
    AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=ch;
    ProcessData pd{};pd.processMode=kRealtime;
    pd.symbolicSampleSize=kSample32;pd.numSamples=64;
    pd.numOutputs=1;pd.outputs=&bus;
    while(!stop.load(std::memory_order_acquire)){
      if(unit.processor->process(pd)!=kResultOk ||
         !std::isfinite(l[0])||!std::isfinite(r[0])){
        ok.store(false,std::memory_order_release);break;
      }
      audioBlocks.fetch_add(1,std::memory_order_relaxed);
    }
  });
  std::thread snapshot([&](){
    while(!stop.load(std::memory_order_acquire)){
      try {
        MemoryStream mem;
        if(unit.component->getState(&mem)!=kResultOk){
          saveErrors.fetch_add(1,std::memory_order_relaxed);
          continue;
        }
        const auto size=static_cast<std::size_t>(mem.getSize());
        const auto* raw=reinterpret_cast<const std::uint8_t*>(mem.getData());
        bool isA=size==A.size() && raw &&
                 std::memcmp(raw,A.data(),size)==0;
        bool isB=size==B.size() && raw &&
                 std::memcmp(raw,B.data(),size)==0;
        if(!isA&&!isB){
          torn.fetch_add(1,std::memory_order_relaxed);
          if(!gotMismatch.exchange(true,std::memory_order_acq_rel) && raw){
            const auto n=std::min<std::size_t>(mismatched.size(),size);
            std::memcpy(mismatched.data(),raw,n);
          }
        }
        observations.fetch_add(1,std::memory_order_relaxed);
      }catch(...){
        ok.store(false,std::memory_order_release);break;
      }
    }
  });
  // Loading the actual 28-MiB chip on the NON-RT thread creates a broad
  // overlap in which the audio thread continues to process, while the
  // saver polls. Different MIDI program and master make split generations
  // observable even if owner path lengths happened to be equal.
  bool setOk=true;
  for(int cycle=0;cycle<80 && setOk;++cycle){
    const auto& state=cycle%2?A:B;
    try {setOk=p174ApplyV5(unit,state)==kResultOk;}
    catch(...){setOk=false;}
    std::this_thread::yield();
  }
  // Allow last audio boundary to acknowledge pending project and finish
  // the 15-ms fade before taking the final exact owner snapshot.
  for(int i=0;i<100000;++i){
    if(audioBlocks.load(std::memory_order_acquire)>4000u)break;
    std::this_thread::yield();
  }
  stop.store(true,std::memory_order_release);
  audio.join();snapshot.join();
  std::printf("P178_CONCURRENT_V5_TX snapshots=%llu torn=%llu save_errors=%llu audio_blocks=%llu first_mismatch=%02X%02X%02X%02X\n",
    static_cast<unsigned long long>(observations.load()),
    static_cast<unsigned long long>(torn.load()),
    static_cast<unsigned long long>(saveErrors.load()),
    static_cast<unsigned long long>(audioBlocks.load()),
    unsigned(mismatched[0]),unsigned(mismatched[1]),
    unsigned(mismatched[2]),unsigned(mismatched[3]));
  check(setOk && ok.load(std::memory_order_acquire),
        "P178 concurrent SF2 state load or original-chip audio callback failed");
  check(observations.load()>50u && audioBlocks.load()>50u,
        "P178 concurrent host snapshot and original-chip audio not exercised");
  check(saveErrors.load()==0u && torn.load()==0u,
        "P178 getState captured TORN project (SF2 SHA/path vs MIDI/master generation)");
  ownerBank(resources,nullptr);
  std::puts("P178_ACTUAL_WIN64_VST3_CONCURRENT_EXTERNAL_SF2_STATE_AUDIO_TRANSACTION_PASS");
}


// P179: Unlike P178's deliberately unpaced adversarial 3-thread
// throughput test, this is a NORMAL DAW-cadence save test with a real
// sounding, looping ORIGINAL EMU8000 hardware voice and a constant
// external SF2. The host must reliably save 500/500 valid V5 states
// during live audio processing without requiring any GUI or transport stop.
void verifyP179PacedAudibleHostSaves(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"voice32.sf2");
  Plugin unit(factory,id,host);unit.start(0,1,44100.);
  const auto expected=p174V5State(resources/"voice32.sf2");
  check(p174ApplyV5(unit,expected)==kResultOk,
        "P179 genuine SF2 bank could not become active before playback");
  constexpr int blockFrames=128;
  constexpr int totalBlocks=700;
  constexpr int saveCount=500;
  std::atomic<int> doneBlocks{0};
  std::atomic<bool> audioOk{true};
  double audioEnergy=0.,finalEnergy=0.,maxCallbackMs=0.;
  std::thread realAudio([&](){
    std::array<float,128> left{},right{};
    float* ptr[2]={left.data(),right.data()};
    AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=ptr;
    EventList notes(2);
    ProcessData data{};
    data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
    data.numSamples=blockFrames;data.numOutputs=1;
    data.outputs=&bus;data.inputEvents=&notes;
    const auto start=std::chrono::steady_clock::now();
    for(int block=0;block<totalBlocks;++block){
      const auto offsetNs=static_cast<long long>(
           (static_cast<std::uint64_t>(block)*blockFrames*
            1000000000ull)/44100ull);
      std::this_thread::sleep_until(start+std::chrono::nanoseconds(offsetNs));
      notes.clear();
      if(block==0){
        Event note{};
        note.type=Event::kNoteOnEvent;note.busIndex=0;note.sampleOffset=12;
        note.noteOn.channel=0;note.noteOn.pitch=60;note.noteOn.velocity=.80f;
        if(notes.addEvent(note)!=kResultOk){
          audioOk.store(false,std::memory_order_release);break;
        }
      }
      const auto tick=std::chrono::steady_clock::now();
      if(unit.processor->process(data)!=kResultOk){
        audioOk.store(false,std::memory_order_release);break;
      }
      const auto tock=std::chrono::steady_clock::now();
      maxCallbackMs=std::max(maxCallbackMs,
         std::chrono::duration<double,std::milli>(tock-tick).count());
      for(int i=0;i<blockFrames;++i){
        if(!std::isfinite(left[i])||!std::isfinite(right[i])){
          audioOk.store(false,std::memory_order_release);break;
        }
        const double power=double(left[i])*left[i]+double(right[i])*right[i];
        audioEnergy+=power;
        if(block>=650)finalEnergy+=power;
      }
      if(!audioOk.load(std::memory_order_acquire))break;
      doneBlocks.fetch_add(1,std::memory_order_release);
    }
  });
  // Guarantee real original-chip audio BEFORE the first host save, while
  // respecting the genuinely paced 128-frame/44.1k callback cadence.
  // The old 500000 x yield() loop could exhaust in 7 blocks on a fast
  // Windows host: all 700 audio blocks, positive PCM and 500/500 saves
  // were valid, but the test alone failed its arbitrary >=8 start gate.
  // Do not reduce that gate; synchronize against the audible block count
  // with a bounded HOST-side wait instead. No waits in plugin process().
  const auto readyBy=std::chrono::steady_clock::now()+
                      std::chrono::seconds(2);
  while(doneBlocks.load(std::memory_order_acquire)<8 &&
        audioOk.load(std::memory_order_acquire) &&
        std::chrono::steady_clock::now()<readyBy)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  const int blocksBeforeSaves=doneBlocks.load(std::memory_order_acquire);
  int failedSaves=0,wrongStates=0,thrown=0;
  for(int i=0;i<saveCount;++i){
    try{
      MemoryStream mem;
      if(unit.component->getState(&mem)!=kResultOk){
        ++failedSaves;
      }else{
        const auto size=static_cast<std::size_t>(mem.getSize());
        const auto* bytes=reinterpret_cast<const std::uint8_t*>(mem.getData());
        if(!bytes||size!=expected.size()||
           std::memcmp(bytes,expected.data(),size)!=0)
          ++wrongStates;
      }
    }catch(...){++thrown;}
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  const int blocksAfterSaves=doneBlocks.load(std::memory_order_acquire);
  realAudio.join();
  std::printf("P179_PACED_AUDIBLE_V5_SAVES saves=%d failed=%d torn=%d thrown=%d blocks=%d blocksBefore=%d blocksAfter=%d energy=%.8f finalEnergy=%.8f maxCallbackMs=%.4f\n",
     saveCount,failedSaves,wrongStates,thrown,
     doneBlocks.load(std::memory_order_acquire),
     blocksBeforeSaves,blocksAfterSaves,audioEnergy,finalEnergy,maxCallbackMs);
  check(audioOk.load(std::memory_order_acquire) &&
        doneBlocks.load(std::memory_order_acquire)==totalBlocks &&
        blocksBeforeSaves>=8 && blocksAfterSaves>blocksBeforeSaves+50 &&
        audioEnergy>0.001 && finalEnergy>0.00001,
        "P179 test did not continuously play a live original-chip voice");
  check(failedSaves==0 && wrongStates==0 && thrown==0,
        "P179 ordinary live 1-voice DAW playback lost valid V5 host saves");
  ownerBank(resources,nullptr);
  std::puts("P179_PACED_ACTUAL_WIN64_VST3_500_OF_500_LIVE_V5_SAVES_PASS");
}


// P180: Public 77-program soundbank with the EXACT sparse topology of the
// owner's 77-preset bank, but with independently synthesized uncopyrighted
// audio, must map SF2, MIDI and fresh V5 recall to the same original chip.
// This does NOT upload or claim to test the owner's private SoundFont in CI.
void verifyP180Sparse77OriginalChipSelection(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"sparse77.sf2");
  std::set<std::uint64_t> distinct;
  for(const int program:{0,7,62,64,77}){
    Plugin unit(factory,id,host);
    unit.start(program,1,44100.);
    const auto hash=p015Play(unit,0,60);
    check(distinct.insert(hash).second,
          "P180 two different sparse user SF2 presets made the SAME chip PCM");
    std::printf("P180_SPARSE_77_ORIGINAL_CHIP program=%d PCM=%016llX\n",
          program,static_cast<unsigned long long>(hash));
  }
  // State restore of the upper sparse program must select EXACTLY the
  // same chip patch as a fresh bank-1 MIDI ProgramChange, not just restore
  // its browser label or the correct SHA/path.
  const auto statePath=resources/"sparse77.sf2";
  auto v5=p174V5State(statePath);
  Saved saved{};
  std::memcpy(&saved,v5.data(),sizeof(saved));
  saved.program=77u;
  std::memcpy(v5.data(),&saved,sizeof(saved));
  ownerBank(resources,"vintage.sf2");
  Plugin reopened(factory,id,host);
  reopened.start(0,1,44100.);
  check(p174ApplyV5(reopened,v5)==kResultOk,
        "P180 external 77-preset SF2 V5 recall rejected");
  check(p174Saved(reopened)==v5,
        "P180 upper owner preset's V5 identity/program not preserved");
  const auto reopenPcm=p015Play(reopened,0,60);
  ownerBank(resources,"sparse77.sf2");
  Plugin direct(factory,id,host);
  direct.start(77,1,44100.);
  const auto directPcm=p015Play(direct,0,60);
  std::printf("P180_SPARSE_77_V5_PROGRAM77 recalled=%016llX direct=%016llX\n",
       static_cast<unsigned long long>(reopenPcm),
       static_cast<unsigned long long>(directPcm));
  check(reopenPcm==directPcm,
        "P180 SF2 V5 restore and MIDI program77 differ on REAL 86Box chip");
  ownerBank(resources,nullptr);
  std::puts("P180_ACTUAL_WIN64_VST3_SPARSE_77_PROGRAM_MAPPING_PCM_V5_PASS");
}


// P192: two public SoundFonts encode the SAME looping two-layer sound.
 // One writes sustainVolEnv=0 cB explicitly; the other leaves generator 37
 // unspecified, which the SF2 specification defines as 0 cB, full sustain.
 // Private owner SF2 is never copied into this public, independent oracle.
void verifyP192ImplicitSf2SustainEqualsExplicit(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  check(fs::is_regular_file(resources/"sustain-explicit.sf2") &&
        fs::is_regular_file(resources/"sustain-implicit.sf2"),
        "P192 public sustain comparison fixtures missing");
  ownerBank(resources,"sustain-explicit.sf2");
  const auto explicitFull=render(factory,id,host,
      "P192_EXPLICIT_SF2_SUSTAIN",44100,0,60,110,0,1);
  audible(explicitFull,"P192 explicit 0-cB full sustain");
  ownerBank(resources,"sustain-implicit.sf2");
  const auto implicitFull=render(factory,id,host,
      "P192_OMITTED_SF2_SUSTAIN",44100,0,60,110,0,1);
  audible(implicitFull,"P192 default 0-cB full sustain");
  check(explicitFull.hash==implicitFull.hash,
        "P192 SF2 omitted generator 37 caused blip instead of sustained bass");
  ownerBank(resources,nullptr);
  std::puts("P192_SF2_OMITTED_SUSTAIN_EQUALS_EXPLICIT_0CB_REAL_CHIP_PCM_PASS");
}

// P194: SF2 gen34/26 omitted versus explicitly -12000 in equivalent
// public SoundFonts. The original Creative SBAWE.VXD LCOD template stores
// -12000 for both defaults. Original SF1/SBK default registers must not move.
// These are output-PCM tests against the compiled Windows VST3, NOT direct
// Win95 Creative driver or physical AWE64 hardware parity tests.
void verifyP194OriginalSf2AttackDefaults(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources) {
  struct Pair {const char* omit;const char* full;const char* label;};
  constexpr std::array<Pair,2> pairs{{
      {"p193-attack-omitted.sf2","p193-attack-explicit.sf2","VOL"},
      {"p194-modattack-omitted.sf2","p194-modattack-explicit.sf2","MOD"}
  }};
  for(const int hz:{44100,48000,96000}) {
    for(const auto& p:pairs) {
      check(fs::is_regular_file(resources/p.omit) &&
            fs::is_regular_file(resources/p.full),
            "P194 public SF2 attack-default fixture missing");
      ownerBank(resources,p.omit);
      const auto implicit=render(factory,id,host,
          "P194_IMPLICIT_ATTACK",hz,0,60,102,0,1);
      audible(implicit,"P194 implicit SF2 default attack");
      ownerBank(resources,p.full);
      const auto explicitFull=render(factory,id,host,
          "P194_EXPLICIT_ATTACK",hz,0,60,102,0,1);
      audible(explicitFull,"P194 explicit SF2 standard attack");
      std::printf("P194_SF2_ATTACK_COMPARE type=%s hz=%d "
            "implicit=%016llX explicit=%016llX equal=%d\\n",
          p.label,hz,
          static_cast<unsigned long long>(implicit.hash),
          static_cast<unsigned long long>(explicitFull.hash),
          implicit.hash==explicitFull.hash?1:0);
      check(implicit.hash==explicitFull.hash,
            "P194 SF2 omitted vs explicit Creative attack default PCM differs");
    }
  }
  ownerBank(resources,nullptr);
  std::puts("P194_SF2_VOL_AND_MOD_ATTACK_DEFAULT_VST3_PCM_3_RATES_PASS");
}

// P195: genuine executable native VST3 audio must reproduce the *original*
// Creative Win95 SF2 descriptor addition. Baseline AWE32Emu treated both
// SF1 and SF2 as FillFrom; the original VXD's SF2 merge uses signed addition.
// Two public pairs have exactly equivalent additive pitch/attenuation.
void verifyP195CreativeSf2PresetAdditive(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources) {
  for(const char* kind:{"tune","fine","atten"}) {
    const std::string prefix=std::string("p195-")+kind;
    const auto split=prefix+"-split.sf2",combined=prefix+"-combined.sf2",
               contrast=prefix+"-contrast.sf2";
    check(fs::is_regular_file(resources/split) &&
          fs::is_regular_file(resources/combined) &&
          fs::is_regular_file(resources/contrast),
          "P195 original Win95 SF2 additive generator test fixtures missing");
    for(const int hz:{44100,48000,96000}) {
      ownerBank(resources,split.c_str());
      const auto p=render(factory,id,host,"P195_PRESET_PLUS_INSTRUMENT",
                           hz,0,60,102,0,1);
      audible(p,"P195 additive preset/instrument");
      ownerBank(resources,combined.c_str());
      const auto expected=render(factory,id,host,"P195_SAME_GENERATOR_IN_INSTRUMENT",
                           hz,0,60,102,0,1);
      audible(expected,"P195 equivalent combined instrument generator");
      std::printf("P195_ORIGINAL_CREATIVE_SF2_GENERATOR type=%s rate=%d "
          "presetPlus=%016llX instrumentSum=%016llX equal=%d\n",
          kind,hz,static_cast<unsigned long long>(p.hash),
          static_cast<unsigned long long>(expected.hash),
          p.hash==expected.hash?1:0);
      check(p.hash==expected.hash,
            "P195 SF2 additive generator has wrong effective pitch/attenuation");
      ownerBank(resources,contrast.c_str());
      const auto negative=render(factory,id,host,"P195_CHANGED_PARAMETER_NEGATIVE",
                                 hz,0,60,102,0,1);
      check(negative.hash!=expected.hash,
            "P195 oracle did not discriminate a genuinely different SF2 generator");
    }
  }
  ownerBank(resources,nullptr);
  std::puts("P195_SF2_CREATIVE_PRESET_INSTRUMENT_LINEAR_3GEN_3_RATES_WITH_NEGATIVES_PASS");
}

// P196: actual Win64 VST3 endAddrsOffset/coarse offset vs original
// Creative SBAWE.VXD LCOD+0x49EE..0x4A02. Test both equivalence and
// adversarial negative inputs, including note-off with a LONG release.
// No private Creative assets in public fixtures.
void verifyP196CreativeSf2EndOffsets(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources) {
    struct Case {const char* file; const char* label;};
    const std::array<Case,5> cases{{
        {"p196-omitted.sf2","P196_END_OMITTED"},
        {"p196-explicit-zero.sf2","P196_END_ZERO"},
        {"p196-fine-minus30000.sf2","P196_END_FINE_NEG30000"},
        {"p196-coarse-minus1.sf2","P196_END_COARSE_NEG1"},
        {"p196-fine-minus32768.sf2","P196_END_FINE_NEG32768"}
    }};
    for(const auto& c:cases)
        check(fs::is_regular_file(resources/c.file),
              "P196 SF2 sample-end test fixture missing");
    for(int hz:{44100,48000,96000}) {
        std::array<Stats,5> v{};
        for(std::size_t i=0;i<cases.size();++i) {
            ownerBank(resources,cases[i].file);
            v[i]=render(factory,id,host,cases[i].label,hz,0,
                        72,102,0,1);
            audible(v[i],cases[i].label);
        }
        check(v[0].hash==v[1].hash,
              "P196 omitted versus zero end offsets changed original PCM");
        check(v[3].hash==v[4].hash,
              "P196 coarse -1 does not equal fine -32768 sample words");
        check(v[2].hash!=v[0].hash,
              "P196 fine endAddrsOffset ignored; end moved 30000 samples");
        check(v[3].hash!=v[0].hash,
              "P196 coarse endAddrsCoarseOffset ignored; end moved 32768 samples");
        std::printf("P196_CREATIVE_SF2_SAMPLE_END_COMPARE rate=%d "
                    "unchanged=%016llX fine=%016llX coarse=%016llX\\n",
              hz,static_cast<unsigned long long>(v[0].hash),
              static_cast<unsigned long long>(v[2].hash),
              static_cast<unsigned long long>(v[3].hash));
    }
    ownerBank(resources,nullptr);
    std::puts("P196_CREATIVE_SF2_GEN1_GEN12_AUDIO_3RATES_POSITIVE_NEGATIVE_PASS");
}

// P181 owner-private release gate. Invoked ONLY by --private against the
// ACTUAL compiled Win64 VST3 with the owner's licensed ROM/SBK/SF2 assets.
// All effects, polyphony and sample rate conversion are the original core;
// never replace it with a synthetic SF2 oracle or upload real owner media.
void verifyP181PrivateMultirateOriginalChipMatrix(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  unsigned matrix=0,clips=0,rateTransitions=0,alternateNotes=0;
  const auto originalRom=resources/"awe32.raw";
  check(fs::file_size(originalRom)==1048576u,
        "P181 authentic owner 1-MiB Creative ROM is missing");
  for(const int rate:{44100,48000,96000}){
    ownerBank(resources,"77-presets.sf2");
    for(const int program:{7,33,54,77}){
      std::array<std::uint64_t,3> velocityHashes{};
      int idx=0;
      for(const int velocity:{40,80,100}){
        const auto result=render(factory,id,host,
          "P181_OWNER_MULTIRATE_MELODIC",rate,0,60,velocity,program,1);
        audible(result,"P181 owner SF2 velocity mapped original 86Box chip");
        velocityHashes[idx++]=result.hash;
        clips+=result.clippedSamples;
        ++matrix;
      }
      check(velocityHashes[0]!=velocityHashes[1] &&
            velocityHashes[1]!=velocityHashes[2],
            "P181 owner real SF2 velocity 40/80/100 failed to change chip PCM");
    }
    for(const int note:{48,72}){
      const auto result=render(factory,id,host,
          "P181_OWNER_KEY_RANGES",rate,0,note,100,7,1);
      audible(result,"P181 owner real SF2 alternate note pitch/range");
      ++alternateNotes;
    }
    const auto p62=render(factory,id,host,
         "P181_OWNER_PRESET62",rate,0,60,100,62,1);
    const auto p64=render(factory,id,host,
         "P181_OWNER_PRESET64",rate,0,60,100,64,1);
    audible(p62,"P181 real SF2 MIDI program 62");
    audible(p64,"P181 real SF2 MIDI program 64");
    check(p62.hash!=p64.hash,
          "P181 real owner SoundFont program gap shifted 62 vs 64");
    const auto missing63=render(factory,id,host,
         "P181_OWNER_HOLE63",rate,0,60,100,63,1);
    const auto rom63=render(factory,id,host,
         "P181_ROM_FALLBACK63",rate,0,60,100,63,0);
    check(missing63.hash==rom63.hash,
          "P181 missing real owner preset63 did not map to authentic ROM");
    ownerBank(resources,"vintage.sf2");
    const auto vintage=render(factory,id,host,
         "P181_OWNER_VINTAGE",rate,0,60,100,7,1);
    audible(vintage,"P181 private Vintage Dreams original chip PCM");
    ownerBank(resources,"77-presets.sf2");
    const auto returned=render(factory,id,host,
         "P181_OWNER_RESTORED",rate,0,60,100,7,1);
    audible(returned,"P181 owner original sound after A-B-A");
    const auto direct=render(factory,id,host,
         "P181_OWNER_DIRECT",rate,0,60,100,7,1);
    check(direct.hash==returned.hash && vintage.hash!=returned.hash,
          "P181 original SF2 A-B-A did not recover exact chip audio");
    ownerBank(resources,nullptr);
    const auto kick=render(factory,id,host,
         "P181_OWNER_ROM_DRUM_AFTER_SF2",rate,9,36,100,0,0);
    const auto melodic=render(factory,id,host,
         "P181_OWNER_ROM_MELODY_AFTER_SF2",rate,0,60,100,7,0);
    audible(kick,"P181 authentic original ROM kick after bank swap");
    audible(melodic,"P181 original ROM patch after bank swap");
    ++rateTransitions;
    std::printf("P181_OWNER_RATE_PASS hz=%d bank_7=%016llX vintage=%016llX"
                " rom_kick=%016llX\n",
       rate,static_cast<unsigned long long>(returned.hash),
       static_cast<unsigned long long>(vintage.hash),
       static_cast<unsigned long long>(kick.hash));
  }
  check(matrix==36u && alternateNotes==6u && rateTransitions==3u,
        "P181 private multi-rate test skipped original-media matrix cases");
  std::printf("P181_PRIVATE_ORIGINAL_WINDOWS_VST3_MATRIX"
              " velocity_renders=%u alternate_notes=%u rates=%u"
              " PCM16_extrema=%u\n",
       matrix,alternateNotes,rateTransitions,clips);
  std::puts("P181_PRIVATE_EXACT_WIN64_VST3_ORIGINAL_ROM_SBK_SF2"
            "_THREE_RATES_VELOCITY_KEY_RANGES_SPARSE_RECALL_PASS");
}

// Genuine P171 packaged VST3 regression: a valid RIFF SF2 with >28 MiB
// sample PCM must fail where pre-P171 silently truncated the upload.
void verifyP171RamOverflow(const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  check(fs::is_regular_file(resources/"oversized.sf2") &&
        fs::file_size(resources/"oversized.sf2")>28u*1024u*1024u,
        "P171 oversized SF2 fixture missing");
  ownerBank(resources,"vintage.sf2");
  const auto prior=render(factory,id,host,"P171_NORMAL_BEFORE",
                          44100,0,60,100,0,1);
  audible(prior,"P171 normal SoundFont before rejection");
  ownerBank(resources,"oversized.sf2");
  const auto refused=render(factory,id,host,"P171_TOO_BIG",
                            44100,0,60,100,0,1);
  check(refused.rms==0.&&refused.peak==0.,
        "P171 overlimit SF2 silently uploaded truncated samples");
  ownerBank(resources,"vintage.sf2");
  const auto restored=render(factory,id,host,"P171_NORMAL_AFTER",
                             44100,0,60,100,0,1);
  audible(restored,"P171 valid SoundFont recovered");
  check(prior.hash==restored.hash,
        "P171 oversized SF2 mutated subsequent original chip PCM");
  ownerBank(resources,nullptr);
  std::puts("P171_28MIB_HARDWARE_RAM_REJECTION_AND_RECOVERY_PASS");
}

// P185 / independent Codex NO-GO regressions. These tests exercise the
// actual compiled Steinberg VST3, not a fake GUI or a standalone DSP clone.
void verifyP185OriginalModeWithNonzeroSavedSend(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host){
  Plugin first(factory,id,host);
  check(first.controller->setParamNormalized(1110u,.625)==kResultOk &&
        first.controller->setParamNormalized(1420u,0.)==kResultOk,
        "P185 cannot set SEND>0 then explicit ORIGINAL mode");
  MemoryStream stored;
  check(first.component->getState(&stored)==kResultOk &&
        stored.getSize()==648u,"P185 ORIGINAL mode V5 not serialized");
  const auto* raw=reinterpret_cast<const std::uint8_t*>(stored.getData());
  check(raw && raw[608]==0x6A && raw[609]==0x18 &&
        raw[644]==4u && raw[645]==2u,
        "P185 saved ORIGINAL send mode conflated with positive SEND knob");
  Plugin recalled(factory,id,host);
  MemoryStream source;
  int32 written=0;
  check(source.write(stored.getData(),648,&written)==kResultOk &&
        written==648 && source.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
        recalled.component->setState(&source)==kResultOk,
        "P185 ORIGINAL positive-SEND project could not be recalled");
  MemoryStream again;
  check(recalled.component->getState(&again)==kResultOk &&
        again.getSize()==648u &&
        std::memcmp(again.getData(),stored.getData(),648u)==0 &&
        recalled.controller->getParamNormalized(1420u)==0.,
        "P185 recalled ORIGINAL silently became MANUAL");
  // An older V5 file lacks the new 0x0200 explicit-mode marker. It must
  // still use the documented legacy fallback instead of losing its SEND.
  std::vector<std::uint8_t> legacy(raw,raw+648u);
  legacy[645]=0u; // legacy mode-unmarked V5, positive control value
  MemoryStream oldStream;
  check(oldStream.write(legacy.data(),648,&written)==kResultOk &&
        oldStream.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
        recalled.component->setState(&oldStream)==kResultOk &&
        recalled.controller->getParamNormalized(1420u)==1.,
        "P185 old V5 positive SEND no longer recalls its legacy MANUAL mode");
  std::puts("P185_V5_POSITIVE_SEND_EXPLICIT_ORIGINAL_AND_LEGACY_COMPAT_PASS");
}

void verifyP185PitchRangeSurvivesNRPN(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"vintage.sf2");
  Saved rpn{};rpn.version=4u;rpn.bank=1u;
  rpn.midiStates.fill(0xFFFFu);
  rpn.advancedStates.fill(0xFFFFu);
  rpn.midiStates[7]=16383u; // actual CH1 pitch bend +8191
  rpn.advancedStates[2]=0u;rpn.advancedStates[3]=0u;
  rpn.advancedStates[4]=12u;rpn.parameterModes[0]=1u;
  Saved nrpn=rpn;
  nrpn.advancedStates[6]=1u;nrpn.advancedStates[7]=2u;
  nrpn.parameterModes[0]=2u; // most recently selected NRPN
  Saved defaultRange=nrpn;
  defaultRange.advancedStates[4]=0xFFFFu; // no retained RPN range
  Plugin a(factory,id,host),b(factory,id,host),control(factory,id,host);
  a.start(0,1,44100.);b.start(0,1,44100.);
  control.start(0,1,44100.);
  p015Restore(a,rpn);
  p015Restore(b,nrpn);
  p015Restore(control,defaultRange);
  const auto withRpn=p015Play(a,0,60);
  const auto afterNrpn=p015Play(b,0,60);
  const auto defaultPcm=p015Play(control,0,60);
  check(withRpn!=defaultPcm,
        "P185 RPN pitch range oracle invalid: 12 and default sound equal");
  check(afterNrpn==withRpn,
        "P185 selecting NRPN lost the original 12-semitone bend range PCM");
  ownerBank(resources,nullptr);
  std::puts("P185_REAL_CHIP_RPN12_NRPN_RESELECT_V4_RECALL_PCM_PASS");
}

void verifyP185StoppedROMBankRestoresAgainstOldCC0(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"77-presets.sf2");
  Saved state{};state.version=4u;state.bank=0u;state.program=0u;
  state.midiStates.fill(0xFFFFu);
  state.advancedStates.fill(0xFFFFu);
  state.advancedStates[0]=1u; // earlier MIDI CC0 selected SF2
  Plugin restored(factory,id,host);
  restored.start(0,1,44100.);
  check(restored.processor->setProcessing(false)==kResultOk &&
        restored.component->setActive(false)==kResultOk,
        "P185 stopped-transport host teardown failed");
  restored.processing=false;restored.active=false;
  p015Restore(restored,state); // while inactive; must defeat historic CC0
  check(restored.component->setActive(true)==kResultOk &&
        restored.processor->setProcessing(true)==kResultOk,
        "P185 stopped-transport host reactivation failed");
  restored.active=true;restored.processing=true;
  MemoryStream after;
  check(restored.component->getState(&after)==kResultOk &&
        after.getSize()==sizeof(Saved),"P185 stopped ROM bank state lost");
  Saved got{};std::memcpy(&got,after.getData(),sizeof(got));
  check(got.bank==0u && got.advancedStates[0]==1u,
        "P185 old CC0 overrode the saved ROM bank at activation");
  Plugin rom(factory,id,host),sf2(factory,id,host);
  rom.start(0,0,44100.);sf2.start(0,1,44100.);
  const auto want=p015Play(rom,0,60);
  const auto wrong=p015Play(sf2,0,60);
  check(want!=wrong,"P185 ROM vs SF2 PCM fixtures not independent");
  check(p015Play(restored,0,60)==want,
        "P185 STOP/START reloaded stale MIDI SF2 bank instead of ROM PCM");
  ownerBank(resources,nullptr);
  std::puts("P185_REAL_CHIP_STOPPED_ROM_BANK_STALE_CC0_RECALL_PCM_PASS");
}

void verifyP185BrowserDrumKitActuallyChangesPCM(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"multi-drums.sf2");
  // Crucial independent negative oracle: all 16 channels START on Kit 0.
  // Plugin::start(5,2) would set CH10's program to 5 as well and falsely
  // pass even if the CH1 browser never programs its audible CH10 route.
  const auto selectBrowserKit=[&](Plugin& plugin,int kit){
    ParameterChanges changes(1);
    int32 qi=0,pi=0;
    auto* q=changes.addParameterData(1000u,qi);
    check(q && q->addPoint(0,double(kit)/127.,pi)==kResultOk,
          "P185 browser program parameter not queued");
    ProcessData flush{};
    flush.processMode=kRealtime;flush.symbolicSampleSize=kSample32;
    flush.numSamples=0;flush.inputParameterChanges=&changes;
    check(plugin.processor->process(flush)==kResultOk,
          "P185 CH1 browser program change rejected at stopped transport");
  };
  Plugin kit0Unit(factory,id,host),kit5Unit(factory,id,host);
  kit0Unit.start(0,2,44100.);
  kit5Unit.start(0,2,44100.);
  selectBrowserKit(kit0Unit,0);
  selectBrowserKit(kit5Unit,5);
  const auto kit0=p015Play(kit0Unit,0,60);
  const auto kit5=p015Play(kit5Unit,0,60);
  check(kit0!=kit5,
        "P185 CH1 browser Kit 5 still sounds exactly like Kit 0");
  // Independent ground truth: a direct CH10 patch change, with CH1 not
  // routed, selects the exact same Creative instrument and PCM.
  Plugin ch10Kit0(factory,id,host),ch10Kit5(factory,id,host);
  ch10Kit0.start(0,0,44100.);
  ch10Kit5.start(5,0,44100.);
  const auto hwKit0=p015Play(ch10Kit0,9,60);
  const auto hwKit5=p015Play(ch10Kit5,9,60);
  check(hwKit0!=hwKit5,
        "P185 independent CH10 Kit 0/5 instruments were not distinguishable");
  check(kit0==hwKit0 && kit5==hwKit5,
        "P185 browser CH1 program must control actual sounding CH10 kit");
  // NEW interaction challenge: the dedicated real CH10 lane must return
  // to its independently stored Kit 0 when CH1 exits the browser drum bank.
  // CH10 played Kit 5 on the old adapter even after CH1 left drums.
  Plugin leavingDrums(factory,id,host);
  leavingDrums.start(0,2,44100.);
  selectBrowserKit(leavingDrums,5);
  ParameterChanges leave(1);
  int32 bankQueue=0,bankPoint=0;
  auto* bank=leave.addParameterData(1002u,bankQueue);
  check(bank && bank->addPoint(0,0.,bankPoint)==kResultOk,
        "P185 cannot leave CH1 drum bank via real VST3 automation");
  ProcessData leaveFlush{};
  leaveFlush.processMode=kRealtime;
  leaveFlush.symbolicSampleSize=kSample32;
  leaveFlush.numSamples=0;
  leaveFlush.inputParameterChanges=&leave;
  check(leavingDrums.processor->process(leaveFlush)==kResultOk,
        "P185 failed to leave CH1 drum bank");
  check(p015Play(leavingDrums,9,60)==hwKit0,
        "P185 leaving CH1 drums failed to restore independent CH10 Kit 0");
  Plugin recalled(factory,id,host);
  recalled.start(0,0,44100.);
  Saved state{};state.version=4u;state.program=5u;state.bank=2u;
  state.midiStates.fill(0xFFFFu);
  state.advancedStates.fill(0xFFFFu);
  // Every OTHER MIDI channel remains at program zero: a realistic fresh
  // project rather than the accidental all-channel Program 5 fixture.
  p015Restore(recalled,state);
  check(p015Play(recalled,0,60)==kit5,
        "P185 recalled CH1 browser Kit 5 not reproduced on CH10 hardware");
  ownerBank(resources,nullptr);
  std::puts("P185_ORIGINAL_CHIP_BROWSER_KIT0_KIT5_CH10_PCM_V4_RECALL_PASS");
}

void verifyP185CoherentGUIAndMidiAutomationSaves(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host){
  Plugin plugin(factory,id,host);plugin.start(0,0,44100.);
  std::atomic<bool> stop{false},worked{true};
  std::atomic<uint64_t> saved{0},torn{0},rejected{0},blocks{0};
  // Each process() atomically changes a complete MIDI/GUI/FX tuple.
  // An independently timed host getState must see exactly one generation.
  const auto apply=[&](bool b)->bool{
    ParameterChanges points(7);
    const auto put=[&](ParamID id,double value)->bool{
      int32 qi=0,pi=0;
      auto* q=points.addParameterData(id,qi);
      return q && q->addPoint(0,value,pi)==kResultOk;
    };
    if(!put(1000u,b?20./127.:10./127.)||
       !put(1002u,b?.5:0.)||
       !put(1001u,b?.875:.375)||
       !put(1100u,b?.75:.25)||
       !put(1400u,b?6./7.:5./7.)||
       !put(1420u,b?0.:1.))return false;
    ProcessData pd{};pd.processMode=kRealtime;
    pd.symbolicSampleSize=kSample32;pd.numSamples=0;
    pd.inputParameterChanges=&points;
    return plugin.processor->process(pd)==kResultOk;
  };
  const auto save=[&](){
    MemoryStream state;
    check(plugin.component->getState(&state)==kResultOk,
          "P185 cannot produce an actual valid VST3 state");
    const auto* ptr=reinterpret_cast<const std::uint8_t*>(state.getData());
    check(ptr && state.getSize()==648u,"P185 expected genuine V5 automation state");
    return std::vector<std::uint8_t>(ptr,ptr+state.getSize());
  };
  // P186 adversarial seed: prior stopped controller values disagree with
  // every later audio automation point. They MUST be consumed by the audio
  // callback, not persist forever as stale getState overlays.
  check(plugin.controller->setParamNormalized(1000u,3./127.)==kResultOk &&
        plugin.controller->setParamNormalized(1002u,1.)==kResultOk &&
        plugin.controller->setParamNormalized(1001u,.125)==kResultOk &&
        plugin.controller->setParamNormalized(1100u,.10)==kResultOk &&
        plugin.controller->setParamNormalized(1400u,1./7.)==kResultOk &&
        plugin.controller->setParamNormalized(1420u,0.)==kResultOk,
        "P186 seed contradictory pre-automation GUI edits");
  check(apply(false),"P185 initial automation state A rejected");
  const auto A=save();
  check(apply(true),"P185 initial automation state B rejected");
  const auto B=save();
  check(A!=B && A[645]==1u && B[645]==0u,
        "P185 independent A/B FX mode and send-mode oracle was not exercised");
  check(apply(false),"P185 restore automation state A rejected");
  std::thread audio([&](){
    for(int i=0;i<20000;++i){
      if(!apply((i&1)!=0)){
        worked.store(false,std::memory_order_release);break;
      }
      blocks.fetch_add(1,std::memory_order_relaxed);
      if((i&15)==0)std::this_thread::yield();
    }
    stop.store(true,std::memory_order_release);
  });
  std::thread snapshots([&](){
    while(!stop.load(std::memory_order_acquire)){
      MemoryStream ms;
      if(plugin.component->getState(&ms)!=kResultOk){
        rejected.fetch_add(1,std::memory_order_relaxed);
      }else{
        const auto* raw=reinterpret_cast<const std::uint8_t*>(ms.getData());
        const auto size=static_cast<std::size_t>(ms.getSize());
        bool matchA=size==A.size() && raw &&
            std::memcmp(raw,A.data(),size)==0;
        bool matchB=size==B.size() && raw &&
            std::memcmp(raw,B.data(),size)==0;
        if(!matchA&&!matchB)torn.fetch_add(1,std::memory_order_relaxed);
        saved.fetch_add(1,std::memory_order_relaxed);
      }
    }
  });
  audio.join();snapshots.join();
  std::printf("P185_CONCURRENT_AUDIO_GUI_FX_MIDI_AUTOMATION_SAVES"
              " successful=%llu mixed=%llu rejected=%llu callbacks=%llu\n",
       static_cast<unsigned long long>(saved.load()),
       static_cast<unsigned long long>(torn.load()),
       static_cast<unsigned long long>(rejected.load()),
       static_cast<unsigned long long>(blocks.load()));
  check(worked.load() && blocks.load()==20000u && saved.load()>=100u,
        "P185 independent audio/save concurrency oracle was not exercised");
  check(torn.load()==0u && rejected.load()==0u,
        "P185 host saved an impossible mixed MIDI/GUI/FX generation");
  std::puts("P185_REAL_WIN64_VST3_ATOMIC_GUI_FX_MIDI_AUTOMATION_SAVE_PASS");
}


// P186 new Codex NO-GO counters: effective pitch bend after RPN NULL and
// NRPN data-entry must retain the last *applied* 12-semitone range.
void verifyP186BendRangeRpnNullAndNrpnData(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"vintage.sf2");
  const auto capture=[&](bool nullRpn){
    Plugin unit(factory,id,host);unit.start(0,1,44100.);
    IMidiMapping* raw=nullptr;
    check(unit.controller->queryInterface(IMidiMapping::iid,
          reinterpret_cast<void**>(&raw))==kResultOk&&raw,
          "P186 actual VST3 IMidiMapping missing");
    const IPtr<IMidiMapping> mapping=owned(raw);
    const auto send=[&](CtrlNumber controller,int value){
      ParamID mapped=0;
      check(mapping->getMidiControllerAssignment(0,0,controller,mapped)==kResultTrue,
            "P186 RPN/NRPN mapping query failed");
      ParameterChanges changes(1);
      int32 q=0,p=0;
      auto* item=changes.addParameterData(mapped,q);
      check(item&&item->addPoint(0,double(value)/127.,p)==kResultOk,
            "P186 mapped CC point missing");
      ProcessData pd{};pd.processMode=kRealtime;
      pd.symbolicSampleSize=kSample32;pd.numSamples=0;
      pd.inputParameterChanges=&changes;
      check(unit.processor->process(pd)==kResultOk,
            "P186 source RPN/NRPN CC callback failed");
    };
    send(kCtrlRPNSelectMSB,0);
    send(kCtrlRPNSelectLSB,0);
    send(kCtrlDataEntryMSB,12);
    if(nullRpn){
      send(kCtrlRPNSelectMSB,127);
      send(kCtrlRPNSelectLSB,127);
    }else{
      send(kCtrlNRPNSelectMSB,1);
      send(kCtrlNRPNSelectLSB,2);
    }
    send(kCtrlDataEntryMSB,71); // must not overwrite the effective RPN12
    MemoryStream state;
    check(unit.component->getState(&state)==kResultOk &&
          state.getSize()==sizeof(Saved),
          "P186 RPN/NRPN state not V4 roundtrippable");
    Saved result{};std::memcpy(&result,state.getData(),sizeof(result));
    check(result.advancedStates[4]==12u &&
          result.parameterModes[0]==(nullRpn?1u:2u) &&
          result.advancedStates[2]==(nullRpn?127u:0u) &&
          result.advancedStates[3]==(nullRpn?127u:0u),
          "P186 unsupported Data Entry erased effective bend-range memory");
    result.midiStates[7]=16383u; // full positive pitch to hear range
    return result;
  };
  const auto nullSaved=capture(true);
  const auto nrpnSaved=capture(false);
  auto reference=nullSaved;
  reference.advancedStates[2]=0u;
  reference.advancedStates[3]=0u;
  auto standard=reference;
  standard.advancedStates[4]=0xFFFFu;
  Plugin a(factory,id,host),b(factory,id,host),c(factory,id,host),d(factory,id,host);
  a.start(0,1,44100.);b.start(0,1,44100.);
  c.start(0,1,44100.);d.start(0,1,44100.);
  p015Restore(a,reference);
  p015Restore(b,nullSaved);
  p015Restore(c,nrpnSaved);
  p015Restore(d,standard);
  const auto expected=p015Play(a,0,60);
  check(expected!=p015Play(d,0,60),
        "P186 bend12 vs default2 negative PCM oracle invalid");
  check(p015Play(b,0,60)==expected && p015Play(c,0,60)==expected,
        "P186 RPN NULL or NRPN Data Entry changed original chip bend PCM");
  ownerBank(resources,nullptr);
  std::puts("P186_ORIGINAL_CHIP_RPN_NULL_NRPN_DATA_ENTRY_BEND12_RECALL_PCM_PASS");
}

void verifyP186CH10ProgramAndCC0DrumRouting(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"multi-drums.sf2");
  Plugin browsed(factory,id,host),direct(factory,id,host),
         bankExit(factory,id,host);
  browsed.start(0,2,44100.);
  direct.start(0,2,44100.);
  bankExit.start(0,2,44100.);
  const auto flush=[&](Plugin& unit,ParamID param,double value){
    ParameterChanges changes(1);
    int32 qi=0,pi=0;
    auto* q=changes.addParameterData(param,qi);
    check(q&&q->addPoint(0,value,pi)==kResultOk,
          "P186 mapped drum bank/program point missing");
    ProcessData data{};data.processMode=kRealtime;
    data.symbolicSampleSize=kSample32;data.numSamples=0;
    data.inputParameterChanges=&changes;
    check(unit.processor->process(data)==kResultOk,
          "P186 CH1/CH10 drum program callback rejected");
  };
  for(Plugin* unit:{&browsed,&direct,&bankExit}){
      flush(*unit,1000u,5./127.); // CH1 browser Kit 5
      flush(*unit,1209u,0.);      // native CH10 kit0
  }
  Plugin kit5(factory,id,host),kit0(factory,id,host);
  kit5.start(5,2,44100.);kit0.start(0,0,44100.);
  const auto five=p015Play(kit5,0,60);
  const auto zero=p015Play(kit0,9,60);
  check(five!=zero,"P186 independent Kit 0/5 waveform oracle invalid");
  check(p015Play(browsed,0,60)==five,
        "P186 CH10 program change stole CH1 browser Drum Kit 5");
  check(p015Play(direct,9,60)==zero,
        "P186 CH10 cannot play its own Kit 0 after CH1 browser Kit 5");
  // Use an actual raw MIDI CC0 on CH1 (mapped VST3 controller), not the
  // host Bank parameter, to exit Drum Kit mode after browser selection.
  IMidiMapping* raw=nullptr;
  check(browsed.controller->queryInterface(IMidiMapping::iid,
        reinterpret_cast<void**>(&raw))==kResultOk&&raw,
        "P186 real mapped CC0 unavailable");
  const IPtr<IMidiMapping> mapping=owned(raw);
  ParamID cc0=0;check(mapping->getMidiControllerAssignment(
        0,0,kCtrlBankSelectMSB,cc0)==kResultTrue,
        "P186 mapping of raw MIDI CH1 CC0 failed");
  flush(bankExit,cc0,0.); // explicit ROM bank 0
  check(p015Play(bankExit,9,60)==zero,
        "P186 CH1 raw CC0 left the wrong CH10 kit active");
  ownerBank(resources,nullptr);
  std::puts("P186_CH1_CC0_CH10_PROGRAM_ISOLATED_REAL_CHIP_DRUM_PCM_PASS");
}

// Same pitched notes with distinct NoteIDs on independent Win95 lanes.
// Releasing the browser note after its bank switch must release that note
// without waiting for, or cutting off, the independently held native CH10.
void verifyP186TwoNoteIDsAfterBankSwitch(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"multi-drums-sustain.sf2");
  const auto renderScenario=[&](bool releaseNotes,bool audition=true){
    Plugin unit(factory,id,host);unit.start(0,2,44100.);
    std::array<float,1024> left{},right{};
    float* channels[2]={left.data(),right.data()};
    AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=channels;
    EventList events;
    ParameterChanges changes(2);
    ProcessData data{};data.processMode=kRealtime;
    data.symbolicSampleSize=kSample32;data.numSamples=1024;
    data.numOutputs=1;data.outputs=&out;
    data.inputEvents=&events;data.inputParameterChanges=&changes;
    std::vector<float> firstReleaseWindow,tail;
    for(int block=0;block<85;++block){
      events.clear();changes.clearQueue();
      if(block==0){
        Event first{};first.type=Event::kNoteOnEvent;
        first.sampleOffset=10;first.noteOn.channel=0;
        first.noteOn.pitch=60;first.noteOn.velocity=1.;
        first.noteOn.noteId=101;
        Event second=first;second.sampleOffset=12;
        second.noteOn.channel=9;second.noteOn.noteId=202;
        check((!audition || events.addEvent(first)==kResultOk)&&
              events.addEvent(second)==kResultOk,
              "P186 two distinct NoteIDs not submitted");
      }
      if(block==3){
        int32 qi=0,pi=0;
        auto* bank=changes.addParameterData(1002u,qi);
        check(bank&&bank->addPoint(0,0.,pi)==kResultOk,
              "P186 route change between NoteOn and NoteOff missing");
      }
      if(releaseNotes && ((block==5&&audition)||block==50)){
        Event off{};off.type=Event::kNoteOffEvent;
        off.sampleOffset=15;off.noteOff.pitch=60;
        off.noteOff.channel=block==5?0:9;
        off.noteOff.noteId=block==5?101:202;
        check(events.addEvent(off)==kResultOk,
              "P186 two independent NoteOff IDs missing");
      }
      check(unit.processor->process(data)==kResultOk,
            "P186 NoteID routed original-chip PCM callback rejected");
      if(block>=38)
        for(int i=0;i<1024;++i){
          check(std::isfinite(left[i])&&std::isfinite(right[i]),
                "P186 NoteID route nonfinite PCM");
          if(block<49){
            firstReleaseWindow.push_back(left[i]);
            firstReleaseWindow.push_back(right[i]);
          }else if(block>=70){
            tail.push_back(left[i]);tail.push_back(right[i]);
          }
        }
    }
    return std::make_pair(std::move(firstReleaseWindow),std::move(tail));
  };
  const auto released=renderScenario(true);
  const auto held=renderScenario(false);
  const auto nativeOnly=renderScenario(true,false);
  const auto energy=[&](const std::vector<float>& pcm){
    double sum=0.;for(float x:pcm)sum+=double(x)*x;
    return sum;
  };
  check(energy(nativeOnly.first)>0.001 && released.first==nativeOnly.first &&
        released.first!=held.first,
        "P189 the first NoteID must release only the audition voice");
  check(energy(held.second)>0.001 &&
        energy(released.second)<energy(held.second)*.90,
        "P186 overlapping NoteIDs remained hanging after last physical release");
  ownerBank(resources,nullptr);
  std::puts("P186_TWO_DISTINCT_NOTEIDS_BANK_SWITCH_NO_STUCK_CH10_VOICES_PASS");
}

void verifyP186ExplicitSF2SHAActivationRejectsChangedFile(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  const auto testPath=resources/"p186-owner-sha-activation.sf2";
  fs::copy_file(resources/"vintage.sf2",testPath,
                fs::copy_options::overwrite_existing);
  try{
    const auto good=p174V5State(testPath);
    Plugin unit(factory,id,host);
    check(p174ApplyV5(unit,good)==kResultOk,
          "P186 V5 external owner source was rejected before activation");
    // Swap with a *different but valid* bank after SHA validation and
    // before setActive. Old code loaded this new SF2 and saved its new SHA.
    fs::copy_file(resources/"77-presets.sf2",testPath,
                  fs::copy_options::overwrite_existing);
    ProcessSetup setup{};setup.processMode=kRealtime;
    setup.symbolicSampleSize=kSample32;setup.sampleRate=44100.;
    setup.maxSamplesPerBlock=1024;
    check(unit.processor->setupProcessing(setup)==kResultOk,
          "P186 setup native original chip processor");
    check(unit.component->setActive(true)!=kResultOk,
          "P186 altered owner SF2 illegally activated with untrusted PCM");
    MemoryStream saved;
    check(unit.component->getState(&saved)==kResultOk &&
          saved.getSize()==good.size() &&
          std::memcmp(saved.getData(),good.data(),good.size())==0,
          "P186 failed asset activation silently rewrote the saved owner SHA");
  }catch(...){fs::remove(testPath);throw;}
  fs::remove(testPath);
  std::puts("P186_PINNED_OWNER_SF2_SHA_ACTIVATION_TAMPER_FAIL_CLOSED_PASS");
}

// Prior editor-only values must not survive as a stale V5 SEND/Cutoff
// override when newer genuine VST3 audio parameter points were processed.
void verifyP186CutoffAndSendRestoreActualPCM(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"expressive.sf2");
  Plugin unit(factory,id,host);unit.start(0,1,44100.);
  check(unit.controller->setParamNormalized(1108u,.20)==kResultOk &&
        unit.controller->setParamNormalized(1110u,.65)==kResultOk &&
        unit.controller->setParamNormalized(1420u,0.)==kResultOk,
        "P186 could not seed cutoff and explicit ORIGINAL mode");
  ParameterChanges values(2);
  int32 qi=0,pi=0;
  auto* cutoff=values.addParameterData(1108u,qi);
  check(cutoff&&cutoff->addPoint(0,.80,pi)==kResultOk,
        "P186 host cutoff override missing");
  auto* send=values.addParameterData(1110u,qi);
  check(send&&send->addPoint(0,.40,pi)==kResultOk,
        "P186 delayed SEND knob automation point missing");
  ProcessData pd{};pd.processMode=kRealtime;
  pd.symbolicSampleSize=kSample32;pd.numSamples=0;
  pd.inputParameterChanges=&values;
  check(unit.processor->process(pd)==kResultOk,
        "P186 filter/send audio parameter flush failed");
  MemoryStream snap;
  check(unit.component->getState(&snap)==kResultOk &&
        snap.getSize()==648u,
        "P186 no project V5 after filter/send automation");
  const auto* bytes=reinterpret_cast<const std::uint8_t*>(snap.getData());
  auto read16=[&](unsigned at)->unsigned {
      return unsigned(bytes[at]) | (unsigned(bytes[at+1u])<<8u);
  };
  check(bytes && read16(588u+8u*2u)==8000u &&
        read16(588u+10u*2u)==4000u &&
        read16(644u)==0x0204u &&
        unit.controller->getParamNormalized(1420u)==0.,
        "P186 saved older Cutoff or wrongly switched explicit ORIGINAL mode");
  // Original send must have NO extra manual hardware SEND. On an original
  // wet expressive bank it must reproduce exact dry/default chip PCM when
  // cutoff is made the same; the active and reopened same-V5 PCM must match.
  Plugin fresh(factory,id,host);fresh.start(0,1,44100.);
  MemoryStream restore;int32 written=0;
  check(restore.write(snap.getData(),648,&written)==kResultOk &&
        written==648 &&
        restore.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk &&
        fresh.component->setState(&restore)==kResultOk,
        "P186 V5 cutoff/send project restore rejected");
  const auto active=p015Play(unit,0,60);
  const auto restarted=p015Play(fresh,0,60);
  check(active==restarted,
        "P186 live cutoff/SEND original-chip PCM differs after V5 reopening");
  ownerBank(resources,nullptr);
  std::puts("P186_SF2_FILTER_CUTOFF_EXPLICIT_ORIGINAL_SEND_STATE_PCM_RECALL_PASS");
}

// P186 independent Codex live-audio save challenge: do not infer project
// freshness from a zero-frame test. Keep a real original-chip looping voice
// sounding while the entire MIDI/GUI/FX tuple alternates under host points.
void verifyP186AudibleAtomicStateAndFreshness(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"voice32.sf2");
  Plugin unit(factory,id,host);unit.start(0,1,44100.);
  // Host-side edits deliberately contradict upcoming audio-point values.
  check(unit.controller->setParamNormalized(1001u,.12)==kResultOk &&
        unit.controller->setParamNormalized(1108u,.11)==kResultOk &&
        unit.controller->setParamNormalized(1110u,.10)==kResultOk &&
        unit.controller->setParamNormalized(1400u,1./7.)==kResultOk &&
        unit.controller->setParamNormalized(1420u,0.)==kResultOk,
        "P186 could not seed old GUI Cutoff/SEND/FX state");
  const auto processState=[&](bool b,bool startNote)->double {
    ParameterChanges changes(8);
    const auto add=[&](ParamID id,double v){
      int32 qi=0,pi=0;
      auto* q=changes.addParameterData(id,qi);
      check(q && q->addPoint(0,v,pi)==kResultOk,
            "P186 genuine VST3 audio automation point missing");
    };
    add(1000u,b?8./127.:7./127.);
    add(1001u,b?.875:.375);
    add(1108u,b?.75:.25); // ACTUAL filter cutoff, not AMP Attack
    add(1110u,b?.60:.20);
    add(1400u,b?6./7.:5./7.);
    add(1420u,b?0.:1.);
    std::array<float,128> l{},r{};
    float* ch[2]={l.data(),r.data()};
    AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=ch;
    EventList note;
    if(startNote){
      Event event{};event.type=Event::kNoteOnEvent;
      event.sampleOffset=12;event.noteOn.channel=0;
      event.noteOn.pitch=60;event.noteOn.velocity=.95f;
      event.noteOn.noteId=900;
      check(note.addEvent(event)==kResultOk,
            "P186 sustained source voice NoteOn rejected");
    }
    ProcessData pd{};pd.processMode=kRealtime;
    pd.symbolicSampleSize=kSample32;pd.numSamples=128;
    pd.numOutputs=1;pd.outputs=&bus;
    pd.inputParameterChanges=&changes;pd.inputEvents=&note;
    check(unit.processor->process(pd)==kResultOk,
          "P186 real sounding callback failed");
    double energy=0.;
    for(int i=0;i<128;++i){
      check(std::isfinite(l[i])&&std::isfinite(r[i]),
            "P186 nonfinite original-chip PCM");
      energy+=double(l[i])*l[i]+double(r[i])*r[i];
    }
    return energy;
  };
  const auto save=[&](){
    MemoryStream memory;
    check(unit.component->getState(&memory)==kResultOk,
          "P186 real audible getState rejected");
    const auto size=static_cast<std::size_t>(memory.getSize());
    const auto* raw=reinterpret_cast<const std::uint8_t*>(memory.getData());
    check(raw && size==648u,"P186 audible full GUI V5 state missing");
    return std::vector<std::uint8_t>(raw,raw+size);
  };
  double initialEnergy=processState(false,true);
  const auto A=save();
  initialEnergy+=processState(true,false);
  const auto B=save();
  check(A!=B && A[644]==5u && A[645]==1u &&
                 B[644]==6u && B[645]==2u,
        "P186 independent audible A/B ORIGINAL vs MANUAL V5 oracle invalid");
  initialEnergy+=processState(false,false);
  std::atomic<int> published{-1},ack{-1};
  std::atomic<bool> done{false},healthy{true};
  std::atomic<unsigned> stale{0},saveFailures{0};
  double handshakeEnergy=0.;
  constexpr int exchanges=256;
  std::thread audio([&](){
    try{
      for(int i=0;i<exchanges;++i){
        handshakeEnergy+=processState((i&1)!=0,false);
        published.store(i,std::memory_order_release);
        while(ack.load(std::memory_order_acquire)<i && healthy.load())
          std::this_thread::yield();
        if(!healthy.load())break;
      }
    }catch(...){healthy.store(false,std::memory_order_release);}
    done.store(true,std::memory_order_release);
  });
  std::thread observer([&](){
    for(int i=0;i<exchanges;++i){
      while(published.load(std::memory_order_acquire)<i && !done.load())
        std::this_thread::yield();
      if(published.load(std::memory_order_acquire)<i)break;
      MemoryStream memory;
      if(unit.component->getState(&memory)!=kResultOk){
        saveFailures.fetch_add(1u);
      }else{
        const auto& expected=(i&1)?B:A;
        if(static_cast<std::size_t>(memory.getSize())!=expected.size() ||
           !memory.getData() ||
           std::memcmp(memory.getData(),expected.data(),expected.size())!=0)
          stale.fetch_add(1u);
      }
      ack.store(i,std::memory_order_release);
    }
  });
  audio.join();observer.join();
  check(healthy.load() && ack.load()==exchanges-1 &&
        stale.load()==0u && saveFailures.load()==0u &&
        initialEnergy+handshakeEnergy>0.001,
        "P186 a settled sounding callback produced stale/mixed host state");
  // Now REMOVE the barrier: getState can race with an actual in-flight
  // original-chip render callback and must return only an entire A or B.
  std::atomic<bool> stop{false};
  std::atomic<unsigned> mixed{0},rejected{0},accepted{0};
  double concurrentEnergy=0.;
  std::thread realAudio([&](){
    try{
      for(int i=0;i<2500;++i)
        concurrentEnergy+=processState((i&1)!=0,false);
    }catch(...){healthy.store(false,std::memory_order_release);}
    stop.store(true,std::memory_order_release);
  });
  std::thread saver([&](){
    while(!stop.load(std::memory_order_acquire)){
      MemoryStream memory;
      if(unit.component->getState(&memory)!=kResultOk){
        rejected.fetch_add(1u);
      }else{
        const auto sz=static_cast<std::size_t>(memory.getSize());
        const auto* raw=reinterpret_cast<const std::uint8_t*>(memory.getData());
        const bool matchA=raw && sz==A.size() &&
             std::memcmp(raw,A.data(),sz)==0;
        const bool matchB=raw && sz==B.size() &&
             std::memcmp(raw,B.data(),sz)==0;
        if(!matchA&&!matchB)mixed.fetch_add(1u);
        accepted.fetch_add(1u);
      }
    }
  });
  realAudio.join();saver.join();
  const auto final=save();
  check(healthy.load() && accepted.load()>=100u &&
        rejected.load()==0u && mixed.load()==0u &&
        concurrentEnergy>0.001 && final==B,
        "P186 sounding automation mixed, stale or dropped V5 project saves");
  std::printf("P186_AUDIBLE_V5_SAVES handshake=%d stale=%u"
              " unpaced_accepted=%u mixed=%u rejected=%u energy=%.6f\n",
              exchanges,stale.load(),accepted.load(),mixed.load(),
              rejected.load(),initialEnergy+handshakeEnergy+concurrentEnergy);
  ownerBank(resources,nullptr);
  std::puts("P186_LIVE_ORIGINAL_CHIP_SOUNDING_AUTOMATION_STATE_FRESH_AND_ATOMIC_PASS");
}


// P187 independent adversarial repeat of P186 N08: the GUI/host has accepted
// BOTH B control edits BEFORE each nonzero 512-frame sounding callback.
// While the audio callback acknowledges the identical points, saves MUST
// NEVER see old A or a mixed A/B. P186 self-oracle lacked live controller
// echo on every block, so its 1.27M saves missed this exact failure.
void verifyP187SoundingControllerEchoWholeGeneration(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"voice32.sf2");
  Plugin unit(factory,id,host);unit.start(0,1,44100.);
  const auto setGui=[&](bool b){
    check(unit.controller->setParamNormalized(1001u,b?.875:.125)==kResultOk &&
          unit.controller->setParamNormalized(1100u,b?.75:.25)==kResultOk,
          "P187 original N08 pre-echo controller edit failed");
  };
  const auto save=[&](){
    MemoryStream stream;
    check(unit.component->getState(&stream)==kResultOk,
          "P187 active VST3 getState was rejected");
    const auto* bytes=reinterpret_cast<const std::uint8_t*>(stream.getData());
    const auto n=static_cast<std::size_t>(stream.getSize());
    check(bytes && n==648u,"P187 expected full V5 controller saved state");
    return std::vector<std::uint8_t>(bytes,bytes+n);
  };
  const auto render=[&](bool b,bool first){
    ParameterChanges changes(2);
    const auto put=[&](ParamID param,double v){
      int32 qi=0,pi=0;
      auto* q=changes.addParameterData(param,qi);
      check(q && q->addPoint(0,v,pi)==kResultOk,
            "P187 controller echo point not sent to actual processor");
    };
    put(1001u,b?.875:.125);
    put(1100u,b?.75:.25);
    EventList events;
    if(first){
      Event note{};note.type=Event::kNoteOnEvent;
      note.noteOn.channel=0;note.noteOn.pitch=60;
      note.noteOn.noteId=701;note.noteOn.velocity=.95f;
      note.sampleOffset=10;
      check(events.addEvent(note)==kResultOk,
            "P187 sounding real original-chip note not queued");
    }
    std::array<float,512> l{},r{};
    float* ch[2]={l.data(),r.data()};
    AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=ch;
    ProcessData pd{};pd.processMode=kRealtime;
    pd.symbolicSampleSize=kSample32;
    pd.numSamples=512;pd.numOutputs=1;pd.outputs=&bus;
    pd.inputEvents=&events;pd.inputParameterChanges=&changes;
    check(unit.processor->process(pd)==kResultOk,
          "P187 nonzero 512-frame sounding original VST3 callback failed");
    double energy=0.;
    for(int i=0;i<512;++i){
      check(std::isfinite(l[i])&&std::isfinite(r[i]),
            "P187 audible original-chip output nonfinite");
      energy+=double(l[i])*l[i]+double(r[i])*r[i];
    }
    return energy;
  };
  setGui(false);
  double energy=render(false,true);
  const auto A=save();
  setGui(true);
  const auto B=save();
  check(A!=B && A.size()==648u && B.size()==648u,
        "P187 audio A/B baseline cannot distinguish master+attack");
  energy+=render(true,false);
  check(save()==B,"P187 starting callback could not acknowledge B");
  setGui(false);
  energy+=render(false,false);
  check(save()==A,"P187 initial A restore did not complete");

  std::atomic<unsigned> phase{0u};
  std::atomic<bool> stopping{false},healthy{true};
  std::atomic<uint64_t> checked{0},stale{0},mixed{0},rejected{0};
  std::thread observer([&](){
    while(!stopping.load(std::memory_order_acquire)){
      const unsigned before=phase.load(std::memory_order_acquire);
      if(before&1u){std::this_thread::yield();continue;}
      MemoryStream stream;
      const auto rc=unit.component->getState(&stream);
      const unsigned after=phase.load(std::memory_order_acquire);
      if(before!=after || (after&1u))continue;
      if(rc!=kResultOk){rejected.fetch_add(1);continue;}
      const auto* bytes=reinterpret_cast<const std::uint8_t*>(stream.getData());
      const auto n=static_cast<std::size_t>(stream.getSize());
      const auto& expected=((before/2u)&1u)?B:A;
      if(!bytes || n!=expected.size() ||
         std::memcmp(bytes,expected.data(),n)!=0){
        const auto& other=((before/2u)&1u)?A:B;
        if(bytes && n==other.size() &&
           std::memcmp(bytes,other.data(),n)==0)stale.fetch_add(1);
        else mixed.fetch_add(1);
      }
      checked.fetch_add(1);
    }
  });
  try {
    for(int i=0;i<900;++i){
      const bool b=(i&1)==0;
      phase.fetch_add(1u,std::memory_order_release); // odd: host edits
      setGui(b);
      check(save()==(b?B:A),
            "P187 controller B/A accepted BEFORE callback not serializable");
      phase.fetch_add(1u,std::memory_order_release); // even: B/A authoritative
      energy+=render(b,false);
      check(save()==(b?B:A),
            "P187 accepted controller edit vanished after callback");
      if((i&3)==0)std::this_thread::yield();
    }
  }catch(...){
    healthy.store(false);stopping.store(true);
    observer.join();
    ownerBank(resources,nullptr);
    throw;
  }
  stopping.store(true,std::memory_order_release);
  observer.join();
  std::printf("P187_SOUNDING_N08_ECHO accepted=%llu stale=%llu mixed=%llu rejected=%llu energy=%.9f\n",
      static_cast<unsigned long long>(checked.load()),
      static_cast<unsigned long long>(stale.load()),
      static_cast<unsigned long long>(mixed.load()),
      static_cast<unsigned long long>(rejected.load()),energy);
  check(healthy.load() && checked.load()>=1000u &&
        stale.load()==0u && mixed.load()==0u &&
        rejected.load()==0u && energy>.001,
        "P187 Codex N08 sounding controller echo saw A/mixed after B confirmed");
  ownerBank(resources,nullptr);
  std::puts("P187_ORIGINAL_CHIP_SOUNDING_CONTROLLER_ECHO_N08_ATOMIC_FRESH_PASS");
}

// Legacy V4 saved a raw DataEntry value but had NO proof it was ever
// effective RPN0/0. A wrong old NRPN/NULL value must not become a hardware
// pitch range on reopen. New RPN-NULL sessions use the reserved bitmap.
void verifyP187LegacyUnownedDataEntryDoesNotRetuneBend(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"vintage.sf2");
  Saved legacy{};legacy.version=4u;legacy.bank=1u;
  legacy.midiStates.fill(0xFFFFu);
  legacy.advancedStates.fill(0xFFFFu);
  legacy.midiStates[7]=16383u;
  legacy.advancedStates[2]=0u;
  legacy.advancedStates[3]=1u; // RPN 0/1, not pitch-bend range 0/0 (Codex X02)
  legacy.advancedStates[4]=5u; // historical Data Entry ignored by bend hardware
  legacy.parameterModes[0]=1u;
  Saved defaultRange=legacy;
  defaultRange.advancedStates[4]=0xFFFFu;
  Saved wrongRange=legacy;
  wrongRange.advancedStates[2]=0u;
  wrongRange.advancedStates[3]=0u;
  Plugin source(factory,id,host),expected(factory,id,host),wrong(factory,id,host);
  source.start(0,1,44100.);expected.start(0,1,44100.);
  wrong.start(0,1,44100.);
  p015Restore(source,legacy);
  p015Restore(expected,defaultRange);
  p015Restore(wrong,wrongRange);
  const auto got=p015Play(source,0,60);
  const auto correct=p015Play(expected,0,60);
  const auto bad=p015Play(wrong,0,60);
  check(correct!=bad,
        "P187 legacy original-chip pitch range 2/5 independent oracle invalid");
  check(got==correct,
        "P187 historical RPN 0/1 data entry illegally retuned pitch bend");
  ownerBank(resources,nullptr);
  std::puts("P187_LEGACY_V4_UNOWNED_DATA_ENTRY_BEND_DEFAULT_PCM_PASS");
}

// VST3 explicit NoteID, unlike noteId==-1, must NEVER fall back by pitch.
// A delayed duplicate NoteOff for an already finished ID is no authority
// to release a newer independently held same-pitch note.
void verifyP187LateExplicitNoteOffNeverKillsNewNoteId(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"voice32.sf2");
  const auto scenario=[&](bool lateOff){
    Plugin unit(factory,id,host);unit.start(0,1,44100.);
    std::array<float,512> l{},r{};
    float* ch[2]={l.data(),r.data()};
    AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=ch;
    EventList events;
    ProcessData pd{};pd.processMode=kRealtime;
    pd.symbolicSampleSize=kSample32;pd.numSamples=512;
    pd.numOutputs=1;pd.outputs=&bus;pd.inputEvents=&events;
    std::vector<float> after;
    for(int block=0;block<60;++block){
      events.clear();
      const auto event=[&](bool on,int id){
        Event e{};e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;
        e.sampleOffset=12;
        if(on){
          e.noteOn.channel=0;e.noteOn.pitch=60;
          e.noteOn.noteId=id;e.noteOn.velocity=.95f;
        }else{
          e.noteOff.channel=0;e.noteOff.pitch=60;
          e.noteOff.noteId=id;
        }
        check(events.addEvent(e)==kResultOk,
              "P187 independent delayed explicit NoteID event rejected");
      };
      if(block==0)event(true,100);
      if(block==4)event(false,100);
      if(block==8)event(true,101);
      if(block==12 && lateOff)event(false,100); // stale duplicate ID
      if(block==36)event(false,101);
      check(unit.processor->process(pd)==kResultOk,
            "P187 genuine original-chip note route render rejected");
      if(block>=14 && block<33)
        for(int i=0;i<512;++i){
          check(std::isfinite(l[i])&&std::isfinite(r[i]),
                "P187 actual original chip produced nonfinite PCM");
          after.push_back(l[i]);after.push_back(r[i]);
        }
    }
    return after;
  };
  const auto withoutLate=scenario(false);
  const auto withLate=scenario(true);
  check(withoutLate==withLate,
        "P187 delayed NoteOff ID100 muted another held ID101");
  ownerBank(resources,nullptr);
  std::puts("P187_DELAYED_EXPLICIT_NOTEID_NEVER_MUTES_ANOTHER_VOICE_PASS");
}


// P187 Codex X01: after valid bend12, NRPN selection deactivates *both*
// hardware RPN selectors. A lone new RPN LSB=0 plus DataEntry=5 must
// never re-enable the old historical MSB=0 or alter effective bend12.
void verifyP187PartialRpnAfterNrpnNeverRetunesBend(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"vintage.sf2");
  Plugin live(factory,id,host);live.start(0,1,44100.);
  IMidiMapping* raw=nullptr;
  check(live.controller->queryInterface(IMidiMapping::iid,
        reinterpret_cast<void**>(&raw))==kResultOk&&raw,
        "P187 partial-RPN actual VST3 IMidiMapping missing");
  const IPtr<IMidiMapping> mapping=owned(raw);
  const auto send=[&](CtrlNumber cc,int value){
    ParamID mapped=0;
    check(mapping->getMidiControllerAssignment(0,0,cc,mapped)==kResultTrue,
          "P187 partial RPN controller mapping missing");
    ParameterChanges changes(1);
    int32 q=0,p=0;
    auto* item=changes.addParameterData(mapped,q);
    const double norm=cc==kPitchBend?double(value)/16383.:
                                          double(value)/127.;
    check(item&&item->addPoint(0,norm,p)==kResultOk,
          "P187 actual mapped RPN controller queue missing");
    ProcessData data{};data.processMode=kRealtime;
    data.symbolicSampleSize=kSample32;data.numSamples=0;
    data.inputParameterChanges=&changes;
    check(live.processor->process(data)==kResultOk,
          "P187 actual RPN/NRPN controller event refused");
  };
  send(kCtrlRPNSelectMSB,0);send(kCtrlRPNSelectLSB,0);
  send(kCtrlDataEntryMSB,12);
  send(kCtrlNRPNSelectMSB,1);send(kCtrlNRPNSelectLSB,2);
  send(kCtrlRPNSelectLSB,0); // only ONE new selector half
  send(kCtrlDataEntryMSB,5); // unsupported new partial RPN
  send(kPitchBend,16383);
  MemoryStream saved;
  check(live.component->getState(&saved)==kResultOk &&
        saved.getSize()==sizeof(Saved),
        "P187 partial RPN saved state must remain legacy V4-size");
  Saved result{};std::memcpy(&result,saved.getData(),sizeof(result));
  check(result.advancedStates[4]==12u &&
        result.advancedStates[2]==127u &&
        result.advancedStates[3]==0u &&
        result.parameterModes[0]==1u,
        "P187 partial RPN reused historical 0/0 to overwrite true range12");
  Saved reference=result;
  reference.advancedStates[2]=0u;
  reference.advancedStates[3]=0u;
  Saved wrong=reference;wrong.advancedStates[4]=5u;
  Plugin reopened(factory,id,host),bend12(factory,id,host),bend5(factory,id,host);
  reopened.start(0,1,44100.);bend12.start(0,1,44100.);
  bend5.start(0,1,44100.);
  p015Restore(reopened,result);
  p015Restore(bend12,reference);
  p015Restore(bend5,wrong);
  const auto pcm12=p015Play(bend12,0,60);
  const auto pcm5=p015Play(bend5,0,60);
  check(pcm12!=pcm5,"P187 partial RPN range12/5 actual PCM oracle invalid");
  check(p015Play(reopened,0,60)==pcm12 &&
        p015Play(live,0,60)==pcm12,
        "P187 partial RPN DataEntry changed original chip live or recalled bend");
  ownerBank(resources,nullptr);
  std::puts("P187_PARTIAL_RPN_AFTER_NRPN_LIVE_AND_RECALL_BEND12_PCM_PASS");
}

// P187 Codex X05: CH1 drum-browser audition physically renders CH10.
// MIDI pan on CH1 must reach that physical channel; compare two genuinely
// different pan values to separately controlled CH10 hardware references.
void verifyP187BrowserDrumPanFollowsPhysicalCH10(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"multi-drums.sf2");
  const auto renderWithPan=[&](bool browser,int pan){
    Plugin unit(factory,id,host);
    unit.start(5,browser?2:0,44100.);
    const unsigned ch=browser?0u:9u;
    ParameterChanges changes(1);
    int32 qi=0,pi=0;
    auto* q=changes.addParameterData(2000u+ch*8u+2u,qi);
    check(q && q->addPoint(0,double(pan)/127.,pi)==kResultOk,
          "P187 browser CH1 or direct CH10 pan point missing");
    EventList events;
    std::array<float,1024> l{},r{};
    float* audio[2]={l.data(),r.data()};
    AudioBusBuffers output{};output.numChannels=2;
    output.channelBuffers32=audio;
    ProcessData data{};data.processMode=kRealtime;
    data.symbolicSampleSize=kSample32;data.numSamples=1024;
    data.numOutputs=1;data.outputs=&output;
    data.inputEvents=&events;data.inputParameterChanges=&changes;
    std::uint64_t hash=14695981039346656037ull;
    double energy=0.;
    for(int block=0;block<30;++block){
      events.clear();
      if(block==0){
        Event on{};on.type=Event::kNoteOnEvent;
        on.sampleOffset=12;on.noteOn.channel=static_cast<int16>(ch);
        on.noteOn.pitch=60;on.noteOn.noteId=501;
        on.noteOn.velocity=.90f;
        check(events.addEvent(on)==kResultOk,
              "P187 direct CH10/browser real drum NoteOn rejected");
      }else if(block==12){
        Event off{};off.type=Event::kNoteOffEvent;
        off.sampleOffset=12;off.noteOff.channel=static_cast<int16>(ch);
        off.noteOff.pitch=60;off.noteOff.noteId=501;
        check(events.addEvent(off)==kResultOk,
              "P187 direct CH10/browser real drum NoteOff rejected");
      }
      check(unit.processor->process(data)==kResultOk,
            "P187 actual stereo original-chip drum pan render rejected");
      changes.clearQueue(); // one controller point at block zero only
      for(int i=0;i<1024;++i)
        for(float v:{l[i],r[i]}){
          check(std::isfinite(v),"P187 nonfinite drum pan PCM");
          energy+=double(v)*v;
          std::uint32_t bits=0;std::memcpy(&bits,&v,sizeof(bits));
          hash=(hash^bits)*1099511628211ull;
        }
    }
    check(energy>.001,"P187 pan positive fixture was not audible");
    return hash;
  };
  const auto browserLeft=renderWithPan(true,0);
  const auto browserRight=renderWithPan(true,127);
  const auto directLeft=renderWithPan(false,0);
  const auto directRight=renderWithPan(false,127);
  check(directLeft!=directRight,
        "P187 genuine CH10 pan0/127 hardware oracle is not discriminating");
  check(browserLeft!=browserRight &&
        browserLeft==directLeft && browserRight==directRight,
        "P187 CH1 browser MIDI pan did not control physical CH10 audio");
  ownerBank(resources,nullptr);
  std::puts("P187_CH1_BROWSER_PAN0_PAN127_MATCHES_DIRECT_CH10_PCM_PASS");
}

// P187 Codex X06: SHA-verified active setState must upload precisely
// those same checked bytes, even if an independent Windows thread tries
// repeatedly to replace the filename while the 28MiB SF2 chip loads.
// Accepted A must play A and save SHA(A); rejected attempts must leave
// the previous B VST3 state/audio unchanged.
void verifyP187ActiveSf2LoadPinPreventsIdentitySplit(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  const auto selected=resources/"p187-racing-owner.sf2";
  ownerBank(resources,"vintage.sf2");
  Plugin a(factory,id,host);a.start(0,1,44100.);
  const auto expectedA=p015Play(a,0,60);
  ownerBank(resources,"77-presets.sf2");
  Plugin b(factory,id,host);b.start(0,1,44100.);
  const auto expectedB=p015Play(b,0,60);
  check(expectedA!=expectedB,
        "P187 two distinct loaded SoundFonts produced identical PCM");
  fs::copy_file(resources/"vintage.sf2",selected,
                fs::copy_options::overwrite_existing);
  const auto signedA=p174V5State(selected);
  unsigned accepted=0,rejected=0,blocked=0,writerAttempts=0;
  try{
    for(int round=0;round<6;++round){
      fs::copy_file(resources/"vintage.sf2",selected,
                    fs::copy_options::overwrite_existing);
      Plugin unit(factory,id,host);unit.start(0,1,44100.);
      const auto oldState=p174Saved(unit);
      std::atomic<bool> start{false},done{false};
      std::atomic<unsigned> attempts{0},denied{0};
      std::thread writer([&](){
        while(!start.load(std::memory_order_acquire))
          std::this_thread::yield();
        while(!done.load(std::memory_order_acquire)){
          for(const auto& src:{resources/"77-presets.sf2",
                               resources/"vintage.sf2"}){
            attempts.fetch_add(1u,std::memory_order_relaxed);
            try {
              fs::copy_file(src,selected,
                            fs::copy_options::overwrite_existing);
            } catch(const fs::filesystem_error&){
              denied.fetch_add(1u,std::memory_order_relaxed);
            }
            if(done.load(std::memory_order_acquire))break;
          }
        }
      });
      start.store(true,std::memory_order_release);
      const auto result=p174ApplyV5(unit,signedA);
      done.store(true,std::memory_order_release);
      writer.join();
      writerAttempts+=attempts.load();
      blocked+=denied.load();
      const auto stored=p174Saved(unit);
      if(result==kResultOk){
        ++accepted;
        check(stored==signedA,
              "P187 accepted active load saved a different SF2 SHA/path");
        check(p015Play(unit,0,60)==expectedA,
              "P187 active SHA(A) accepted but audible PCM is swapped SF2 B");
        check(p174Saved(unit)==signedA,
              "P187 chip playback changed the accepted explicit SHA(A)");
      }else{
        ++rejected;
        check(stored==oldState,
              "P187 rejected concurrent SF2 swap mutated previous valid session");
        check(p015Play(unit,0,60)==expectedB,
              "P187 failed active SF2 restore mutated previous chip PCM");
      }
    }
  }catch(...){fs::remove(selected);ownerBank(resources,nullptr);throw;}
  fs::remove(selected);
  ownerBank(resources,nullptr);
  std::printf("P187_SF2_ACTIVE_RECALL_RACE accepted=%u rejected=%u writer_attempts=%u locked=%u\n",
              accepted,rejected,writerAttempts,blocked);
  check(accepted+rejected==6u && writerAttempts>0u && blocked>0u,
        "P187 external SF2 change/lock race was not actually exercised");
  std::puts("P187_PINNED_OWNER_SHA_MATCHES_ACTIVE_SF2_CHIP_PCM_PASS");
}

void verifyP189BankEventOrderAndSave(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"77-presets.sf2");
  Plugin referenceRom(factory,id,host),referenceSf2(factory,id,host);
  referenceRom.start(0,0,44100.);referenceSf2.start(0,1,44100.);
  const auto rom=p015Play(referenceRom,0,60);
  const auto sf2=p015Play(referenceSf2,0,60);
  check(rom!=sf2,"P189 ROM and SoundFont PCM references are indistinguishable");

  Plugin unit(factory,id,host);
  unit.start(0,0,44100.);
  FUnknownPtr<IMidiMapping> mapping(unit.controller);
  ParamID cc0=0;
  check(mapping && mapping->getMidiControllerAssignment(0,0,kCtrlBankSelectMSB,cc0)==kResultTrue,
        "P189 CC0 mapping");
  auto flush=[&](Plugin& target,bool midiSwitch,bool timedHostLast){
    ParameterChanges changes(2);
    const auto add=[&](ParamID tag,int32 offset,double value){
      int32 number=0,point=0;
      auto* q=changes.addParameterData(tag,number);
      check(q && q->addPoint(offset,value,point)==kResultOk,"P189 bank point");
    };
    if(midiSwitch)add(cc0,0,1./127.);
    if(timedHostLast)add(1002u,17,0.);
    std::array<float,64> left{},right{};
    float* channels[]{left.data(),right.data()};
    AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=channels;
    ProcessData data{};data.symbolicSampleSize=kSample32;data.processMode=kRealtime;
    data.numSamples=64;data.numOutputs=1;data.outputs=&out;data.inputParameterChanges=&changes;
    check(target.processor->process(data)==kResultOk,"P189 ordered VST3 block");
  };
  const auto save=[&](){
    MemoryStream stream;
    check(unit.component->getState(&stream)==kResultOk && stream.getSize()==sizeof(Saved),
          "P189 save while processing");
    Saved bytes{};std::memcpy(&bytes,stream.getData(),sizeof(bytes));
    return bytes;
  };
  check(unit.controller->setParamNormalized(1002u,0.)==kResultOk,"P189 earlier controller bank");
  const auto before=save();
  std::atomic<bool> entered{false},done{false};
  std::exception_ptr failure;
  std::thread audio([&](){
    entered.store(true,std::memory_order_release);
    try {flush(unit,true,false);}catch(...){failure=std::current_exception();}
    done.store(true,std::memory_order_release);
  });
  std::vector<Saved> observed;
  observed.reserve(256);
  std::exception_ptr saveFailure;
  try {
    while(!entered.load(std::memory_order_acquire))std::this_thread::yield();
    do observed.push_back(save());while(!done.load(std::memory_order_acquire)&&observed.size()<256u);
  }catch(...){saveFailure=std::current_exception();}
  audio.join();
  if(failure)std::rethrow_exception(failure);
  if(saveFailure)std::rethrow_exception(saveFailure);
  const auto after=save();
  check(after.bank==1u && after.advancedStates[0]==1u &&
        unit.controller->getParamNormalized(1002u)==.5,
        "P189 older controller ROM reversed the later MIDI bank");
  for(const auto& state:observed)
    check(std::memcmp(&state,&before,sizeof(state))==0 ||
          std::memcmp(&state,&after,sizeof(state))==0,"P189 mixed bank generation");
  Plugin userRecall(factory,id,host);
  userRecall.start(0,0,44100.);p015Restore(userRecall,after);
  check(p015Play(userRecall,0,60)==sf2,"P189 MIDI bank recall PCM");
  flush(unit,true,true);
  const auto reverse=save();
  check(reverse.bank==0u && reverse.advancedStates[0]==0xFFFFu &&
        unit.controller->getParamNormalized(1002u)==0.,
        "P189 later timed host bank did not win over CC0");
  // The reference sees exactly the same number and partitioning of audio
  // frames before NoteOn. Free-running chip FX/LFO phase is not a bank oracle.
  Plugin matchedRom(factory,id,host);
  matchedRom.start(0,0,44100.);
  flush(matchedRom,false,false);flush(matchedRom,false,true);
  const auto reversedPcm=p015Play(unit,0,60);
  check(reversedPcm==p015Play(matchedRom,0,60),"P189 later host point ROM PCM");
  Plugin romRecall(factory,id,host);
  romRecall.start(0,0,44100.);p015Restore(romRecall,reverse);
  check(p015Play(romRecall,0,60)==rom,"P189 later host point ROM recall PCM");
  ownerBank(resources,nullptr);
  std::puts("P189_CHRONOLOGICAL_BANK_MIDI_HOST_SAVE_PCM_PASS");
}

void verifyP189AuditionPanIsolation(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host,const fs::path& resources){
  ownerBank(resources,"multi-drums-sustain.sf2");
  const auto run=[&](int playedChannel,int changedChannel){
    Plugin unit(factory,id,host);unit.start(0,2,44100.);
    FUnknownPtr<IMidiMapping> mapping(unit.controller);
    check(bool(mapping),"P189 pan mapping");
    ParameterChanges parameters(1);
    const auto pan=[&](int channel,double value){
      ParamID param=0;
      check(mapping->getMidiControllerAssignment(0,channel,kCtrlPan,param)==kResultTrue,
            "P189 pan controller mapped");
      int32 i=0,j=0;auto* q=parameters.addParameterData(param,i);
      check(q && q->addPoint(0,value,j)==kResultOk,"P189 pan point");
    };
    EventList events;
    Event note{};note.type=Event::kNoteOnEvent;
    note.noteOn.channel=static_cast<int16>(playedChannel);
    note.noteOn.pitch=60;note.noteOn.velocity=.75f;note.noteOn.noteId=189;
    check(events.addEvent(note)==kResultOk,"P189 pan held note");
    std::array<float,512> left{},right{};
    float* channels[]{left.data(),right.data()};
    AudioBusBuffers out{};out.numChannels=2;out.channelBuffers32=channels;
    ProcessData data{};data.symbolicSampleSize=kSample32;data.processMode=kRealtime;
    data.numSamples=512;data.numOutputs=1;data.outputs=&out;
    data.inputParameterChanges=&parameters;data.inputEvents=&events;
    pan(playedChannel,1.);
    std::vector<float> pcm;
    double energy=0.;
    for(int n=0;n<24;++n){
      if(n==4 && changedChannel>=0)pan(changedChannel,0.);
      check(unit.processor->process(data)==kResultOk,"P189 held pan PCM render");
      if(n>=5)
        for(unsigned i=0;i<left.size();++i){
          pcm.push_back(left[i]);pcm.push_back(right[i]);
          energy+=double(left[i])*left[i]+double(right[i])*right[i];
        }
      parameters.clearQueue();events.clear();
    }
    check(energy>0.001,"P189 held reference is silent");
    return pcm;
  };
  for(int held:{0,9}){
    const auto unchanged=run(held,-1);
    check(run(held,held)!=unchanged,"P189 direct pan positive control must change PCM");
    check(run(held,held==0?9:0)==unchanged,
          "P189 control from other MIDI lane altered a held chip voice");
  }
  ownerBank(resources,nullptr);
  std::puts("P189_HELD_NATIVE_AND_AUDITION_PAN_ISOLATION_PCM_PASS");
}

int main(int argc,char** argv){
 try{
  std::setvbuf(stdout,nullptr,_IONBF,0);
  check(argc==3&&(std::strcmp(argv[2],"--public")==0||
      std::strcmp(argv[2],"--private")==0||
      std::strcmp(argv[2],"--synthetic")==0),
      "Usage: P010NativeHostQA.exe <P009-vst3-bundle> --public|--private|--synthetic");
  const bool priv=std::strcmp(argv[2],"--private")==0;
  const bool synthetic=std::strcmp(argv[2],"--synthetic")==0;
  const fs::path bundle=fs::absolute(argv[1]);
  check(fs::is_directory(bundle),"actual native VST3 bundle missing");
  const auto resources=bundle/"Contents"/"Resources"/"P009-Private";
  if(priv){
    for(const char* file:{"awe32.raw","SYNTHGM.SBK","77-presets.sf2","vintage.sf2"})
      check(fs::is_regular_file(resources/file),"original ROM/SBK/2 owner SF2 missing");
    check(fs::file_size(resources/"awe32.raw")==1048576u,"ROM must be genuine 1MiB");
  }else if(synthetic){
    for(const char* file:{"awe32.raw","SYNTHGM.SBK","77-presets.sf2","vintage.sf2"})
      check(fs::is_regular_file(resources/file),
            "public P165 original-chip VST3 synthetic A/B SF2 fixture missing");
    check(fs::file_size(resources/"awe32.raw")==1048576u,
          "public fake ROM must be exactly 1MiB");
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
  verifyZeroFrameParameterFlush(factory,cls->ID(),host.get());
  verifyP169ControllerComponentStateAppliedAndRejectsCorrupt(factory,cls->ID(),host.get());
  verifyP166StoppedHostEditsAndAudioReadback(factory,cls->ID(),host.get());
  verifyP011MultichannelProjectRecall(factory,cls->ID(),host.get());
  verifyP165GuiV5OwnerAssetState(factory,cls->ID(),host.get());
  verifyP185OriginalModeWithNonzeroSavedSend(factory,cls->ID(),host.get());
  verifyP185CoherentGUIAndMidiAutomationSaves(factory,cls->ID(),host.get());
  auditV5DoesNotEraseAdvancedMidiControllers(factory,cls->ID(),host.get());
  verifyP165ConcurrentHostStateWithLiveProcess(factory,cls->ID(),host.get());
  verifyP012MidiMapping(factory,cls->ID(),host.get());
  verifyP014PatchAutomationState(factory,cls->ID(),host.get());
  verifyP015PublicActiveRestore(factory,cls->ID(),host.get());
  verifyP020True64FrameHostLatency(factory,cls->ID(),host.get());
  if(synthetic){
    ownerBank(resources,nullptr);
    verifyTailAndBlockPartitions(factory,cls->ID(),host.get(),resources);
    verifyDrumReleaseAfterBankChange(factory,cls->ID(),host.get(),resources);
    verifyP185BrowserDrumKitActuallyChangesPCM(factory,cls->ID(),host.get(),resources);
    verifyP186BendRangeRpnNullAndNrpnData(factory,cls->ID(),host.get(),resources);
    verifyP186CH10ProgramAndCC0DrumRouting(factory,cls->ID(),host.get(),resources);
    verifyP186TwoNoteIDsAfterBankSwitch(factory,cls->ID(),host.get(),resources);
    verifyP186ExplicitSF2SHAActivationRejectsChangedFile(factory,cls->ID(),host.get(),resources);
    verifyP186CutoffAndSendRestoreActualPCM(factory,cls->ID(),host.get(),resources);
    verifyP186AudibleAtomicStateAndFreshness(factory,cls->ID(),host.get(),resources);
    verifyP187SoundingControllerEchoWholeGeneration(factory,cls->ID(),host.get(),resources);
    verifyP187LegacyUnownedDataEntryDoesNotRetuneBend(factory,cls->ID(),host.get(),resources);
    verifyP187LateExplicitNoteOffNeverKillsNewNoteId(factory,cls->ID(),host.get(),resources);
    verifyP187PartialRpnAfterNrpnNeverRetunesBend(factory,cls->ID(),host.get(),resources);
    verifyP187BrowserDrumPanFollowsPhysicalCH10(factory,cls->ID(),host.get(),resources);
    verifyP187ActiveSf2LoadPinPreventsIdentitySplit(factory,cls->ID(),host.get(),resources);
    verifyP185PitchRangeSurvivesNRPN(factory,cls->ID(),host.get(),resources);
    verifyP185StoppedROMBankRestoresAgainstOldCC0(factory,cls->ID(),host.get(),resources);
    verifyP189BankEventOrderAndSave(factory,cls->ID(),host.get(),resources);
    verifyP189AuditionPanIsolation(factory,cls->ID(),host.get(),resources);
    verifyP165HostBankSupersedesOldCC0(factory,cls->ID(),host.get(),resources);
    verifyP168AllTenHardwareKnobsOriginalPCMAndReopen(factory,cls->ID(),host.get(),resources);
    verifyP167TimedMasterOriginalPCM(factory,cls->ID(),host.get(),resources);
    verifyP165HeldNoteOverflowRecovery(factory,cls->ID(),host.get(),resources);
    verifyP165InterruptedAssetLifecycle(factory,cls->ID(),host.get(),resources);
    verifyP173LiveSf2SwitchOriginalChipFade(factory,cls->ID(),host.get(),resources);
    verifyP165RealChipFxSendAndPresetPCM(factory,cls->ID(),host.get(),resources);
    verifyP192ImplicitSf2SustainEqualsExplicit(factory,cls->ID(),host.get(),resources);
    verifyP194OriginalSf2AttackDefaults(factory,cls->ID(),host.get(),resources);
    verifyP195CreativeSf2PresetAdditive(factory,cls->ID(),host.get(),resources);
    verifyP196CreativeSf2EndOffsets(factory,cls->ID(),host.get(),resources);
    verifyP171RamOverflow(factory,cls->ID(),host.get(),resources);
    verifyP174OneWordPhysicalRamBoundary(factory,cls->ID(),host.get(),resources);
    verifyP174SixtyActiveValidInvalidLoads(factory,cls->ID(),host.get(),resources);
    verifyP175ThirtyTwoOriginalChipVoicesAndSustain(factory,cls->ID(),host.get(),resources);
    verifyP176RapidSf2SwitchForwardProgress(factory,cls->ID(),host.get(),resources);
    verifyP177CorruptV5StreamFailClosed(factory,cls->ID(),host.get(),resources);
    verifyP178ConcurrentOwnerSF2StateTransaction(factory,cls->ID(),host.get(),resources);
    verifyP179PacedAudibleHostSaves(factory,cls->ID(),host.get(),resources);
    verifyP180Sparse77OriginalChipSelection(factory,cls->ID(),host.get(),resources);
    verifyP165PrivateExternalSf2V5OriginalPCM(factory,cls->ID(),host.get(),
                                              resources,0);
    std::puts("P165_PUBLIC_REAL_VST3_SYNTHETIC_86BOX_SF2_V5_SHA_POSITIVE_NEGATIVE_PCM_PASS");
    return 0;
  }
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
  verifyP165PrivateExternalSf2V5OriginalPCM(factory,cls->ID(),host.get(),resources);
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
  verifyP181PrivateMultirateOriginalChipMatrix(factory,cls->ID(),host.get(),resources);
  std::puts("P010_PRIVATE_ACTUAL_WIN64_VST3_ORIGINAL_ROM_SBK_TWO_SF2_77_PRESETS_ABA_PASS");
  return 0;
 }catch(const std::exception& e){
   std::fprintf(stderr,"P010_ACTUAL_WIN64_VST3_ORIGINAL_AUDIO_FAIL %s\n",e.what());
   return 1;
 }
}
