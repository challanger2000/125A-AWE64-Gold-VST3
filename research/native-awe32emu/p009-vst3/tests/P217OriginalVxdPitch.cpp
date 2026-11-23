// P217: PUBLIC synthetic SF2 generator-only ORIGINAL Creative x86 pitch.
#include "SoundFont.h"
#include "Emu8000Regs.h"
#include <cstdio>
#include <stdexcept>
#include <utility>
#include <vector>
namespace {
void require(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
SoundFont::Bank make(bool sf1=false,unsigned rate=44100u){
 using namespace SoundFont;
 Bank b;b.valid=true;b.version=sf1?Version::Sf1:Version::Sf2;
 b.name="PUBLIC synthetic fixed point";
 Sample sample;sample.name="synthetic";
 sample.start=256;sample.end=8192;sample.loopStart=512;sample.loopEnd=6144;
 sample.sampleRate=rate;sample.originalKey=60;
 b.samples.push_back(sample);
 Instrument i;i.name="public synthetic voice";
 i.global.Set(Gen::OverridingRootKey,72);
 i.global.Set(Gen::ScaleTuning,100);
 for(int n=0;n<4;++n){
   Zone z;z.sampleId=0;
   z.gen.Set(Gen::FineTune,n<2?-6:-5);
   z.gen.Set(Gen::CoarseTune,n%2==0?0:12);
   i.zones.push_back(z);
 }
 b.instruments.push_back(std::move(i));
 Preset p;p.name="public program";p.bank=1;p.program=7;
 Zone z;z.instrument=0;p.zones.push_back(z);
 b.presets.push_back(std::move(p));return b;
}
}
int main(){
  try{
   const auto b=make();
   const auto zones=b.Select(1,7,60,110);
   require(zones.size()==4,"P217 original SF2 four independent notes not selected");
   const unsigned orig[4]={0xCFEA,0xDFEB,0xDFEE,0xCFEE};
   const unsigned legacy[4]={0xCFEB,0xDFEB,0xDFEE,0xCFEE};
   for(int i=0;i<4;++i){
     const auto win=SoundFont::MakeVoiceParams(
       b,zones[i],60,110,0x200010u,0,Awe32::Driver::Win95);
     const auto dos=SoundFont::MakeVoiceParams(
       b,zones[i],60,110,0x200010u,0,Awe32::Driver::Dos);
     require(win.ip==orig[i],"P217_NO_GO Win95 SF2 original x86 0x36A/SAR8 pitch mismatch");
     require(dos.ip==legacy[i],"P217_NO_GO DOS SF2 pitch conversion was altered");
   }
   const auto half=make(false,22050u);
   const auto other=half.Select(1,7,60,110);
   require(other.size()==4,"P217 alternate sampling-rate fixture missing");
   const auto prior=SoundFont::MakeVoiceParams(
      half,other[0],60,110,0x200010u,0,Awe32::Driver::Win95);
   require(prior.ip==SoundFont::PitchToIp(-2406),
      "P217 changed unverified 22.05k SF2 original driver path");
   const auto old=make(true);
   const auto r=old.Select(1,7,60,110);
   require(r.size()==4,"P217 original SF1 synthetic regions missing");
   require(SoundFont::MakeVoiceParams(
      old,r[0],60,110,0x200010u,0,Awe32::Driver::Win95).ip==legacy[0],
      "P217 original Creative SF1/SBK ROM IP path unexpectedly altered");
   std::puts("P217_WIN95_SF2_ORIGINAL_X86_874_SAR8_FOUR_VOICE_IP_WITH_SF1_DOS_RATE_ISOLATION_PASS");
   return 0;
  }catch(const std::exception&e){std::fprintf(stderr,"P217_NO_GO %s\n",e.what());return 1;}
}
