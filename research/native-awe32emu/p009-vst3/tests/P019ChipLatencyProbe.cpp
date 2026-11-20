// P019: standalone ORIGINAL pinned Win95 Creative-driver/86Box EMU8000
// PCM output with test-only chip update windows 980 / 128 / 64.
// Never compile a shortened window into the production VST3.
// ROM is a PUBLIC all-zero fixture; SF2 is generated from public waveforms.
// This measures whether changing the 86Box render/update window preserves
// dry and FX PCM after aligning the well-understood one-block FIFO delay.
#include "Synth.h"
#include "Emu8000Box.h"
#include "Awe32Driver.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
constexpr unsigned kRate=44100, kBlocks=72, kFrames=1024;
void require(bool condition,const char* message){
    if(!condition)throw std::runtime_error(message);
}
std::uint64_t render(const char* rom,const char* sf2,
                     const char* output,bool fx){
    Synth synth(kRate);
    std::string err;
    require(synth.LoadWaveRom(rom,err),"P019 failed synthetic ROM");
    synth.Core().SetChipRamKb(28672);
    require(synth.Core().UseBox86Chip(rom,err),"P019 failed genuine chip");
    synth.Core().SetDriver(Awe32::Driver::Win95);
    require(synth.LoadBank(sf2,err,false,1),"P019 failed public SF2");
    auto* dst=synth.Core().ChipRam();
    require(dst,"P019 missing EMU8000 RAM");
    const auto words=std::min(synth.Core().DramSize(),
                              synth.Core().ChipRamWords());
    std::memcpy(dst,synth.Core().DramData(),words*sizeof(std::int16_t));
    synth.Core().PowerOnInit();
    require(synth.PrepareRealtime(kFrames),"P019 no RT scratch");
    synth.ControlChange(0,0,1); // public user SF2, NOT fallback ROM
    synth.ProgramChange(0,0);
    synth.ControlChange(0,7,100);
    synth.ControlChange(0,91,fx?83:0);
    synth.ControlChange(0,93,fx?71:0);
    std::ofstream pcm(output,std::ios::binary);
    require(bool(pcm),"P019 unable to create PCM");
    std::array<std::int16_t,kFrames*2> buf{};
    std::uint64_t hash=14695981039346656037ULL;
    std::uint64_t energy=0;
    for(unsigned b=0;b<kBlocks;++b){
        if(b==0)synth.NoteOn(0,60,100);
        if(b==5)synth.ControlChange(0,1,96);
        if(b==10)synth.PitchBend(0,2050);
        if(b==15)synth.NoteOff(0,60);
        if(b==18)synth.NoteOn(0,64,75);
        if(b==30)synth.NoteOff(0,64);
        if(b==33)synth.PitchBend(0,0);
        if(b==36)synth.NoteOn(0,67,125);
        if(b==54)synth.NoteOff(0,67);
        synth.RenderBlock(buf.data(),kFrames);
        pcm.write(reinterpret_cast<const char*>(buf.data()),
                  static_cast<std::streamsize>(buf.size()*sizeof(std::int16_t)));
        for(const auto v:buf){
            hash=(hash^static_cast<std::uint16_t>(v))*1099511628211ULL;
            const auto sv=static_cast<std::int64_t>(v);
            energy+=static_cast<std::uint64_t>(sv*sv);
        }
    }
    require(bool(pcm),"P019 unable to write PCM");
    require(energy>100000000ULL,"P019 silent real EMU8000 test");
    return hash;
}
}
int main(int argc,char** argv){
    try{
        require(argc==5,"Usage: P019Probe<N>.exe fake.raw generated.sf2 dry|fx out.pcm");
        const bool fx=std::strcmp(argv[3],"fx")==0;
        require(fx||std::strcmp(argv[3],"dry")==0,"P019 mode must be dry|fx");
        const auto h=render(argv[1],argv[2],argv[4],fx);
        std::printf("P019_REAL_EMU8000_SYNTHETIC_SF2_WINDOW_PCM_OK"
                    " block=%d rate=%u mode=%s frames=%u hash=%016llX\n",
            Emu8000Box::kBlockFrames,kRate,fx?"fx":"dry",kBlocks*kFrames,
            static_cast<unsigned long long>(h));
        return 0;
    }catch(const std::exception& e){
        std::fprintf(stderr,"P019_CHIP_WINDOW_QA_FAIL %s\n",e.what());
        return 1;
    }
}
