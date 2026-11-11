/*
 * Synthetic non-silent reference produced by the original, byte-for-byte
 * pinned turican0/86Box-AWE32 snd_emu8k.c (not the 125A Chip wrapper).
 * Register stimulus adapted from AWETEST's documented direct-chip tone
 * setup (P236). This is NOT a real original-Win95/Creative-VM capture.
 */
#include "HostShim.h"
#include <86box/sound.h>
extern "C" {
#include <86box/snd_emu8k.h>
void emu8k_outw(uint16_t,uint16_t,void*);
uint16_t emu8k_inw(uint16_t,void*);
}
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs=std::filesystem;
namespace {
void require(bool ok,const char* m){ if(!ok) throw std::runtime_error(m); }
void u16(std::ostream& o,uint16_t x) {o.put(static_cast<char>(x));o.put(static_cast<char>(x>>8));}
void u32(std::ostream& o,uint32_t x) {u16(o,uint16_t(x));u16(o,uint16_t(x>>16));}
void header(std::ostream& o,uint32_t n){
    o.write("RIFF",4);u32(o,36+n*4);o.write("WAVEfmt ",8);
    u32(o,16);u16(o,1);u16(o,2);u32(o,44100);u32(o,176400);
    u16(o,4);u16(o,16);o.write("data",4);u32(o,n*4);
}
void syntheticRom(const fs::path& path) {
    std::ofstream o(path,std::ios::binary|std::ios::trunc);
    require(bool(o),"cannot create synthetic ROM");
    for(uint32_t i=0;i<524288u;++i){
        // Entirely synthetic source: a 96-sample sawtooth in the intended ROM region.
        const int16_t s=(i>=458900u && i<467400u)?
           static_cast<int16_t>((int(i%96u)-48)*350):0;
        u16(o,static_cast<uint16_t>(s));
    }
    require(bool(o),"synthetic ROM write incomplete");
}
struct Device {
    std::unique_ptr<emu8k_t> chip{new emu8k_t{}};
    explicit Device(const fs::path& rom) {
        const auto u=rom.u8string();
        const std::string path(u.begin(),u.end());
        require(awe64_host_set_rom_path(path.c_str()),"ROM binding");
        emu8k_init(chip.get(),0,28672);
        require(chip->rom&&chip->ram&&chip->ram_end_addr==0x1000000u,"fork init mapping");
    }
    ~Device(){
        emu8k_close(chip.get());
        std::free(chip->empty);
    }
};
struct Recorder {
    emu8k_t* chip;
    std::ofstream trace;
    explicit Recorder(emu8k_t* c,const fs::path& path):chip(c),trace(path) {
        require(bool(trace),"cannot open synthetic trace");
        trace<<"# synthetic fork direct-core reference, 44100 Hz; not a Win95 driver capture\n";
    }
    void write(uint64_t at,uint16_t port,uint16_t val) {
        trace<<at<<' '<<std::hex<<std::uppercase<<port<<' '<<val<<std::dec<<'\n';
        wavetable_pos_global=chip->pos;
        emu8k_outw(port,val,chip);
    }
    void read(uint64_t at,uint16_t port) {
        const uint16_t val=emu8k_inw(port,chip);
        trace<<"R "<<at<<' '<<std::hex<<std::uppercase<<port<<' '<<val<<std::dec<<'\n';
    }
    void set16(uint64_t at,unsigned reg,unsigned voice,uint16_t port,uint16_t val){
        write(at,0xE22,uint16_t((reg<<5)|voice));
        write(at,port,val);
    }
    void set32(uint64_t at,unsigned reg,unsigned voice,uint16_t port,uint32_t val) {
        write(at,0xE22,uint16_t((reg<<5)|voice));
        write(at,port,uint16_t(val));
        write(at,uint16_t(port+2),uint16_t(val>>16));
    }
};
void note(Recorder& w,uint64_t f) {
    constexpr unsigned v=29u;
    const uint32_t start=458995u,loopStart=459001u,loopEnd=467282u;
    // Original card's 0x04 HWCF3 unmute, exact register-port layout.
    w.set16(f,1,31,0xA20,0x0004);
    w.set16(f,5,v,0xA20,0x0080);          // voice off before new setup
    w.set32(f,3,v,0x620,0x0000FFFF);     // VTFT
    w.set32(f,2,v,0x620,0x0000FFFF);     // CVCF
    w.set32(f,6,v,0x620,(128u<<24)|loopStart);
    w.set32(f,7,v,0x620,loopEnd);        // CSL
    w.set32(f,0,v,0xA20,start);         // CCCA
    w.set16(f,0,v,0xE20,0xE000);        // IP
    w.set16(f,1,v,0xE20,0xFF00);        // IFATN: open cutoff, 0 attenuation
    w.set16(f,2,v,0xE20,0);             // PEFE
    w.set16(f,3,v,0xE20,0);             // FMMOD
    w.set16(f,4,v,0xE20,0);             // TREMFRQ
    w.set16(f,5,v,0xE20,0);             // FM2FRQ2
    w.set16(f,6,v,0xA20,0x8000);        // ENVVAL
    w.set16(f,6,v,0xA22,0x7F7F);        // ATKHLD
    w.set16(f,7,v,0xA20,0x7F00);        // DCYSUS
    w.set16(f,4,v,0xA20,0x8000);        // ENVVOL
    w.set16(f,4,v,0xA22,0x7F7F);        // ATKHLDV
    w.set16(f,5,v,0xA22,0x8000);        // LFO1VAL
    w.set16(f,7,v,0xA22,0x8000);        // LFO2VAL
    w.set32(f,1,v,0x620,0x40000000);    // PTRX
    w.set16(f,5,v,0xA20,0x7F00);        // trigger
    w.set16(f,0,v,0xE20,0xE000);
    w.read(f,0xE20);                    // stable IP readback
}
void render(emu8k_t* chip,std::ofstream& wav,uint32_t n,uint64_t& nonzero) {
    while(n) {
        const auto part=std::min<uint32_t>(n,137u);
        emu8k_reset_buffer(chip);
        wavetable_pos_global=static_cast<int>(part);
        emu8k_update(chip);
        for(uint32_t i=0;i<part*2u;++i){
            const auto x=std::clamp<int32_t>(chip->buffer[i],-32768,32767);
            nonzero+=(x!=0);
            u16(wav,static_cast<uint16_t>(static_cast<int16_t>(x)));
        }
        n-=part;
    }
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"usage: awe64_direct_fork_fixture OUTPUTDIR");
        const fs::path dir=argv[1];
        fs::create_directories(dir);
        const fs::path rom=dir/"synthetic.raw";
        syntheticRom(rom);
        Device d(rom);
        Recorder w(d.chip.get(),dir/"fork.trace");
        std::ofstream wav(dir/"fork.wav",std::ios::binary|std::ios::trunc);
        require(bool(wav),"can't write synthetic fork WAV");
        constexpr uint32_t total=4410u;
        header(wav,total);
        uint64_t nonzero=0;
        render(d.chip.get(),wav,11,nonzero);
        note(w,11);                       // first write AFTER frame zero
        render(d.chip.get(),wav,1589,nonzero);
        w.set16(1600,1,29,0xE20,0xB000); // audible IFATN attenuation change
        render(d.chip.get(),wav,1500,nonzero);
        w.set16(3100,5,29,0xA20,0x8080); // stop/release
        render(d.chip.get(),wav,total-3100,nonzero);
        wav.flush();
        require(bool(wav),"fork synthetic WAV write incomplete");
        require(nonzero>200,"positive control: original fork output silent");
        std::cout<<"SYNTHETIC_DIRECT_FORK nonzero_pcm16="<<nonzero
                 <<" frames="<<total<<" first_trace_frame=11\n";
        std::cout<<"NOT an original Creative VM/driver or hardware capture\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"SYNTHETIC_DIRECT_FORK_FIXTURE_FAIL "<<e.what()<<'\n';
        return 1;
    }
}
