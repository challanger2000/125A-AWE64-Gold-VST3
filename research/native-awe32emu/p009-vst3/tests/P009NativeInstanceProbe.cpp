// P009 WINDOWS CI: original AWE32Emu driver/EMU8000 core, same-process
// parallel *synthetic* fallback sine with 1MiB all-zero TEST ROM.
// This does NOT establish authentic original Creative sounds or RT safety.
// The VST3 CI is linked against the identical pinned+per-instance chip.
#include "Synth.h"
#include "Awe32Driver.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace {
struct Result {std::uint64_t hash=0; double rms=0.;};
class Runner {
    Synth core_{44100};
public:
    explicit Runner(const std::string& rom){
        std::string err;
        if(!core_.LoadWaveRom(rom,err))throw std::runtime_error(err);
        core_.Core().SetChipRamKb(28672);
        if(!core_.Core().UseBox86Chip(rom,err))
            throw std::runtime_error(err);
        core_.Core().SetDriver(Awe32::Driver::Win95);
        if(auto* ram=core_.Core().ChipRam()){
            const auto n=std::min(core_.Core().DramSize(),
                                  core_.Core().ChipRamWords());
            std::memcpy(ram,core_.Core().DramData(),n*sizeof(std::int16_t));
        } else throw std::runtime_error("no chip RAM");
        core_.Core().PowerOnInit();
        core_.ProgramChange(0,7);
    }
    Result run(std::atomic<bool>* start=nullptr,bool migrate=false){
        if(start)while(!start->load(std::memory_order_acquire))
            std::this_thread::yield();
        core_.NoteOn(0,60,95);
        std::vector<std::int16_t> buf(2048);
        std::uint64_t hash=14695981039346656037ull;
        double sum=0.;unsigned count=0;
        for(int block=0;block<100;++block){
            const auto write=[&](){
                if(block==44)core_.NoteOff(0,60);
                core_.RenderBlock(buf.data(),1024);
            };
            if(migrate) {
                // Deliberately run each successive block on a DIFFERENT
                // host worker thread. Global TLS timers are not a valid
                // per-instance clock under DAW worker migration.
                std::thread worker(write); worker.join();
            }else write();
            for(const auto sample:buf){
                hash=(hash^static_cast<std::uint16_t>(sample))*
                    1099511628211ull;
                sum+=double(sample)*double(sample);++count;
            }
        }
        return {hash,std::sqrt(sum/double(count))/32768.};
    }
};
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    try {
        const std::string fakeRom=argv[1];
        Runner a(fakeRom),b(fakeRom);
        const auto baseline=a.run(),baseline2=b.run();
        Runner c(fakeRom),d(fakeRom),m(fakeRom);
        std::atomic<bool> go{false};
        Result concurrent1{},concurrent2{};
        std::thread t1([&](){concurrent1=c.run(&go);});
        std::thread t2([&](){concurrent2=d.run(&go);});
        go.store(true,std::memory_order_release);
        t1.join();t2.join();
        const Result migrated=m.run(nullptr,true);
        const bool okay=baseline.hash==baseline2.hash &&
            baseline.hash==concurrent1.hash &&
            baseline.hash==concurrent2.hash &&
            baseline.hash==migrated.hash &&
            baseline.rms>0.00001 &&
            baseline2.rms>0.00001 &&
            concurrent1.rms>0.00001 &&
            concurrent2.rms>0.00001 &&
            migrated.rms>0.00001;
        std::cout<<"P009_SAME_PROCESS_PCM_HASH seq="
                 <<std::hex<<baseline.hash<<" other="<<baseline2.hash
                 <<" parallel1="<<concurrent1.hash
                 <<" parallel2="<<concurrent2.hash
                 <<" migrated="<<migrated.hash<<std::dec
                 <<" rms="<<baseline.rms<<"\n";
        std::cout<<(okay?
           "P009_WINDOWS_SYNTHETIC_2_PARALLEL_AND_THREAD_MIGRATION_HASH_PASS":
           "P009_WINDOWS_SYNTHETIC_2_PARALLEL_AND_THREAD_MIGRATION_HASH_FAIL")
           <<"\n";
        return okay?0:1;
    }catch(const std::exception& e){
        std::cerr<<"P009_INSTANCE_EXCEPTION "<<e.what()<<"\n";
        return 2;
    }
}
