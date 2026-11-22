// P215 clean-room x86 PCOD original hold numeric sweep. Public synthetic
// cases only; no licensed ROM/SBK/SF2/Win95 guest driver embedded.
#include "SoundFont.h"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <utility>
namespace {
void check(bool b,const char*why) {if(!b)throw std::runtime_error(why);}
SoundFont::Bank fixture(bool sf1=false) {
    using namespace SoundFont;
    Bank b;
    b.valid=true;
    b.version=sf1?Version::Sf1:Version::Sf2;
    b.name="public original driver arithmetic stress";
    Sample s;
    s.originalKey=60;
    s.start=1000;s.end=3000;s.loopStart=1200;s.loopEnd=2400;
    b.samples.push_back(s);
    Instrument i;
    i.name="synthetic instrument";
    i.global.Set(Gen::HoldModEnv,0);
    Zone iz;iz.sampleId=0;i.zones.push_back(iz);
    b.instruments.push_back(std::move(i));
    Preset p;
    p.name="synthetic preset";p.bank=1;p.program=7;
    Zone z;z.instrument=0;
    z.gen.Set(Gen::SustainVolEnv,900);
    p.zones.push_back(z);
    b.presets.push_back(std::move(p));
    return b;
}
SoundFont::VoiceParams voice(const SoundFont::Bank&b,Awe32::Driver drv) {
    const auto zones=b.Select(1,7,60,110);
    check(zones.size()==1,"P215 synthetic zone selection failed");
    return SoundFont::MakeVoiceParams(b,zones[0],60,110,0x200020u,0u,drv);
}
}
int main() {
 try {
    std::uint64_t fnv=0xcbf29ce484222325ULL;
    for(int x=-32768;x<=32767;++x) {
        const int converted=SoundFont::OriginalCreativePcodHoldRate(x);
        check(converted>=0 && converted<=127,
              "P215 original PCOD conversion overflow/out-of-range");
        const std::uint16_t w=static_cast<std::uint16_t>(converted);
        for(unsigned sh: {0u,8u}) {
            fnv^=static_cast<std::uint8_t>(w>>sh);
            fnv*=0x100000001b3ULL;
        }
    }
    // Independently computed from the ORIGINAL PCOD+0x3BD4 x86 algorithm
    // on every signed-16-bit value. FNV is not derived from this code.
    check(fnv==0x8de7932315cfb4eaULL,
          "P215 65,536-case original PCOD numeric fingerprint mismatch");
    check(SoundFont::OriginalCreativePcodHoldRate(0)==116 &&
          SoundFont::OriginalCreativePcodHoldRate(-12000)==127 &&
          SoundFont::OriginalCreativePcodHoldRate(-5368)==127,
          "P215 PCOD original default, saturation or branch-boundary mismatch");
    const auto b=fixture();
    const auto win=voice(b,Awe32::Driver::Win95);
    const auto dos=voice(b,Awe32::Driver::Dos);
    check(((win.atkhld>>8)&255)==116,
          "P215 original Win95 SF2 HoldModEnv default not 0x74");
    check(((win.dcysusv>>8)&255)==13,
          "P215 original Win95 SF2 preset sustain units not 13");
    check(((dos.atkhld>>8)&255)!=116,
          "P215 DOS SF2 hold unexpectedly replaced by original Win95 PCOD");
    check(((dos.dcysusv>>8)&255)!=13,
          "P215 DOS SF2 sustain unexpectedly changed by Win95 template");
    const auto sf1=fixture(true);
    const auto regions=sf1.Select(1,7,60,110);
    check(regions.size()==1 &&
          regions[0].sf2OriginalModHoldRate==-1 &&
          regions[0].sf2OriginalVolSustain==-1,
          "P215 SF1/SBK original chip generator format altered");
    std::printf("P215_PUBLIC_ORIGINAL_PC0D_65536_HOLD_SF2_SCOPE_AND_SF1_DOS_ISOLATION_PASS fnv=%016llX\n",
          static_cast<unsigned long long>(fnv));
    return 0;
 }catch(const std::exception&e){
    std::fprintf(stderr,"P215_NO_GO %s\n",e.what());return 1;
 }
}
