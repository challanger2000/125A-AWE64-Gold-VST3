// P206: actual original 86Box C EMU8000 chip + SF2, captured hardware
// port chronology for Win95 Creative VXD CC120 and CC123 sustain handling.
// Original independently reconstructed VXD: LCOD+0x6C82.
#include "Synth.h"
#include "Awe32Driver.h"
#include "Emu8000Regs.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Hit { unsigned voice=0, value=0; };
struct Log {
    std::vector<Hit> volRelease,modRelease,volStop;
    std::size_t portWords=0;
};
void require(bool ok,const char*why) {
    if(!ok) throw std::runtime_error(why);
}
Log parse(const std::string&path) {
    std::ifstream f(path);
    require(bool(f),"original hardware trace missing");
    Log out;
    unsigned selector=0;
    std::string line;
    while(std::getline(f,line)){
        if(line.empty() || line[0]=='#' || line.rfind("R ",0)==0) continue;
        unsigned long long frame=0;
        unsigned port=0,value=0;
        if(std::sscanf(line.c_str(),"%llu %x %x",&frame,&port,&value)!=3)
            throw std::runtime_error("P206 malformed original chip port trace: "+line);
        if(port==0xe22){selector=value;continue;}
        ++out.portWords;
        if(port!=0xa20)continue;
        const auto voice=selector&31u;
        if((selector&0xe0u)==0xa0u){
            if((value&0x8000u)!=0u)out.volRelease.push_back({voice,value});
            else out.volStop.push_back({voice,value});
        }
        if((selector&0xe0u)==0xe0u && (value&0x8000u)!=0u)
            out.modRelease.push_back({voice,value});
    }
    return out;
}
template <typename Fn>
Log record(Synth& s,const std::string&tag,Fn action){
    const auto file="_p206-"+tag+".trace";
    require(s.Core().OpenTrace(file.c_str()),"original chip tracing unavailable");
    action();
    s.Core().CloseTrace();
    auto result=parse(file);
    std::remove(file.c_str());
    return result;
}
void checkReleases(const Log&log,std::size_t count,const char*why){
    require(log.volRelease.size()==count && log.modRelease.size()==count,why);
    require(log.volStop.empty(),"P206 original channel release hard-muted voices");
    std::set<unsigned> vols,mods;
    for(auto x:log.volRelease)vols.insert(x.voice);
    for(auto x:log.modRelease)mods.insert(x.voice);
    require(vols.size()==count && vols==mods,
            "P206 original CC voice release duplicated/omitted physical voice");
}
void run(const char*rom,const char*sf2,const char*layout,
         int variant) {
    Synth s(44100);
    std::string error;
    require(s.LoadWaveRom(rom,error),"P206 public ROM init failed");
    s.Core().SetChipRamKb(28672);
    require(s.Core().UseBox86Chip(rom,error),
            "P206 original 86Box chip did not initialize");
    s.Core().SetDriver(Awe32::Driver::Win95);
    require(s.LoadBank(sf2,error,false,1),
            "P206 public SF2 bank load failed");
    auto* physical=s.Core().ChipRam();
    require(physical!=nullptr,"P206 original 28MiB chip RAM absent");
    const auto words=std::min(s.Core().DramSize(),s.Core().ChipRamWords());
    std::memcpy(physical,s.Core().DramData(),words*sizeof(int16_t));
    s.Core().PowerOnInit();
    s.ControlChange(0,0,1);
    s.ProgramChange(0,0);
    s.NoteOn(0,60,102); // 3 valid mono or stereo+mono SF2 work voices
    if(variant==0){
        s.ControlChange(0,64,127);
        auto silent=record(s,std::string(layout)+"-cc123-defer", [&]{
            s.ControlChange(0,123,0);
        });
        require(silent.volRelease.empty() && silent.modRelease.empty()
                && silent.volStop.empty(),
            "P206 original CC123 sustain must not write a release");
        std::vector<int16_t>pcm(4096*2);
        s.RenderBlock(pcm.data(),4096);
        require(std::any_of(pcm.begin(),pcm.end(),[](int16_t v){return v!=0;}),
            "P206 pedal-held original chip rendered silence");
        auto released=record(s,std::string(layout)+"-pedal-release", [&]{
            s.ControlChange(0,64,0);
        });
        checkReleases(released,3u,
            "P206 original CC123 pedal OFF must release three layers");
        auto noSecond=record(s,std::string(layout)+"-pedal-off-again", [&]{
            s.ControlChange(0,64,0);
        });
        require(noSecond.volRelease.empty() && noSecond.modRelease.empty(),
            "P206 released pedal wrote duplicate voice release");
    } else if(variant==1) {
        s.ControlChange(0,64,127);
        auto release=record(s,std::string(layout)+"-cc120", [&]{
            s.ControlChange(0,120,0);
        });
        checkReleases(release,3u,
            "P206 original CC120 must release both envelopes, NOT hard-stop");
        auto noSecond=record(s,std::string(layout)+"-cc120-repeat", [&]{
            s.ControlChange(0,120,0);
            s.ControlChange(0,64,0);
        });
        require(noSecond.volRelease.empty() && noSecond.modRelease.empty()
                && noSecond.volStop.empty(),
            "P206 CC120 repeated release and/or pedal off touched idle voice");
    } else {
        auto release=record(s,std::string(layout)+"-cc123-no-pedal", [&]{
            s.ControlChange(0,123,0);
        });
        checkReleases(release,3u,
            "P206 original CC123 pedal OFF must release both envelopes");
    }
    std::cout<<"P206_ORIGINAL_CHIP_CC_CASE "<<layout
        <<" variant="<<variant<<" PASS\n";
}
}
int main(int argc,char**argv){
    if(argc!=5)return 2;
    try{
        const char*names[]={"THREE_MONO","MONO_FIRST_STEREO","RIGHT_FIRST_STEREO"};
        for(int i=0;i<3;i++)
            for(int variant=0;variant<3;variant++)
                run(argv[1],argv[2+i],names[i],variant);
        std::puts("P206_ORIGINAL_86BOX_CHIP_CC120_CC123_SUSTAIN_NINE_CASES_PASS");
        return 0;
    }catch(const std::exception&e){
        std::cerr<<"P206_WIN95_ORIGINAL_CC_NO_GO "<<e.what()<<"\n";
        return 1;
    }
}
