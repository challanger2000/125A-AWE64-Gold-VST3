// P010 actual Windows x64 Steinberg VST3 original PCM owner acceptance.
// No guest Win95, 86Box VM, fabricated tones, Studio One, or driver DLL.
// --public with NO private files expects SILENCE, not a claimed audio pass.
// --private with original ROM + Creative SBK + owner SF2s tests REAL AUDIO.
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
struct Saved {uint32 magic=0x39303050u,version=1u,program=0u,bank=0u;float master=1.f;};
static_assert(sizeof(Saved)==20,"P009 state format changed");
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
