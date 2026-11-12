#include "CreativeSbkLayers.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>
#include <string_view>
using namespace awe64::creative;
using Bytes=std::vector<std::uint8_t>;
#define CHECK(x) do{if(!(x)){std::cerr<<"P002_MULTILAYER_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
void w16(Bytes&v,unsigned n){v.push_back(static_cast<uint8_t>(n));v.push_back(static_cast<uint8_t>(n>>8));}
void w32(Bytes&v,unsigned n){w16(v,n);w16(v,n>>16);}
void word(Bytes&v,unsigned id,unsigned raw){w16(v,id);w16(v,raw);}
void name(Bytes&v,std::string_view text){v.insert(v.end(),text.begin(),text.end());v.insert(v.end(),20-text.size(),0);}
void c(Bytes&v,std::string_view id,Bytes body){v.insert(v.end(),id.begin(),id.end());w32(v,static_cast<unsigned>(body.size()));v.insert(v.end(),body.begin(),body.end());if(body.size()&1)v.push_back(0);}
Bytes fixture(){
    Bytes phdr,pbag,pgen,inst,ibag,igen,shdr(96,0),info,pdta,root;
    // A synthetic ROM address entry: start=5, end=25, loop=10..20.
    Bytes addr;w32(addr,5);w32(addr,25);w32(addr,10);w32(addr,20);
    std::copy(addr.begin(),addr.end(),shdr.begin());
    name(phdr,"TWO-LAYER");w16(phdr,7);w16(phdr,0);w16(phdr,0);w32(phdr,0);w32(phdr,0);w32(phdr,0);
    name(phdr,"EOP");w16(phdr,0);w16(phdr,0);w16(phdr,3);w32(phdr,0);w32(phdr,0);w32(phdr,0);
    for(unsigned off:{0u,1u,5u,9u}){w16(pbag,off);w16(pbag,0);}
    word(pgen,48,111); // global zone
    word(pgen,41,0);word(pgen,43,0x7F20);word(pgen,48,100);word(pgen,8,111); // first local
    word(pgen,41,1);word(pgen,43,0x7F30);word(pgen,48,99);word(pgen,8,120); // second local
    name(inst,"LAYER-A");w16(inst,0);
    name(inst,"LAYER-B");w16(inst,2);
    name(inst,"EOI");w16(inst,4);
    for(unsigned off:{0u,2u,5u,7u,9u}){w16(ibag,off);w16(ibag,0);}
    word(igen,8,105);word(igen,34,10); // first instrument globals
    word(igen,43,0x7F20);word(igen,44,0x7F40);word(igen,53,151); // first local ROM selector
    word(igen,34,9);word(igen,55,3); // second instrument globals; no cutoff
    word(igen,43,0x7F30);word(igen,53,200); // second local ROM selector
    Bytes e; e.insert(e.end(),{'E','M','U','8','0','0','0',0});c(info,"isng",e);
    c(info,"IPRD",Bytes{'S','B','A','W','E','3','2',0});
    c(info,"irom",Bytes{'1','M','G','M',0});
    Bytes i; i.insert(i.end(),{'I','N','F','O'});i.insert(i.end(),info.begin(),info.end());
    Bytes p; p.insert(p.end(),{'p','d','t','a'});
    c(p,"phdr",phdr);c(p,"pbag",pbag);c(p,"pgen",pgen);
    c(p,"inst",inst);c(p,"ibag",ibag);c(p,"igen",igen);c(p,"shdr",shdr);
    c(root,"LIST",i);c(root,"LIST",p);
    Bytes b; b.insert(b.end(),{'R','I','F','F'});w32(b,static_cast<unsigned>(root.size()+4));
    b.insert(b.end(),{'s','f','b','k'});b.insert(b.end(),root.begin(),root.end());return b;
}
int main(){
    const auto bank=fixture();
    auto address=resolveCreative1MgmRomAddress(bank,0,524288);
    CHECK(address && address->start_word==5 && address->end_word==25);
    CHECK(address->loop_start_word==10 && address->loop_end_word==20);
    CHECK(!resolveCreative1MgmRomAddress(bank,0,24)); // actual ROM bound
    CHECK(!resolveCreative1MgmRomAddress(bank,151,524288)); // 2 fake rows, 6 slots
    CHECK(!resolveCreative1MgmRomAddress(bank,6,524288));   // outside all slots
    auto x=resolveCreativeSbkLayers(bank,0,7,64,100);
    CHECK(x && x->count==2);
    const auto &a=x->layers[0],&b=x->layers[1];
    CHECK(a.preset_bag==1 && a.instrument==0 && a.instrument_bag==1);
    CHECK(b.preset_bag==2 && b.instrument==1 && b.instrument_bag==3);
    CHECK(a.rom_sample_selector==151 && b.rom_sample_selector==200);
    CHECK(a.instrumentRawCutoff()==105 && !b.instrumentRawCutoff());
    CHECK(a.presetRawCutoff()==111 && b.presetRawCutoff()==120);
    CHECK(a.presetRawAttenuation()==100 && b.presetRawAttenuation()==99);
    CHECK(a.preset_global.get(48)==111 && b.preset_global.get(48)==111);
    CHECK(a.instrumentRawAttackVolEnv()==10 && b.instrumentRawAttackVolEnv()==9);
    CHECK(!b.instrument_global.get(8) && !b.instrument_local.get(8));
    auto low=resolveCreativeSbkLayers(bank,0,7,40,100);CHECK(low && low->count==1);
    CHECK(low->layers[0].instrument==0);
    auto quiet=resolveCreativeSbkLayers(bank,0,7,64,63);CHECK(quiet && quiet->count==1);
    CHECK(quiet->layers[0].instrument==1);
    auto noKey=resolveCreativeSbkLayers(bank,0,7,31,127);CHECK(noKey && noKey->count==0);
    CHECK(!resolveCreativeSbkLayers(bank,0,7,64,0));
    CHECK(!resolveCreativeSbkLayers(bank,0,7,128,100));
    CHECK(!resolveCreativeSbkLayers(bank,0,7,64,128));
    CHECK(!resolveCreativeSbkLayers(bank,0,128,64,100));
    auto wrongProgram=resolveCreativeSbkLayers(bank,0,8,64,100);
    CHECK(wrongProgram && wrongProgram->count==0);
    auto corrupted=bank;corrupted.pop_back();CHECK(!resolveCreativeSbkLayers(corrupted,0,7,64,100));
    corrupted=bank;auto it=std::search(corrupted.begin(),corrupted.end(),
         std::begin("1MGM"),std::end("1MGM")-1);CHECK(it!=corrupted.end());
    *it='X';CHECK(!resolveCreativeSbkLayers(corrupted,0,7,64,100));
    corrupted=bank; // invalid instrument gen op 64 must fail rather than discard silently
    const Bytes pattern{34,0,9,0,55,0,3,0};
    it=std::search(corrupted.begin(),corrupted.end(),pattern.begin(),pattern.end());
    CHECK(it!=corrupted.end());it[4]=64;
    CHECK(!resolveCreativeSbkLayers(corrupted,0,7,64,100));
    corrupted=bank;
    // A malformed sampled loop is not accepted as an audio address.
    const Bytes addressPattern{5,0,0,0,25,0,0,0,10,0,0,0,20,0,0,0};
    it=std::search(corrupted.begin(),corrupted.end(),addressPattern.begin(),addressPattern.end());
    CHECK(it!=corrupted.end());it[8]=26;
    CHECK(!resolveCreative1MgmRomAddress(corrupted,0,524288));
    // The synthetic test's preset generator8 offsets are unresolved
    // by the original VXD. Do not silently discard them.
    CHECK(!a.candidateOriginal1MgmInstrumentCutoff());
    CHECK(!b.candidateOriginal1MgmInstrumentCutoff());
    SbkLayer piano{};piano.bank=0;piano.key=60;piano.velocity=100;
    piano.instrument_global.raw[8]=110;
    piano.instrument_local.raw[8]=89;
    CHECK(piano.candidateOriginal1MgmInstrumentCutoff()==178);
    SbkLayer violin{};violin.bank=0;violin.program=40;
    violin.key=60;violin.velocity=100;
    violin.instrument_global.raw[8]=105;
    CHECK(violin.candidateOriginal1MgmInstrumentCutoff()==210);
    piano.instrument_local.raw[8].reset();
    CHECK(piano.candidateOriginal1MgmInstrumentCutoff()==220);
    piano.instrument_global.raw[8].reset();
    CHECK(!piano.candidateOriginal1MgmInstrumentCutoff());
    piano.instrument_local.raw[8]=127;
    CHECK(piano.candidateOriginal1MgmInstrumentCutoff()==254);
    piano.instrument_local.raw[8]=128;
    CHECK(!piano.candidateOriginal1MgmInstrumentCutoff());
    piano.instrument_local.raw[8]=89;
    piano.preset_global.raw[8]=1;
    CHECK(!piano.candidateOriginal1MgmInstrumentCutoff());
    piano.preset_global.raw[8].reset();
    piano.bank=128;
    CHECK(!piano.candidateOriginal1MgmInstrumentCutoff());
    std::cout<<"P002_CREATIVE_SBK_2_LAYER_NATIVE_CONTRACT_PASS cases=16 private_bytes=NONE\n";
}
