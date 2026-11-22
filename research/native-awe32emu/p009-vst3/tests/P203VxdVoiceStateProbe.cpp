// P203: independently source-derived original Win95 VXD allocation oracle.
// Both real Win64 executable variants link exactly the production native
// Synth and authentic original 86Box EMU8000; no private ROM in public CI.
#include "Synth.h"
#include "Emu8000Regs.h"
#include "Awe32Driver.h"
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
void test(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
struct P203VoiceStateQA {
    static std::unique_ptr<Synth> make(const char*rom) {
        auto s=std::make_unique<Synth>(44100);
        std::string error;
        test(s->LoadWaveRom(rom,error),"P203 public synthetic ROM load");
        s->Core().SetChipRamKb(28672);
        test(s->Core().UseBox86Chip(rom,error),"P203 actual 86Box chip init");
        s->Core().SetDriver(Awe32::Driver::Win95);
        s->Core().PowerOnInit();
        for(int i=0;i<32;i++){
            s->Core().Write(Emu8000::Reg::VTFT,i,0);
            auto&v=s->m_alloc[i];
            v.vxdState=0x0ffe;
            v.vxdBlocked=false;v.vxdPairPenalty=false;
            v.vxdEndAddr=0;v.vxdRom=false;
        }
        s->m_vxdModeCount=32u;
        return s;
    }
    static uint32_t mode(Synth const&s){return s.m_vxdModeCount;}
    static void mode(Synth&s,uint32_t n){s.m_vxdModeCount=n;}
    static bool blocked(Synth const&s,int n){return s.m_alloc[n].vxdBlocked;}
    static void blocked(Synth&s,int n,bool flag){s.m_alloc[n].vxdBlocked=flag;}
    static void penalty(Synth&s,int n,bool flag){s.m_alloc[n].vxdPairPenalty=flag;}
    static void state(Synth&s,int n,uint16_t state){s.m_alloc[n].vxdState=state;}
    static void release(Synth&s,int n){s.Core().Write(Emu8000::Reg::DCYSUSV,n,0x8001u);}
    static void classTransition(Synth&s,int n,bool rom){s.SetVxdPhysicalRomClass(n,rom);}
    static int pair(Synth&s){return s.AllocateStereoPairVxd(0xfffe);}
    static int mono(Synth&s){return s.AllocateVoiceVxd(0xfffe);}
};
int main(int argc,char**argv){
    if(argc!=2)return 2;
    try{
        {
            auto s=P203VoiceStateQA::make(argv[1]);
            test(P203VoiceStateQA::mode(*s)==32,"C0 physical VXD mode_count must initialize at 32");
            P203VoiceStateQA::classTransition(*s,5,true);
            test(P203VoiceStateQA::mode(*s)==33 &&
                 P203VoiceStateQA::blocked(*s,5),"C0 RAM-to-ROM increments count");
            P203VoiceStateQA::classTransition(*s,5,true);
            test(P203VoiceStateQA::mode(*s)==33,"C0 repeated ROM does not increment");
            P203VoiceStateQA::classTransition(*s,5,false);
            test(P203VoiceStateQA::mode(*s)==32 &&
                 !P203VoiceStateQA::blocked(*s,5),"C0 ROM-to-RAM decrements count");
            P203VoiceStateQA::classTransition(*s,5,false);
            test(P203VoiceStateQA::mode(*s)==32,"C0 repeated RAM does not decrement");
            std::puts("P203_C0_ORIGINAL_VXD_MUTABLE_COUNT_RAM_ROM_PASS");
        }
        {
            auto s=P203VoiceStateQA::make(argv[1]);
            P203VoiceStateQA::blocked(*s,30,true);
            P203VoiceStateQA::blocked(*s,31,true);
            test(P203VoiceStateQA::pair(*s)==30,
                 "C1 VXD mode32 ignores protected class and takes upper pair30/31");
            std::puts("P203_C1_MODE32_STEREO_BASE30_PASS");
        }
        {
            auto s=P203VoiceStateQA::make(argv[1]);
            P203VoiceStateQA::mode(*s,1u);
            P203VoiceStateQA::blocked(*s,30,true);
            P203VoiceStateQA::blocked(*s,31,true);
            test(P203VoiceStateQA::pair(*s)==27,
                 "C2 VXD mode1 must skip upper protected 30/31 and take base27");
            std::puts("P203_C2_MODE1_PROTECTED_27_28_PASS");
        }
        {
            auto s=P203VoiceStateQA::make(argv[1]);
            P203VoiceStateQA::mode(*s,1u);
            for(int i=0;i<32;i++)P203VoiceStateQA::blocked(*s,i,true);
            test(P203VoiceStateQA::pair(*s)==-1,
                 "C3 protected 32 voices must not be stolen by stereo allocator");
            test(P203VoiceStateQA::mono(*s)==-1,
                 "C3 old unsafe fallback voice0 must not run");
            std::puts("P203_C3_NO_CANDIDATE_FAIL_CLOSED_PASS");
        }
        {
            auto s=P203VoiceStateQA::make(argv[1]);
            P203VoiceStateQA::state(*s,31,0x2000);
            test(P203VoiceStateQA::pair(*s)==29,
                 "C4 upper NEIGHBOR state>=0x2000 must not veto current row");
            std::puts("P203_C4_STEREO_RESERVED_UPPER_NEIGHBOR_NOT_VETO_PASS");
        }
        {
            auto a=P203VoiceStateQA::make(argv[1]);
            auto b=P203VoiceStateQA::make(argv[1]);
            for(int i=0;i<32;i++){
                P203VoiceStateQA::state(*a,i,0x003c);
                P203VoiceStateQA::state(*b,i,0x003c);
                P203VoiceStateQA::release(*a,i);
                P203VoiceStateQA::release(*b,i);
            }
            test(P203VoiceStateQA::pair(*a)==0,
                 "C5 all equal low nonzero priorities choose base0");
            P203VoiceStateQA::penalty(*b,0,true);
            test(P203VoiceStateQA::pair(*b)==1,
                 "C5 original row+0x1A penalty 0xC00 must choose base1");
            std::puts("P203_C5_ORIGINAL_PAIR_PENALTY_0C00_PASS");
        }
        std::puts("P203_REAL_ORIGINAL_86BOX_VXD_SIX_CASES_PASS");
        return 0;
    }catch(std::exception const&e){
        std::fprintf(stderr,"P203_VXD_MODE1_FAIL %s\n",e.what());
        return 1;
    }
}
