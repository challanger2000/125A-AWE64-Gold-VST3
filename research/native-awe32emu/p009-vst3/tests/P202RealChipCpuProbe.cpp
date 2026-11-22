// P202: real pinned Synth/EMU8000 chip, paired pre-fix and optimized executables.
// SYNTHETIC ROM and SF2 only. Timing is unpaced offline Windows CI, NOT ASIO.
#include "Synth.h"
#include "Awe32Driver.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using Clock=std::chrono::steady_clock;
namespace {
void need(bool okay,const char*msg){if(!okay)throw std::runtime_error(msg);}
std::string hexHash(std::uint64_t value){
    std::ostringstream s;
    s<<std::hex<<std::setw(16)<<std::setfill('0')<<value;
    return s.str();
}
struct Case {
    std::uint32_t rate,block,notes,samples,overruns;
    std::uint64_t hash,energy;
    double mean,p95,p99,maximum,deadline;
};
double clockOverheadNs(){
    std::vector<double> samples;
    samples.reserve(4096);
    for(unsigned i=0;i<4096;i++){
        auto a=Clock::now(),b=Clock::now();
        samples.push_back(std::chrono::duration<double,std::nano>(b-a).count());
    }
    std::sort(samples.begin(),samples.end());
    return samples[samples.size()/2];
}
Case measure(const char*rom,const char*sf2,std::uint32_t rate,
             std::uint32_t block,std::uint32_t voices,bool mutant){
    Synth synth(rate);
    std::string error;
    need(synth.LoadWaveRom(rom,error),"ROM init");
    synth.Core().SetChipRamKb(28672);
    need(synth.Core().UseBox86Chip(rom,error),"Original 86Box EMU8000 missing");
    synth.Core().SetDriver(Awe32::Driver::Win95);
    need(synth.LoadBank(sf2,error,false,1),"Public sustained SF2 load");
    auto* physical=synth.Core().ChipRam();
    need(physical!=nullptr,"Original chip RAM missing");
    const size_t words=std::min(synth.Core().DramSize(),synth.Core().ChipRamWords());
    std::memcpy(physical,synth.Core().DramData(),words*sizeof(std::int16_t));
    synth.Core().PowerOnInit();
    need(synth.PrepareRealtime(1024),"Real-time scratch preallocation");
    synth.ControlChange(0,0,1);
    synth.ProgramChange(0,0);
    // The public test SF2 has exactly TWO independent layers per note.
    for(std::uint32_t i=0;i<voices/2;i++)
        synth.NoteOn(0,static_cast<std::uint8_t>(48+i),110);
    std::vector<std::int16_t> buf(block*2);
    for(unsigned i=0;i<45;i++)synth.RenderBlock(buf.data(),block);
    constexpr unsigned count=320;
    std::vector<double> durations;
    durations.reserve(count);
    Case r{};
    r.rate=rate;r.block=block;r.notes=voices/2;
    r.samples=count*block;r.hash=14695981039346656037ull;
    r.deadline=1.e6*block/double(rate);
    double sum=0;
    for(unsigned i=0;i<count;i++){
        const auto a=Clock::now();
        synth.RenderBlock(buf.data(),block);
        const auto b=Clock::now();
        const auto usec=std::chrono::duration<double,std::micro>(b-a).count();
        durations.push_back(usec);
        sum+=usec;
        if(usec>r.deadline)r.overruns++;
        if(mutant&&i==0)buf[0]^=std::int16_t{1}; // TEST-ONLY bad PCM
        for(const auto v:buf){
            r.hash=(r.hash^static_cast<std::uint16_t>(v))*1099511628211ull;
            r.energy+=static_cast<std::uint64_t>(std::int64_t{v}*v);
        }
    }
    std::sort(durations.begin(),durations.end());
    r.mean=sum/count;
    r.p95=durations[static_cast<size_t>(std::ceil(.95*count))-1];
    r.p99=durations[static_cast<size_t>(std::ceil(.99*count))-1];
    r.maximum=durations.back();
    need(r.energy>0,"zero chip audio: test cannot measure sound");
    return r;
}
void emit(Case const&r){
    std::cout<<"{\"rate\":"<<r.rate<<",\"block\":"<<r.block
        <<",\"notes\":"<<r.notes<<",\"samples\":"<<r.samples
        <<",\"hash\":\""<<hexHash(r.hash)<<"\",\"energy\":"<<r.energy
        <<",\"mean_us\":"<<r.mean<<",\"p95_us\":"<<r.p95
        <<",\"p99_us\":"<<r.p99<<",\"max_us\":"<<r.maximum
        <<",\"deadline_us\":"<<r.deadline<<",\"overruns\":"<<r.overruns<<"}";
}
}
int main(int argc,char**argv){
    if(argc<3||argc>4)return 2;
    try{
        const bool mutant=argc==4&&std::string(argv[3])=="--mutate-audio";
        need(argc==3||mutant,"unknown P202 test flag");
        std::cout<<std::fixed<<std::setprecision(3);
        std::cout<<"{\"timer_overhead_ns\":"<<clockOverheadNs()<<",\"cases\":[";
        bool first=true;
        for(std::uint32_t rate:{44100u,48000u})
            for(std::uint32_t block:{64u,256u})
                for(std::uint32_t voices:{2u,32u}){
                    const auto c=measure(argv[1],argv[2],rate,block,voices,mutant);
                    if(!first)std::cout<<",";
                    first=false;emit(c);
                }
        std::cout<<"]}\n";
        return 0;
    }catch(const std::exception&e){
        std::cerr<<"P202_REAL_CHIP_CPU_PCM_FAIL "<<e.what()<<"\n";
        return 1;
    }
}
