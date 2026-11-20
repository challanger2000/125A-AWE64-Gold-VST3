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
 uint32 magic=0x39303050u,version=3u,program=0u,bank=0u;
 float master=1.f;
 std::array<std::uint8_t,15> otherPrograms{},otherBanks{};
 std::array<std::uint8_t,2> reserved{};
 std::array<std::uint16_t,16*8> midiStates{};
};
static_assert(sizeof(Saved)==308,"P012 16-channel MIDI controller project state");
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
void verifyP011MultichannelProjectRecall(
    const VST3::Hosting::PluginFactory& factory,
    const VST3::UID& id,FUnknown* host) {
 auto original=Plugin(factory,id,host);
 Saved expected{};expected.midiStates.fill(0xFFFFu);
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
           "P012 308-byte VST3 state output");
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
 check(upgraded.version==3u&&upgraded.program==12u&&
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
 check(upgraded2.version==3u && upgraded2.program==out.program &&
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
    ParamID bad=0;
    check(mapping->getMidiControllerAssignment(1,0,kCtrlModWheel,bad)
          !=kResultTrue,"P012 rejects nonexistent MIDI bus");
    check(mapping->getMidiControllerAssignment(0,16,kCtrlModWheel,bad)
          !=kResultTrue,"P012 rejects invalid MIDI channel");
    check(mapping->getMidiControllerAssignment(0,0,kCtrlBreath,bad)
          !=kResultTrue,"P012 rejects unsupported CC");

    p.start(7,0,44100.);
    ParameterChanges params(8);
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
    std::puts("P012_REAL_STEINBERG_VST3_IMIDIMAPPING_128_ASSIGNMENTS_SAMPLE_OFFSETS_STATE_RECALL_PASS");
}

struct Stats {
 double rms=0,peak=0;
 uint64_t hash=14695981039346656037ull;
 std::vector<int16_t> audio;
};
Stats render(const VST3::Hosting::PluginFactory& factory,
             const VST3::UID& id,FUnknown* host,
             const char* label,int hz,int ch,int key,int velocity,
             int program,int bank,bool save=false) {
 Plugin p(factory,id,host);p.start(program,bank,hz);
 Stats s;
 std::array<float,frames> left{},right{};
 float* chan[2]={left.data(),right.data()};
 AudioBusBuffers output{};output.numChannels=2;output.channelBuffers32=chan;
 EventList ev;
 ProcessData data{};
 data.processMode=kRealtime;data.symbolicSampleSize=kSample32;
 data.numSamples=frames;data.numOutputs=1;data.outputs=&output;data.inputEvents=&ev;
 double sum=0;unsigned count=0;
 for(int b=0;b<34;++b){
   ev.clear();
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
             " velocity=%d rms=%.9f peak=%.9f hash=%016llX latency=%u\n",
             label,hz,ch+1,bank,program,key,velocity,s.rms,s.peak,
             static_cast<unsigned long long>(s.hash),p.processor->getLatencySamples());
 std::fflush(stdout);
 return s;
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
  verifyP012MidiMapping(factory,cls->ID(),host.get());
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
  check(first.hash!=other.hash,"both owner SF2s share the same PCM fingerprint");
  std::set<uint64_t> unique;
  for(int prog=0;prog<77;++prog){
    const auto s=render(factory,cls->ID(),host.get(),
        "USER_77_SWEEP",44100,0,60,100,prog,1);
    audible(s,"one of 77 SF2 presets");
    unique.insert(s.hash);
  }
  check(unique.size()==77u,"77 preset SF2 outputs are not distinct");
  std::puts("P010_WIN64_ORIGINAL_VST3_77_DISTINCT_OWNER_PRESETS_PASS");
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
