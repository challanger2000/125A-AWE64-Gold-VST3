// P200 actual native Win64 C++ driver/86Box-chip port trace order.
// Independent owner original-Win95 traces recorded 25 stereo linked paired
// starts: main(voiceA),main(voiceB),triggerA,triggerB. A mono pair remains
// mainA,triggerA,mainB,triggerB. A PCM-only test misses this difference.
// Synthetic public SF2/ROM only. No private original data is embedded.
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
struct Mark {int voice=0;std::size_t index=0;};
struct Trace {std::vector<Mark> mains;std::vector<Mark> starts;};
void check(bool ok,const char* msg) {if(!ok)throw std::runtime_error(msg);}
Trace parse(const std::string& file) {
    std::ifstream f(file);
    check(bool(f),"P200 expected actual original-engine port trace missing");
    std::string line;
    std::uint16_t pointer=0;
    std::size_t index=0;
    Trace t;
    while(std::getline(f,line)) {
        if(line.empty() || line[0]=='#')continue;
        // The authentic pinned core records all IN operations as
        // "R <frame> <port> <value>"; never mistake these for writes.
        // Reject any other unknown record rather than silently skipping it.
        if(line.rfind("R ",0)==0)continue;
        unsigned long long frame=0;
        unsigned port=0,value=0;
        if(std::sscanf(line.c_str(),"%llu %x %x",&frame,&port,&value)!=3)
            throw std::runtime_error("P200 invalid EMU8000 port trace tuple: "+line);
        ++index;
        if(port==0xE22){pointer=static_cast<std::uint16_t>(value);continue;}
        // CPF is the LAST main original Win95 voice register in StartVoice.
        // Data0 reg0, high word, its pointer is exactly voice (0..31).
        if(port==0x622 && pointer < 32 && value!=0)
            t.mains.push_back({static_cast<int>(pointer),index});
        // Data1 reg5 (DCYSUSV), only on/trigger values (off has bit 7).
        if(port==0xA20 && (pointer&0xE0)==0xA0 && !(value&0x80u))
            t.starts.push_back({static_cast<int>(pointer&31),index});
    }
    return t;
}
Trace run(const char* rom,const char* sf2,const char* stem) {
    Synth synth(44100);
    std::string error;
    check(synth.LoadWaveRom(rom,error),"P200 public ROM initialization");
    synth.Core().SetChipRamKb(28672);
    check(synth.Core().UseBox86Chip(rom,error),"P200 authentic chip init failed");
    synth.Core().SetDriver(Awe32::Driver::Win95);
    check(synth.LoadBank(sf2,error,false,1),"P200 SF2 bank didn't load");
    auto* ram=synth.Core().ChipRam();
    check(ram!=nullptr,"P200 original physical RAM missing");
    const auto count=std::min(synth.Core().DramSize(),synth.Core().ChipRamWords());
    std::memcpy(ram,synth.Core().DramData(),count*sizeof(std::int16_t));
    synth.Core().PowerOnInit();
    synth.ControlChange(0,0,1);
    synth.ProgramChange(0,0);
    const std::string logfile=std::string("_p200-")+stem+".trace";
    check(synth.Core().OpenTrace(logfile.c_str()),"P200 core trace not open");
    synth.NoteOn(0,60,102);
    synth.Core().CloseTrace();
    const auto t=parse(logfile);
    std::remove(logfile.c_str());
    return t;
}
void verdict(const Trace& t,bool stereo,const char* name){
    if(!(t.mains.size()==(stereo?2u:1u) ||
         (!stereo && t.mains.size()==2u)))
        throw std::runtime_error("P201 wrong nonzero chip CPF main count="
              +std::to_string(t.mains.size())+
              " starts="+std::to_string(t.starts.size()));
    check(t.starts.size()==t.mains.size(),"P200 main/trigger voice count mismatch");
    check(t.mains.size()>=1 && t.starts.size()>=1,"P200 no sound");
    std::set<int> mainVoices,startVoices;
    for(const auto& x:t.mains)mainVoices.insert(x.voice);
    for(const auto& x:t.starts)startVoices.insert(x.voice);
    check(mainVoices.size()==t.mains.size()&&startVoices==mainVoices,
          "P200 voice alias / trigger without main program");
    if(stereo) {
        check(t.mains.size()==2,"P200 reciprocal stereo must program TWO physical voices");
        const auto lastMain=std::max(t.mains[0].index,t.mains[1].index);
        check(t.starts[0].index>lastMain && t.starts[1].index>lastMain,
              "P200 original Creative stereo BOTH mains must precede EITHER trigger");
        check(t.starts[0].index<t.starts[1].index,
              "P200 paired trigger order reversed");
        const int rightBase=t.mains[1].voice;
        const int leftUpper=t.mains[0].voice;
        check(rightBase>=0 && rightBase<31 && leftUpper==rightBase+1,
              "P201 original Creative linked stereo voices MUST be adjacent");
        check(t.starts[0].voice==rightBase &&
              t.starts[1].voice==leftUpper,
              "P201 original Creative paired note triggers right(base) then left(base+1)");
    } else if(t.mains.size()==2) {
        check(t.mains[0].index < t.starts[0].index &&
              t.starts[0].index < t.mains[1].index &&
              t.mains[1].index < t.starts[1].index,
              "P200 independent mono layers must preserve immediate trigger");
    } else {
        check(t.mains[0].index < t.starts[0].index,
              "P200 single mono must trigger after main programming");
    }
    std::cout<<"P200_NATIVE_PORT_SEQUENCE "<<name
             <<" voices="<<t.mains.size()
             <<" first_main="<<t.mains[0].index
             <<" last_main="<<t.mains.back().index
             <<" first_trigger="<<t.starts[0].index
             <<" last_trigger="<<t.starts.back().index<<"\n";
}
}
int main(int argc,char** argv){
    if(argc!=6)return 2;
    try {
        verdict(run(argv[1],argv[2],"linked"),true,"STEREO_SINGLE_LINKED_ZONE");
        verdict(run(argv[1],argv[3],"explicit"),true,"STEREO_TWO_EXPLICIT_ZONES");
        verdict(run(argv[1],argv[4],"mono"),false,"MONO_SINGLE_ZONE");
        verdict(run(argv[1],argv[5],"mono2"),false,"MONO_TWO_INDEPENDENT_LAYERS");
        std::puts("P200_ORIGINAL_CREATIVE_STEREO_JOINT_TRIGGER_NATIVE_PORT_ORDER_4_CASES_PASS");
        std::puts("P201_ORIGINAL_CREATIVE_ADJACENT_STEREO_REVERSE_BASE_TRIGGER_4CASES_PASS");
        return 0;
    } catch(const std::exception& e){
        std::cerr<<"P200_ORIGINAL_CREATIVE_STEREO_NATIVE_TRACE_NO_GO "<<e.what()<<"\n";
        return 1;
    }
}
