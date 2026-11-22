// P207 genuine original 86Box EMU8000 original register port witness.
// Original Creative Win95 SBAWE.VXD +6F56 pitch bend uses unsigned
// over-limit check before signed check. A negative summed IP saturates
// to FFFF, whereas old native signed std::clamp silently wrote zero.
// Public synthetic SF2 data only. Test-only legacy source must fail
// specifically for negative pitch saturation, not an unrelated failure.
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
struct Ip { unsigned voice; unsigned pitch; };
void need(bool ok,const char*msg){if(!ok)throw std::runtime_error(msg);}
std::vector<Ip> parseIp(const std::string&path){
    std::ifstream f(path);
    need(bool(f),"P207 original chip trace not created");
    std::string line;
    unsigned sel=0;
    std::vector<Ip> ip;
    while(std::getline(f,line)){
        if(line.empty()||line[0]=='#'||line.rfind("R ",0)==0)continue;
        unsigned long long frame=0;
        unsigned port=0,value=0;
        if(std::sscanf(line.c_str(),"%llu %x %x",&frame,&port,&value)!=3)
            throw std::runtime_error("P207 original chip trace malformed: "+line);
        if(port==0xe22u){sel=value;continue;}
        // Genuine EMU8000 original IP writes: E22 selector == voice,
        // E20 16-bit DATA3. IFATN is selector 0x20|voice, excluded.
        if(port==0xe20u && sel<32u)
            ip.push_back({sel,value});
    }
    return ip;
}
template <typename Action>
std::vector<Ip> record(Synth&s,const std::string&name,Action op){
    const auto path="_p207-"+name+".trace";
    need(s.Core().OpenTrace(path.c_str()),"P207 original chip trace open failed");
    op();
    s.Core().CloseTrace();
    auto got=parseIp(path);
    std::remove(path.c_str());
    return got;
}
void config(Synth&s,const char*rom,const char*sf2){
    std::string error;
    need(s.LoadWaveRom(rom,error),"P207 public ROM invalid");
    s.Core().SetChipRamKb(28672);
    need(s.Core().UseBox86Chip(rom,error),"P207 original 86Box C chip absent");
    s.Core().SetDriver(Awe32::Driver::Win95);
    need(s.LoadBank(sf2,error,false,1),"P207 public SoundFont invalid");
    auto* p=s.Core().ChipRam();
    need(p!=nullptr,"P207 original 28 MiB chip RAM missing");
    const auto words=std::min(s.Core().DramSize(),s.Core().ChipRamWords());
    std::memcpy(p,s.Core().DramData(),words*sizeof(int16_t));
    s.Core().PowerOnInit();
    s.ControlChange(0,0,1);
    s.ProgramChange(0,0);
}
std::vector<Ip> coldNote(const char*rom,const char*sf2,
                         bool bendBefore,const std::string&name){
    Synth s(44100);
    config(s,rom,sf2);
    if(bendBefore){
        s.ControlChange(0,101,0);
        s.ControlChange(0,100,0);
        s.ControlChange(0,6,127);
        s.PitchBend(0,-8192);
    }
    auto v=record(s,name,[&]{s.NoteOn(0,0,105);});
    need(v.size()==3u,"P207 public SF2 must start exactly three chip voices");
    std::set<unsigned> ids;
    for(auto i:v)ids.insert(i.voice);
    need(ids.size()==3u,"P207 cold note physical allocation duplicated voices");
    return v;
}
void match(const std::vector<Ip>&actual,const std::vector<Ip>&base,
           int delta,const char*context){
    need(actual.size()==base.size(),"P207 original chip missed physical IP writes");
    // Original NoteOn worklist is stereo-partner-first, then independent
    // mono; original pitch-wheel handler walks PHYSICAL voices ascending.
    // Never require those distinct, source-confirmed I/O orderings to
    // match. Compare uniquely owned voice identities AND IP per voice.
    auto sortedActual=actual,sortedBase=base;
    const auto byVoice=[](const Ip&a,const Ip&b){return a.voice<b.voice;};
    std::sort(sortedActual.begin(),sortedActual.end(),byVoice);
    std::sort(sortedBase.begin(),sortedBase.end(),byVoice);
    for(std::size_t i=0;i<sortedBase.size();++i){
        const auto& a=sortedActual[i];
        const auto& b=sortedBase[i];
        need(i==0u || (b.voice!=sortedBase[i-1].voice &&
                        a.voice!=sortedActual[i-1].voice),
             "P207 original chip duplicated physical IP register writes");
        need(a.voice==b.voice,
             "P207 original chip physical voice identity changed");
        const std::int64_t sum=static_cast<std::int64_t>(b.pitch)+delta;
        // Original x86 compares unsigned DWORD to 0xffff before sign.
        const unsigned want=(sum<0 || sum>65535)?65535u:
                             static_cast<unsigned>(sum);
        if(a.pitch!=want)
            throw std::runtime_error(std::string(context)+
               " voice="+std::to_string(b.voice)+
               " original="+std::to_string(b.pitch)+
               " expected="+std::to_string(want)+
               " actual="+std::to_string(a.pitch));
    }
}
void run(const char*rom,const char*sf2,const std::string&name){
    auto baseline=coldNote(rom,sf2,false,name+"-baseline");
    // With root C4 and played MIDI key C-1, all three original
    // sample-IP values must be below the -127st RPN bend magnitude.
    constexpr int delta=(-8192*127)/24;
    need(delta<0,"P207 RPN arithmetic fixture corrupted");
    for(auto b:baseline)
        need(static_cast<int>(b.pitch)+delta<0,
             "P207 fixture did not cross original unsigned-wrap boundary");
    Synth s(44100);
    config(s,rom,sf2);
    const auto on=record(s,name+"-live-note",[&]{s.NoteOn(0,0,105);});
    match(on,baseline,0,"P207 cold original IP mismatch");
    s.ControlChange(0,101,0);
    s.ControlChange(0,100,0);
    s.ControlChange(0,6,127);
    auto down=record(s,name+"-live-min-bend",[&]{s.PitchBend(0,-8192);});
    match(down,baseline,delta,"P207 VXD negative pitch must saturate to FFFF");
    auto centered=record(s,name+"-live-center",[&]{s.PitchBend(0,0);});
    match(centered,baseline,0,"P207 center bend must restore original IP");
    // Distinct code path in StartVoice: the bend is already in effect
    // BEFORE new note-on. Must match live bend and original VXD.
    auto prebent=coldNote(rom,sf2,true,name+"-before-note");
    match(prebent,baseline,delta,
          "P207 VXD note started under negative bend must saturate to FFFF");
    std::cout<<"P207_ORIGINAL_IP_CASE "<<name
             <<" negative_bend="<<delta
             <<" voices="<<baseline.size()<<" PASS\n";
}
}
int main(int argc,char**argv){
    if(argc!=5)return 2;
    try{
        run(argv[1],argv[2],"THREE_MONO");
        run(argv[1],argv[3],"MONO_FIRST_STEREO");
        run(argv[1],argv[4],"RIGHT_FIRST_STEREO");
        std::puts("P207_ORIGINAL_86BOX_CREATIVE_SIGNED_BEND_UNSIGNED_SAT_SIX_PATHS_PASS");
        return 0;
    }catch(const std::exception&e){
        std::cerr<<"P207_ORIGINAL_WIN95_PITCH_NO_GO "<<e.what()<<"\n";
        return 1;
    }
}
