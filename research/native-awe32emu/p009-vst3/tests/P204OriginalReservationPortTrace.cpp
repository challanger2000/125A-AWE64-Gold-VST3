// P204 original source-based native Windows C++ actual 86Box EMU8000 ports.
// 3-layer SF2: prove every allocator stop before FIRST voice register body.
// SF2 stereo must be planned before mono regardless of zone order.
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
struct Entry {int voice;std::size_t at;};
struct Trace {
    std::vector<Entry> reserves,mainStarts,mains,triggers,vtftResets;
    int nonzero=0;
};
void must(bool yes,const char*why){if(!yes)throw std::runtime_error(why);}
Trace capture(const char*rom,const char*sf2,const char*tag){
    Synth synth(44100);
    std::string error;
    must(synth.LoadWaveRom(rom,error),"P204 synthetic ROM invalid");
    synth.Core().SetChipRamKb(28672);
    must(synth.Core().UseBox86Chip(rom,error),"P204 genuine 86Box chip failed");
    synth.Core().SetDriver(Awe32::Driver::Win95);
    must(synth.LoadBank(sf2,error,false,1),"P204 valid public SF2 import failed");
    auto*ram=synth.Core().ChipRam();
    must(ram!=nullptr,"P204 original chip RAM unavailable");
    const auto words=std::min(synth.Core().DramSize(),synth.Core().ChipRamWords());
    std::memcpy(ram,synth.Core().DramData(),words*sizeof(std::int16_t));
    synth.Core().PowerOnInit();
    synth.ControlChange(0,0,1);
    synth.ProgramChange(0,0);
    const std::string name=std::string("_p204-native-")+tag+".trace";
    must(synth.Core().OpenTrace(name.c_str()),"P204 genuine chip trace failed");
    synth.NoteOn(0,60,102);
    synth.Core().CloseTrace();
    std::ifstream f(name);
    must(bool(f),"P204 original chip trace missing");
    std::string line;
    unsigned sel=0;
    std::size_t at=0;
    Trace t;
    while(std::getline(f,line)){
        if(line.empty()||line[0]=='#'||line.rfind("R ",0)==0)continue;
        unsigned long long frame=0;
        unsigned port=0,value=0;
        if(std::sscanf(line.c_str(),"%llu %x %x",&frame,&port,&value)!=3)
            throw std::runtime_error("P204 invalid real-chip port tuple: "+line);
        ++at;
        if(port==0xE22){sel=value;continue;}
        // Original DATA0 VTFT port and E22=(3<<5)|physical voice.
        if(port==0x620 && (sel&0xE0)==0x60 && value==0x0000FFFFu)
            t.vtftResets.push_back({static_cast<int>(sel&31),at});
        if(port==0xA20 && (sel&0xE0)==0xA0){
            if(value==0x00FFu)t.reserves.push_back({static_cast<int>(sel&31),at});
            else if((value&0x80u)==0u)t.triggers.push_back({static_cast<int>(sel&31),at});
        }
        // The first genuine voice-main IFATN write is on DATA3, selector
        // (1<<5)|voice, port E20. Unlike CPF-last, this pins the start
        // of programming independently of the later main-body operations.
        if(port==0xE20 && (sel&0xE0)==0x20)
            t.mainStarts.push_back({static_cast<int>(sel&31),at});
        if(port==0x622 && sel<32 && value!=0)
            t.mains.push_back({static_cast<int>(sel),at});
    }
    f.close();std::remove(name.c_str());
    std::vector<int16_t>pcm(8192*2);
    synth.RenderBlock(pcm.data(),8192);
    for(const auto v:pcm)if(v!=0)++t.nonzero;
    must(t.nonzero>64,"P204 genuine original chip must render nonzero PCM");
    return t;
}
void validate(const Trace&t,const char*name,bool stereo){
    // Source has exactly one 0x00FF allocator stop PER voice, and
    // StartVoice begins each main with a SECOND 0x00FF silencing write.
    // Do not mistake 6 real port writes for 6 allocated hardware voices.
    must(t.reserves.size()==6 && t.mainStarts.size()==3 &&
         t.mains.size()==3 && t.triggers.size()==3,
        "P204 expected 3 original allocator stops plus 3 main stops, IFATN and triggers");
    std::set<int>reserved,played,triggered;
    // The first three 00FF writes must reserve three DISTINCT physical
    // voices before any actual main IFATN write. The later three are
    // the original VXD's own per-voice main-silencing operations.
    for(int i=0;i<3;++i)reserved.insert(t.reserves[i].voice);
    for(const auto&v:t.mains)played.insert(v.voice);
    for(const auto&v:t.triggers)triggered.insert(v.voice);
    // This is the PRIMARY causal test: old interleaved source has its
    // first main IFATN before the third DCYSUSV allocator reservation.
    // Check chronology before secondary voice-identity assertions so
    // the intentionally broken old-order executable fails for exactly
    // the advertised source defect, never because of incidental aliasing.
    must(t.reserves[2].at < t.mainStarts.front().at,
        "P204 VXD all 3 reservation prologues did not precede first main IFATN");
    must(reserved.size()==3 && played==reserved && triggered==reserved,
        "P204 duplicate/absent reserved voice or hardware start");
#ifdef P205_VERIFY_ALLOC_RESET
    // Fourth allocator-stop-looking write is really StartVoice main
    // stop. The first three are distinct allocations. Original VXD
    // performs one DATA0 VTFT reset per voice before that fourth stop.
    std::set<int> resetVoices;
    int resetCount=0;
    for(const auto& v:t.vtftResets)
        if(v.at<t.reserves[3].at){
            ++resetCount;
            resetVoices.insert(v.voice);
            bool preceded=false;
            for(int j=0;j<3;j++)
                if(t.reserves[j].voice==v.voice &&
                   t.reserves[j].at<v.at)preceded=true;
            must(preceded,"P205 VTFT reset before own allocator stop");
        }
    must(resetCount==3 && resetVoices==reserved,
         "P205 original allocator VTFT reset missing before first main stop");
#endif
    if(stereo){
        const int left=t.mains[0].voice,right=t.mains[1].voice;
        must(right>=0 && right<31 && left==right+1,
            "P204 linked SF2 pair must be first and left at base+1");
        must(t.mains[1].at<t.triggers[0].at &&
             t.triggers[0].voice==right && t.triggers[1].voice==left &&
             t.triggers[1].at<t.mains[2].at,
            "P204 real VXD stereo paired mains/triggers before mono");
        must(t.mains[2].voice==t.triggers[2].voice &&
             t.mains[2].at<t.triggers[2].at,
            "P204 remaining mono must start immediately after own main");
    }else{
        for(int i=0;i<3;i++){
            must(t.mains[i].voice==t.triggers[i].voice &&
                 t.mains[i].at<t.triggers[i].at,
                 "P204 independent mono direct start after main");
            if(i<2)must(t.triggers[i].at<t.mains[i+1].at,
                 "P204 first mono must trigger before next main");
        }
    }
    std::cout<<"P204_ORIGINAL_CHIP_CASE "<<name
             <<" allocators="<<3<<" all_00FF="<<t.reserves.size()
             <<" main="<<t.mains.size()
             <<" audible_samples="<<t.nonzero<<"\n";
}
}
int main(int argc,char**argv){
    if(argc!=5)return 2;
    try{
        validate(capture(argv[1],argv[2],"three"),"THREE_MONO",false);
        validate(capture(argv[1],argv[3],"monofirst"),"MONO_ZONE_FIRST_PAIR_FIRST",true);
        validate(capture(argv[1],argv[4],"rightfirst"),"RIGHT_ZONE_FIRST_PAIR_FIRST",true);
        std::puts("P204_REAL_ORIGINAL_CHIP_3_LAYER_RESERVE_ALL_AND_STEREO_FIRST_PASS");
#ifdef P205_VERIFY_ALLOC_RESET
        std::puts("P205_ORIGINAL_CHIP_ALLOCATOR_VTFT_RESET_3_LAYER_PASS");
#endif
        return 0;
    }catch(const std::exception&e){
        std::cerr<<"P204_ORIGINAL_CHIP_RESERVATION_NO_GO "<<e.what()<<"\n";
        return 1;
    }
}
