// P213 PUBLIC-SYNTHETIC in-memory SF2 scopes. Absolutely no owner media.
// First reproduce three independently present instrument/preset generator
// scopes, then prove a baseline instrument-only selection is unchanged.
// Both paths run actual original SoundFont::SelectInto and MakeVoiceParams.
#include "SoundFont.h"
#include "Emu8000Regs.h"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void must(bool cond,const char* reason) {
    if(!cond) throw std::runtime_error(reason);
}
SoundFont::Bank build(bool presetScope,bool sf1=false) {
    using namespace SoundFont;
    Bank b;
    b.valid=true;
    b.version=sf1 ? Version::Sf1 : Version::Sf2;
    b.name="P213 PUBLIC synthetic-only";
    Sample sample;
    sample.name="public synthetic PCM";
    sample.start=1000;
    sample.end=3000;
    sample.loopStart=1200;
    sample.loopEnd=2400;
    sample.originalKey=60;
    b.samples.push_back(sample);
    Instrument instrument;
    instrument.name="public instrument";
    instrument.global.Set(Gen::AttackVolEnv, -7973);
    instrument.global.Set(Gen::HoldVolEnv, -2786);
    Zone iz;
    iz.sampleId=0;
    instrument.zones.push_back(iz);
    b.instruments.push_back(std::move(instrument));
    Preset preset;
    preset.name="public preset";
    preset.bank=1;
    preset.program=7;
    Zone pz;
    pz.instrument=0;
    if(presetScope) {
        pz.gen.Set(Gen::AttackVolEnv,-3986);
        pz.gen.Set(Gen::HoldVolEnv,-3986);
        pz.gen.Set(Gen::DelayVolEnv,-3986);
    }
    preset.zones.push_back(pz);
    b.presets.push_back(std::move(preset));
    return b;
}
SoundFont::VoiceParams voice(const SoundFont::Bank& bank) {
    auto regions=bank.Select(1,7,60,110);
    must(regions.size()==1,"P213 synthetic program/zone selection unavailable");
    return SoundFont::MakeVoiceParams(bank,regions[0],60,110,
        0x200020u,0u,Awe32::Driver::Win95);
}
}
int main(){
  try {
    const auto layered=voice(build(true));
    const auto instrument=voice(build(false));
    // Genuine private guest reproduces exactly 7F7F and BFFF after
    // volume-default delayed scope is combined; test PUBLIC numbers
    // exercise the ALGORITHM, never include user WAV/SF2 bytes.
    must(layered.atkhldv==0x7F7Fu,
         "P213_NO_GO SF2 converted two-scope attack/hold not saturating");
    must(layered.envvol==0x8000u,
         "P213_NO_GO missing instrument delay default not saturated with preset");
    must(instrument.atkhldv==0x7D74u && instrument.envvol==0x8000u,
         "P213_NO_GO single instrument scope changed unexpectedly");
    must(layered.atkhldv!=instrument.atkhldv,
         "P213_NO_GO vacuous attack/hold negative baseline");
    const auto sf1=build(true,true);
    const auto regs=sf1.Select(1,7,60,110);
    must(regs.size()==1 && regs[0].sf2CombinedVolDelay==-1
      && regs[0].sf2CombinedVolAttackRate==-1
      && regs[0].sf2CombinedVolHoldRate==-1,
      "P213_NO_GO Creative SF1/SBK scope semantics were modified");
    std::printf("P213_PUBLIC_SF2_ACTUAL_CONVERTED_SCOPES_PASS "
                "double_atkhldv=%04X delay=%04X single_atkhldv=%04X\n",
                layered.atkhldv,layered.envvol,instrument.atkhldv);
    return 0;
  }catch(const std::exception& ex){
    std::fprintf(stderr,"P213_NO_GO %s\n",ex.what());return 1;
  }
}
