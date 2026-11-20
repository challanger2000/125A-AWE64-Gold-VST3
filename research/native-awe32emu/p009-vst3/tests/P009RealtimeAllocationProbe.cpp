// P009: Count C++ heap operations ONLY inside the live synth callback.
// This standalone native engine probe intentionally contains NO owner ROM
// data; PRIVATE owner-SBK/SF2 test is performed only outside public CI.
#include "Synth.h"
#include "Awe32Driver.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace {
thread_local bool countThisThread=false;
std::atomic<std::uint64_t> callbackAllocs{0};
std::atomic<std::uint64_t> callbackBytes{0};
void* countedMalloc(std::size_t n){
    if(countThisThread){
        callbackAllocs.fetch_add(1,std::memory_order_relaxed);
        callbackBytes.fetch_add(n,std::memory_order_relaxed);
    }
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
struct SynthUnit{
    Synth synth;
    explicit SynthUnit(std::uint32_t rate,const char* rom,
                       const char* gm,const char* bank):synth(rate){
        std::string err;
        if(!synth.LoadWaveRom(rom,err))throw std::runtime_error("ROM "+err);
        synth.Core().SetChipRamKb(28672);
        if(!synth.Core().UseBox86Chip(rom,err))
            throw std::runtime_error("chip "+err);
        synth.Core().SetDriver(Awe32::Driver::Win95);
        if(gm && !synth.LoadBank(gm,err,false,-1))
            throw std::runtime_error("SBK "+err);
        if(bank && !synth.LoadBank(bank,err,false,1))
            throw std::runtime_error("SF2 "+err);
        if(auto* ram=synth.Core().ChipRam()){
            const auto words=std::min(synth.Core().DramSize(),
                                      synth.Core().ChipRamWords());
            std::memcpy(ram,synth.Core().DramData(),words*sizeof(std::int16_t));
        }else throw std::runtime_error("No original EMU8000 RAM");
        synth.Core().PowerOnInit();
        if(!synth.PrepareRealtime(1024))
            throw std::runtime_error("Unable to prepare bounded RT scratch memory");
    }
    std::uint64_t run(unsigned events){
        std::int16_t pcm[2048]{};
        std::uint64_t hash=14695981039346656037ull;
        std::uint64_t energy=0;
        const auto before=callbackAllocs.load(std::memory_order_relaxed);
        countThisThread=true;
        for(unsigned i=0;i<events;++i){
            const unsigned ch=i%16;
            const unsigned pitch=ch==9?36u+(i%5):48u+(i%24);
            if(i%3==0)synth.ProgramChange(
                static_cast<std::uint8_t>(ch),static_cast<std::uint8_t>(i%77));
            if(i%5==0)synth.ControlChange(
                static_cast<std::uint8_t>(ch),1,static_cast<std::uint8_t>(i%127));
            if(i%7==0)synth.ControlChange(
                static_cast<std::uint8_t>(ch),91,static_cast<std::uint8_t>(i%128));
            if(i%13==0)synth.ControlChange(
                static_cast<std::uint8_t>(ch),93,static_cast<std::uint8_t>(i%128));
            if(i%11==0)synth.PitchBend(
                static_cast<std::uint8_t>(ch),
                static_cast<std::int16_t>(static_cast<int>(i%16000)-8000));
            synth.NoteOn(static_cast<std::uint8_t>(ch),
                         static_cast<std::uint8_t>(pitch),
                         static_cast<std::uint8_t>(20+i%108));
            const unsigned frames=i%3==0?1u:(i%3==1?257u:1024u);
            synth.RenderBlock(pcm,frames);
            synth.NoteOff(static_cast<std::uint8_t>(ch),
                          static_cast<std::uint8_t>(pitch));
            for(unsigned j=0;j<frames*2;++j){
                hash=(hash^static_cast<std::uint16_t>(pcm[j]))*
                      1099511628211ull;
                energy+=static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(pcm[j])*pcm[j]);
            }
        }
        countThisThread=false;
        const auto delta=callbackAllocs.load(std::memory_order_relaxed)-before;
        if(delta!=0)throw std::runtime_error("C++ operator new on audio callback");
        if(energy==0)throw std::runtime_error("all PCM silent");
        return hash;
    }
};
}
void* operator new(std::size_t n){return countedMalloc(n);}
void* operator new[](std::size_t n){return countedMalloc(n);}
void operator delete(void*p) noexcept{std::free(p);}
void operator delete[](void*p) noexcept{std::free(p);}
void operator delete(void*p,std::size_t) noexcept{std::free(p);}
void operator delete[](void*p,std::size_t) noexcept{std::free(p);}
int main(int argc,char**argv){
    if(argc<2||argc>4)return 2;
    try {
        const char* gm=argc>=3?argv[2]:nullptr;
        const char* bank=argc>=4?argv[3]:nullptr;
        for(const auto rate:{44100u,48000u,96000u}){
            SynthUnit a(rate,argv[1],gm,bank),b(rate,argv[1],gm,bank);
            const auto before=callbackAllocs.load();
            const auto ha=a.run(128),hb=b.run(128);
            if(ha!=hb)throw std::runtime_error("sequential instance PCM mismatch");
            SynthUnit c(rate,argv[1],gm,bank),d(rate,argv[1],gm,bank);
            std::uint64_t pa=0,pb=0;
            std::thread t1([&](){pa=c.run(128);});
            std::thread t2([&](){pb=d.run(128);});
            t1.join();t2.join();
            if(pa!=ha||pb!=hb)
                throw std::runtime_error("parallel instance PCM mismatch");
            const auto delta=callbackAllocs.load()-before;
            if(delta!=0)throw std::runtime_error("RT heap allocation");
            std::cout<<"P009_RT_NO_HEAP_ORIGINAL_EMU8000_PASS rate="<<rate
                     <<" events=512 allocations="<<delta
                     <<" hash="<<std::hex<<pa<<std::dec<<"\n";
        }
        std::cout<<"P009_RT_THREE_RATES_TWO_INSTANCES_NO_HEAP_PASS\n";
        return 0;
    }catch(const std::exception&e){
        std::cerr<<"P009_RT_CALLBACK_FAIL "<<e.what()
                 <<" allocs="<<callbackAllocs.load()
                 <<" bytes="<<callbackBytes.load()<<"\n";
        return 1;
    }
}
