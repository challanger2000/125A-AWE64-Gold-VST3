// P016 public ORIGINAL EMU8000 chip and reconstructed Win95 driver.
// Uses generated PUBLIC two-layer SF2 and all-zero synthetic ROM, not owner
// AWE64 ROM/SYNTHGM bank. A real non-silent signal parity regression,
// but emphatically NOT original Creative ROM acoustic acceptance.
// Same reset routine is compiled into the shipping native VST3.
#include "P009CreativeDefaults.hpp"
#include "Synth.h"
#include "Awe32Driver.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
namespace {
struct Result {
    std::uint64_t hash=14695981039346656037ULL;
    double energy=0.;
};
struct Unit {
    Synth synth;
    Unit(unsigned rate,const std::string& rom,const std::string& publicBank)
      :synth(rate) {
        std::string err;
        if(!synth.LoadWaveRom(rom,err))throw std::runtime_error("ROM "+err);
        synth.Core().SetChipRamKb(28672);
        if(!synth.Core().UseBox86Chip(rom,err))
            throw std::runtime_error("EMU8000 "+err);
        synth.Core().SetDriver(Awe32::Driver::Win95);
        // The public bank is a genuine synthetic-SF2 RIFF file generated in CI.
        // No Creative samples, no alternate DSP/fake output code path.
        if(!synth.LoadBank(publicBank,err,false,1))
            throw std::runtime_error("public SF2 "+err);
        auto* chipRam=synth.Core().ChipRam();
        if(!chipRam)throw std::runtime_error("chip RAM missing");
        const auto words=std::min(synth.Core().DramSize(),
                                  synth.Core().ChipRamWords());
        std::memcpy(chipRam,synth.Core().DramData(),
                    words*sizeof(std::int16_t));
        synth.Core().PowerOnInit();
        if(!synth.PrepareRealtime(1024))
            throw std::runtime_error("PrepareRealtime failed");
    }
    void dirty(unsigned channel){
        auto ch=static_cast<std::uint8_t>(channel);
        synth.ControlChange(ch,0,1);
        synth.ProgramChange(ch,0);
        synth.ControlChange(ch,7,0);  // mute from OLD project
        synth.ControlChange(ch,64,127);
        synth.ControlChange(ch,1,115);
        synth.ControlChange(ch,10,23);
        synth.ControlChange(ch,11,38);
        synth.ControlChange(ch,91,127);
        synth.ControlChange(ch,93,127);
        synth.ControlChange(ch,101,0);
        synth.ControlChange(ch,100,0);
        synth.ControlChange(ch,6,24);
        synth.ControlChange(ch,101,127);
        synth.ControlChange(ch,100,127);
        synth.PitchBend(ch,8191);
    }
    void target(unsigned channel){
        auto ch=static_cast<std::uint8_t>(channel);
        synth.ControlChange(ch,0,1);
        synth.ControlChange(ch,32,0);
        synth.ProgramChange(ch,0);
        // CC7 is intentionally UNSET in target: the default should be 100.
        // Other saved controls and RPN 0,0 are deliberately nontrivial.
        synth.ControlChange(ch,1,52);
        synth.ControlChange(ch,10,84);
        synth.ControlChange(ch,11,116);
        synth.ControlChange(ch,91,0);
        synth.ControlChange(ch,93,0);
        synth.PitchBend(ch,2108);
        synth.ControlChange(ch,101,0);
        synth.ControlChange(ch,100,0);
        synth.ControlChange(ch,6,12);
        synth.ControlChange(ch,101,127);
        synth.ControlChange(ch,100,127);
    }
    Result play(unsigned channel){
        const auto ch=static_cast<std::uint8_t>(channel);
        synth.NoteOn(ch,60,104);
        Result result;
        std::array<std::int16_t,2048> pcm{};
        for(unsigned block=0;block<36;++block){
            if(block==17)synth.NoteOff(ch,60);
            synth.RenderBlock(pcm.data(),1024);
            for(const auto sample:pcm){
                result.hash=(result.hash ^
                    static_cast<std::uint16_t>(sample))*1099511628211ULL;
                result.energy+=static_cast<double>(sample)*sample;
            }
        }
        return result;
    }
};
void assertCase(unsigned rate,unsigned channel,
                const std::string& rom,const std::string& bank) {
    Unit fresh(rate,rom,bank),reused(rate,rom,bank),bad(rate,rom,bank);
    p009::resetOriginalCreativeControllers(fresh.synth);
    fresh.target(channel);
    reused.dirty(channel);
    p009::resetOriginalCreativeControllers(reused.synth);
    reused.target(channel);
    bad.dirty(channel);
    // Negative control deliberately omits production controller reset.
    bad.target(channel);
    const auto a=fresh.play(channel);
    const auto b=reused.play(channel);
    const auto c=bad.play(channel);
    if(a.energy<=0.00001 || b.energy<=0.00001)
        throw std::runtime_error("public SF2 is silent; PCM proof invalid");
    if(a.hash!=b.hash)
        throw std::runtime_error("real EMU8000 PCM mismatch after dirty->target");
    if(c.hash==a.hash)
        throw std::runtime_error("NEGATIVE CONTROL FAILED: stale CC7 ignored");
    std::printf("P016_PUBLIC_REAL_EMU8000_SF2_RECALL_PCM_PARITY_PASS"
                " rate=%u ch=%u fresh=%016llX reused=%016llX"
                " bad-no-reset=%016llX energy=%.1f\n",
                rate,channel+1,
                static_cast<unsigned long long>(a.hash),
                static_cast<unsigned long long>(b.hash),
                static_cast<unsigned long long>(c.hash),a.energy);
}
}
int main(int argc,char** argv){
    if(argc!=3){
        std::fprintf(stderr,"Usage: P016PublicRecallPCM <fake-ROM> <PUBLIC-generated-SF2>\n");
        return 2;
    }
    try{
        for(const unsigned rate:{44100u,48000u,96000u}){
            for(const unsigned channel:{0u,15u})
                assertCase(rate,channel,argv[1],argv[2]);
        }
        std::puts("P016_PUBLIC_SF2_EMU8000_6_PCM_PARITY_CASES_NEGATIVE_CONTROLS_PASS");
        std::puts("P016_NOT_ORIGINAL_CREATIVE_ROM_OR_WIN95_REFERENCE");
        return 0;
    }catch(const std::exception& e){
        std::fprintf(stderr,"P016_EMU8000_RECALL_PCM_FAIL %s\n",e.what());
        return 1;
    }
}
