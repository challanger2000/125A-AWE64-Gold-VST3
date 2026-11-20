// P021: Prevent a non-44.1k VST3 from silently playing the SECOND chip model.
// Measure genuine 86Box original chip PCM at 44100 and independently compare
// 8/48/96/192k output with linear resampling of that exact original reference.
// Deliberately uses actual Synth, pinned original 86Box snd_emu8k.c and the
// reconstructed Creative Win95 driver; not the synth's secondary DSP model.
// Public: fake.wavROM public.sf2; private: awe32.raw SYNTHGM.SBK owner.sf2.
// Only the owner's ROM can prove private original-bank output.
#include "Synth.h"
#include "Awe32Driver.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr uint32_t kNative=44100,kHostFrames=8192,kReferenceFrames=49152;
void require(bool okay,const char* message){
  if(!okay) throw std::runtime_error(message);
}
std::vector<int16_t> capture(uint32_t rate,const char* rom,
                             const char* gm,const char* sf2,
                             int program,bool effects,uint32_t count) {
  Synth s(rate);
  std::string err;
  require(s.LoadWaveRom(rom,err),"P021 ROM cannot load");
  s.Core().SetChipRamKb(28672);
  require(s.Core().UseBox86Chip(rom,err),"P021 BOX86 EMU8000 not active");
  s.Core().SetDriver(Awe32::Driver::Win95);
  if(gm)require(s.LoadBank(gm,err,false,-1),"P021 Creative SBK cannot load");
  require(s.LoadBank(sf2,err,false,1),"P021 SF2 cannot load");
  require(s.Core().ChipRam()!=nullptr,"P021 no original chip DRAM");
  const auto words=std::min(s.Core().DramSize(),s.Core().ChipRamWords());
  std::memcpy(s.Core().ChipRam(),s.Core().DramData(),
              words*sizeof(int16_t));
  s.Core().PowerOnInit();
  require(s.PrepareRealtime(1024),"P021 original chip failed RT preparation");
  s.ControlChange(0,0,1);
  s.ProgramChange(0,static_cast<uint8_t>(program));
  s.ControlChange(0,7,100);
  s.ControlChange(0,91,effects?85:0);
  s.ControlChange(0,93,effects?72:0);
  s.NoteOn(0,60,110);
  std::vector<int16_t> result;
  result.reserve(static_cast<size_t>(count)*2);
  std::array<int16_t,2048> block{};
  for(uint32_t i=0;i<count;i+=1024){
    const auto n=std::min<uint32_t>(1024,count-i);
    s.RenderBlock(block.data(),n);
    result.insert(result.end(),block.begin(),block.begin()+n*2);
  }
  require(result.size()==static_cast<size_t>(count)*2,
          "P021 invalid output length");
  return result;
}
int16_t linear(const std::vector<int16_t>& native,double q,size_t channel){
  const size_t i0=static_cast<size_t>(q);
  const float f=static_cast<float>(q-i0);
  const float s0=static_cast<float>(native[2*i0+channel]);
  const float s1=static_cast<float>(native[2*(i0+1)+channel]);
  const float v=s0+(s1-s0)*f;
  return static_cast<int16_t>(std::clamp(
      std::lround(static_cast<double>(v)),-32768L,32767L));
}
void compare(const std::vector<int16_t>& native,
             const std::vector<int16_t>& other,uint32_t rate,bool effects){
  const double ratio=static_cast<double>(kNative)/rate;
  uint64_t changed=0,near=0;
  int maxErr=0;
  double signal=0,errors=0;
  for(uint32_t i=0;i<kHostFrames;++i){
    const double pos=ratio*i;
    require(2*(static_cast<size_t>(pos)+1)+1<native.size(),
            "P021 reference too short");
    for(size_t c=0;c<2;++c){
      const int expected=linear(native,pos,c);
      const int got=other[2*i+c];
      const int error=std::abs(expected-got);
      if(error)++changed;
      if(error<=1)++near;
      maxErr=std::max(maxErr,error);
      signal+=double(expected)*expected;
      errors+=double(error)*error;
    }
  }
  require(signal>100000000.,"P021 original chip PCM is silent");
  const double rms=std::sqrt(errors/(kHostFrames*2.));
  std::printf("P021_ORIGINAL_BOX86_CHIP_RESAMPLER_"
      "%s rate=%u host_frames=%u stereo_values=%u"
      " different=%llu within1lsb=%llu max_error_lsb=%d rms_error_lsb=%.8f\n",
      effects?"FX":"DRY",rate,kHostFrames,kHostFrames*2,
      static_cast<unsigned long long>(changed),
      static_cast<unsigned long long>(near),maxErr,rms);
  require(maxErr<=1 && near==kHostFrames*2,
      "P021 NON-NATIVE RATE DOES NOT MATCH ORIGINAL EMU8000: SECONDARY CHIP DSP");
}
}
int main(int argc,char** argv){
 try{
  require(argc==3||argc==4,
          "Usage: P021OriginalChipResamplingProbe.exe rom [SYNTHGM.SBK] sf2");
  const bool privateBank=argc==4;
  const char* rom=argv[1];
  const char* gm=privateBank?argv[2]:nullptr;
  const char* sf2=argv[privateBank?3:2];
  const int program=privateBank?7:0;
  for(const bool effects:{false,true}){
    const auto native=capture(kNative,rom,gm,sf2,program,effects,
                              kReferenceFrames);
    for(const uint32_t rate:{8000u,48000u,96000u,192000u}){
       const auto other=capture(rate,rom,gm,sf2,program,effects,kHostFrames);
       compare(native,other,rate,effects);
    }
  }
  std::puts("P021_ALL_RATES_GENUINE_86BOX_EMU8000_PCM_FROM_NATIVE_44100_PASS_8_OF_8");
  return 0;
 }catch(const std::exception& e){
  std::fprintf(stderr,"P021_ORIGINAL_EMU8000_NON_NATIVE_RATE_FAIL %s\n",e.what());
  return 1;
 }
}
