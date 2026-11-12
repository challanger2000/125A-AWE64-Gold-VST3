#include "CreativeSbkLayers.h"
#include <algorithm>
#include <array>
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
Bytes fixtureSf2(){
    // Synthetic 46-byte-shdr, ifil2.0 Creative ROM-bank fixture.
    // It exercises the SAME native SbkLayer resolver as old SBK;
    // no proprietary SF2 file, ROM or driver bytes are published.
    Bytes phdr,pbag,pgen,inst,ibag,igen,shdr,info,pdta,root;
    name(phdr,"SF2-ONE");w16(phdr,0);w16(phdr,0);w16(phdr,0);
    w32(phdr,0);w32(phdr,0);w32(phdr,0);
    name(phdr,"EOP");w16(phdr,0);w16(phdr,0);w16(phdr,2);
    w32(phdr,0);w32(phdr,0);w32(phdr,0);
    for(unsigned genIndex:{0u,1u,3u}){w16(pbag,genIndex);w16(pbag,0);}
    word(pgen,48,5); // original SF2 preset global
    word(pgen,41,0);word(pgen,48,7); // local wins global
    name(inst,"SF2-INST");w16(inst,0);
    name(inst,"EOI");w16(inst,2);
    for(unsigned genIndex:{0u,2u,6u}){w16(ibag,genIndex);w16(ibag,0);}
    word(igen,8,9617);word(igen,48,9); // instrument global
    word(igen,43,0x7F00);word(igen,48,11);
    word(igen,53,0); // first sampleId ENDS the zone in original VXD
    word(igen,43,0x0500); // late range excludes key60 ONLY if bugged
    name(shdr,"SAMPLE");w32(shdr,100);w32(shdr,1000);
    w32(shdr,200);w32(shdr,900);w32(shdr,44100);
    shdr.push_back(60);shdr.push_back(0);w16(shdr,0);w16(shdr,0x8001);
    name(shdr,"EOS");w32(shdr,0);w32(shdr,0);
    w32(shdr,0);w32(shdr,0);w32(shdr,44100);
    shdr.push_back(60);shdr.push_back(0);w16(shdr,0);w16(shdr,0);
    Bytes ver;w16(ver,2);w16(ver,0);c(info,"ifil",ver);
    c(info,"isng",Bytes{'E','M','U','8','0','0','0',0});
    c(info,"IPRD",Bytes{'S','B','A','W','E','3','2',0});
    c(info,"irom",Bytes{'1','M','G','M',0});
    Bytes i{'I','N','F','O'};i.insert(i.end(),info.begin(),info.end());
    Bytes p{'p','d','t','a'};c(p,"phdr",phdr);c(p,"pbag",pbag);
    c(p,"pgen",pgen);c(p,"inst",inst);c(p,"ibag",ibag);
    c(p,"igen",igen);c(p,"shdr",shdr);
    c(root,"LIST",i);c(root,"LIST",p);
    Bytes b{'R','I','F','F'};w32(b,static_cast<unsigned>(root.size()+4));
    b.insert(b.end(),{'s','f','b','k'});
    b.insert(b.end(),root.begin(),root.end());
    return b;
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
    // Original format1 operator48 conversion is NOT a device-state
    // attenuation: 107 -> original instVol descriptor150, 116 -> 82.
    SbkLayer sf1{};
    sf1.instrument_global.raw[48]=107;
    CHECK(sf1.candidateOriginalSf1InstVolFromGenerator48()==150);
    sf1.instrument_local.raw[48]=116;
    CHECK(sf1.candidateOriginalSf1InstVolFromGenerator48()==82);
    sf1.preset_global.raw[48]=5;
    sf1.preset_local.raw[48]=7;
    sf1.instrument_global.raw[48]=9;
    sf1.instrument_local.raw[48]=11;
    CHECK(sf1.candidateOriginalSf1InstVolFromGenerator48()==870);
    sf1.instrument_local.raw[48].reset();
    CHECK(sf1.candidateOriginalSf1InstVolFromGenerator48()==885);
    sf1.instrument_global.raw[48].reset();
    CHECK(sf1.candidateOriginalSf1InstVolFromGenerator48()==900);
    sf1.preset_local.raw[48].reset();
    CHECK(sf1.candidateOriginalSf1InstVolFromGenerator48()==915);
    sf1.preset_global.raw[48].reset();
    CHECK(!sf1.candidateOriginalSf1InstVolFromGenerator48());
    sf1.instrument_global.raw[48]=128;
    CHECK(sf1.candidateOriginalSf1InstVolFromGenerator48()==-7);
    sf1.instrument_global.raw[48]=static_cast<std::uint16_t>(-32768);
    CHECK(!sf1.candidateOriginalSf1InstVolFromGenerator48());
    // Original x86 PCOD+0x3985 format2 SF2, NOT the 48-byte SBK:
    // genuine source SF2 generator8 cutoffs 9617->179, 10561->211.
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(8,9617)==179);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(8,10561)==211);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(8,14400)==344);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(48,150)==150);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(48,82)==82);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(5,1000)==107);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(9,24)==2);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(10,1000)==36);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(11,1000)==18);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(13,120)==128);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(13,-120)==-128);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(29,1000)==0);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(29,-1000)==254);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(51,3)==300);
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(51,30000)==
          static_cast<std::int16_t>(static_cast<std::uint16_t>(3000000)));
    CHECK(!candidateOriginalSf2InstrumentLoadGenerator(59,0));
    CHECK(candidateOriginalSf2PresetDeltaGenerator(8,0)==0);
    CHECK(candidateOriginalSf2PresetDeltaGenerator(8,29)==1);
    CHECK(candidateOriginalSf2PresetDeltaGenerator(29,1000)==-127);
    CHECK(candidateOriginalSf2PresetDeltaGenerator(51,3)==300);
    CHECK(candidateOriginalSf2PresetDeltaGenerator(48,7)==7);
    CHECK(!candidateOriginalSf2PresetDeltaGenerator(59,0));
    // Exact original SF2 builder: local overrides global within each
    // level; separately converted preset and instrument are SAT-ADDED.
    SbkLayer sf2{};
    sf2.source_format=OriginalCreativeBankFormat::Format2Sf2;
    sf2.preset_global.raw[48]=5;
    sf2.preset_local.raw[48]=7;
    sf2.instrument_global.raw[48]=9;
    sf2.instrument_local.raw[48]=11;
    CHECK(candidateOriginalSf2InstVolFromExplicitScopes(sf2)==18);
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,48)==18);
    CHECK(!candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,8));
    CHECK(!candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,9));
    CHECK(!candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,59));
    // Original PCOD format2 cutoff: C(9617)=179, C(0)=-153;
    // preset 29 contributes delta1, preset 58 contributes delta2.
    sf2.preset_global.raw[8]=29;
    sf2.instrument_local.raw[8]=9617;
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,8)==180);
    sf2.preset_local.raw[8]=58;
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,8)==181);
    sf2.instrument_global.raw[8]=10561;
    sf2.instrument_local.raw[8].reset();
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,8)==213);
    sf2.preset_local.raw[8].reset();
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,8)==212);
    sf2.preset_global.raw[8].reset();
    CHECK(!candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,8));
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,8,0)==211);
    // Source-verified SF2 GM100 signed volume attack descriptor -11958.
    // The subsequent distinct original PCOD attack rate yields 128;
    // only the later VXD ATKHLDV hardware writer maps 128 to 127.
    sf2.preset_global.raw[34]=0;
    sf2.instrument_local.raw[34]=static_cast<std::uint16_t>(-11958);
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,34)==-11958);
    sf2.preset_local.raw[34]=200;
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,34)==-11758);
    sf2.preset_local.raw[34].reset();
    sf2.instrument_global.raw[34]=static_cast<std::uint16_t>(-8800);
    sf2.instrument_local.raw[34].reset();
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,34)==-8800);
    sf2.preset_global.raw[34].reset();
    CHECK(!candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,34));
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(sf2,34,0)==-8800);
    // Bounded source-backed numerical PCOD voice-generator matrix.
    // Instrument C(x), preset C(x)-C(0) where the original table says
    // OFFSET; identity elsewhere. No private bank bytes enter this test.
    struct NumericVoiceCase {
        std::uint16_t op;
        std::int16_t preset, instrument, expected;
    };
    constexpr std::array<NumericVoiceCase,14> numericCases{{
        {5,1000,1000,214}, {6,1000,1000,214},
        {7,1000,1000,214}, {8,29,9617,180},
        {9,24,-24,0}, {10,1000,1000,72},
        {11,1000,1000,36}, {13,120,-120,0},
        {29,1000,-1000,127}, {34,200,-11958,-11758},
        {37,1000,-1000,127}, {48,7,11,18},
        {51,3,-2,100}, {52,200,-100,100}
    }};
    for(const auto& v:numericCases) {
        SbkLayer layer{};
        layer.source_format=OriginalCreativeBankFormat::Format2Sf2;
        layer.preset_global.raw[v.op]=static_cast<std::uint16_t>(v.preset);
        layer.instrument_global.raw[v.op]=static_cast<std::uint16_t>(v.instrument);
        CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
            layer,v.op)==v.expected);
        // A missing level must not turn into an invented zero.
        layer.preset_global.raw[v.op].reset();
        CHECK(!candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
            layer,v.op));
        CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
            layer,v.op,0)==
            candidateOriginalSf2InstrumentLoadGenerator(v.op,v.instrument));
        layer.preset_local.raw[v.op]=static_cast<std::uint16_t>(v.preset);
        CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
            layer,v.op)==v.expected);
        layer.source_format=OriginalCreativeBankFormat::Format1Sbk;
        CHECK(!candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
            layer,v.op,0,0));
    }
    // All numeric format-2 envelope/LFO source fields are merged before
    // the original rate conversion; key-to-hold/decay fields stay signed.
    for(const std::uint16_t op:{21u,22u,23u,24u,25u,26u,27u,28u,
                               30u,31u,32u,33u,35u,36u,38u,39u,40u}) {
        SbkLayer env{};
        env.source_format=OriginalCreativeBankFormat::Format2Sf2;
        env.preset_global.raw[op]=10;
        env.instrument_global.raw[op]=100;
        CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(env,op)==110);
    }
    // Preset local always replaces preset global, instrument local always
    // replaces instrument global, including when the local value is zero.
    SbkLayer replace{};
    replace.source_format=OriginalCreativeBankFormat::Format2Sf2;
    replace.preset_global.raw[51]=3;
    replace.preset_local.raw[51]=0;
    replace.instrument_global.raw[51]=2;
    replace.instrument_local.raw[51]=0;
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(replace,51)==0);
    replace.preset_local.raw[51].reset();
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(replace,51)==300);
    replace.instrument_local.raw[51].reset();
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(replace,51)==500);
    replace.preset_global.raw[51]=300;
    replace.instrument_global.raw[51]=300;
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(replace,51)==32767);
    replace.preset_global.raw[51]=static_cast<std::uint16_t>(-300);
    replace.instrument_global.raw[51]=static_cast<std::uint16_t>(-300);
    CHECK(candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(replace,51)==-32768);
    // Non-numeric structure/control generators must never enter arithmetic.
    for(const unsigned id:{0u,4u,41u,43u,44u,46u,47u,49u,
                           53u,54u,56u,57u,58u,63u})
        CHECK(!candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
            replace,static_cast<std::uint16_t>(id),0,0));
    // The same four scopes format1 yields 870, not 18.
    auto identicalRawAsFormat1=sf2;
    identicalRawAsFormat1.source_format=OriginalCreativeBankFormat::Format1Sbk;
    CHECK(identicalRawAsFormat1.candidateOriginalSf1InstVolFromGenerator48()==870);
    CHECK(!sf2.candidateOriginalSf1InstVolFromGenerator48());
    sf2.preset_local.raw[48].reset();
    sf2.instrument_local.raw[48].reset();
    CHECK(candidateOriginalSf2InstVolFromExplicitScopes(sf2)==14);
    sf2.preset_global.raw[48].reset();
    CHECK(!candidateOriginalSf2InstVolFromExplicitScopes(sf2));
    CHECK(candidateOriginalSf2InstVolFromExplicitScopes(sf2,0)==9);
    sf2.instrument_global.raw[48]=150;
    CHECK(candidateOriginalSf2InstVolFromExplicitScopes(sf2,0)==150);
    sf2.instrument_global.raw[48]=82;
    CHECK(candidateOriginalSf2InstVolFromExplicitScopes(sf2,0)==82);
    sf2.instrument_global.raw[48].reset();
    CHECK(!candidateOriginalSf2InstVolFromExplicitScopes(sf2,0));
    CHECK(candidateOriginalSf2InstVolFromExplicitScopes(sf2,0,0)==0);
    sf2.preset_local.raw[48]=32767;
    sf2.instrument_local.raw[48]=32767;
    CHECK(candidateOriginalSf2InstVolFromExplicitScopes(sf2)==32767);
    sf2.preset_local.raw[48]=static_cast<std::uint16_t>(-32768);
    sf2.instrument_local.raw[48]=static_cast<std::uint16_t>(-32768);
    CHECK(candidateOriginalSf2InstVolFromExplicitScopes(sf2)==-32768);
    // Original format-2 Creative SF2 structurally differs from SBK:
    // ifil2, 46-byte shdr with name20+addresses, plus sampleId
    // terminating the instrument zone before late keyRange.
    const auto bankSf2=fixtureSf2();
    const auto parsedSf2=resolveCreativeSbkLayers(bankSf2,0,0,60,90);
    CHECK(parsedSf2 && parsedSf2->count==1);
    const auto& fromSf2=parsedSf2->layers[0];
    CHECK(fromSf2.source_format==OriginalCreativeBankFormat::Format2Sf2);
    CHECK(fromSf2.original_preset_row_index==0u);
    CHECK(fromSf2.preset_bag==0u);
    CHECK(fromSf2.preset_global.get(48)==5);
    CHECK(fromSf2.preset_local.get(48)==7);
    CHECK(fromSf2.instrument_global.get(48)==9);
    CHECK(fromSf2.instrument_local.get(48)==11);
    CHECK(fromSf2.instrument_local.get(43)==0x7F00u);
    CHECK(fromSf2.instrument_local.get(53)==0);
    CHECK(candidateOriginalSf2InstVolFromExplicitScopes(fromSf2)==18);
    CHECK(!fromSf2.candidateOriginalSf1InstVolFromGenerator48());
    CHECK(!fromSf2.candidateOriginal1MgmInstrumentCutoff());
    CHECK(candidateOriginalSf2InstrumentLoadGenerator(
        8,static_cast<std::int16_t>(*fromSf2.instrument_global.get(8)))==179);
    const auto key127Sf2=resolveCreativeSbkLayers(bankSf2,0,0,127,90);
    CHECK(key127Sf2 && key127Sf2->count==1); // late keyRange ignored
    const auto sf2Sample=resolveCreative1MgmRomAddress(
        bankSf2,0,524288);
    CHECK(sf2Sample && sf2Sample->start_word==100 &&
          sf2Sample->end_word==1000 &&
          sf2Sample->loop_start_word==200 &&
          sf2Sample->loop_end_word==900);
    CHECK(sf2Sample->source_format==OriginalCreativeBankFormat::Format2Sf2);
    CHECK(sf2Sample->sample_rate_hz==44100u &&
          sf2Sample->original_pitch==60u &&
          sf2Sample->pitch_correction==0 &&
          sf2Sample->sample_link==0u &&
          sf2Sample->sample_type==0x8001u);
    CHECK(!resolveCreative1MgmRomAddress(bankSf2,1,524288)); // EOS
    CHECK(!resolveCreative1MgmRomAddress(bankSf2,0,900)); // out of ROM
    // The ROM-only SF2 resolver must never reinterpret a RAM sample or
    // an unsupported linked-stereo member as a raw ROM address.
    const std::array<std::uint8_t,6> sampleName{'S','A','M','P','L','E'};
    const auto sampleIt=std::search(bankSf2.begin(),bankSf2.end(),
                                    sampleName.begin(),sampleName.end());
    CHECK(sampleIt!=bankSf2.end());
    const auto sampleStart=static_cast<std::size_t>(sampleIt-bankSf2.begin());
    CHECK(sampleStart+45u<bankSf2.size());
    CHECK(bankSf2[sampleStart+44u]==1u && bankSf2[sampleStart+45u]==128u);
    auto sf2RamSample=bankSf2;
    sf2RamSample[sampleStart+45u]=0u; // sampleType 0x0001: RAM mono
    CHECK(resolveCreativeSbkLayers(sf2RamSample,0,0,60,90));
    CHECK(!resolveCreative1MgmRomAddress(sf2RamSample,0,524288));
    auto sf2StereoMember=bankSf2;
    sf2StereoMember[sampleStart+44u]=2u; // ROM right, requires link handling
    CHECK(!resolveCreative1MgmRomAddress(sf2StereoMember,0,524288));
    // Valid ROM-mono sample must also have a usable SF2 sample rate and
    // genuine original pitch. A 46-byte header is NOT a packed SBK row.
    auto zeroSampleRate=bankSf2;
    std::fill_n(zeroSampleRate.begin()+sampleStart+36u,4,0u);
    CHECK(!resolveCreative1MgmRomAddress(zeroSampleRate,0,524288));
    auto invalidOriginalPitch=bankSf2;
    invalidOriginalPitch[sampleStart+40u]=128u;
    CHECK(!resolveCreative1MgmRomAddress(invalidOriginalPitch,0,524288));
    auto negativeCorrection=bankSf2;
    negativeCorrection[sampleStart+41u]=static_cast<std::uint8_t>(-7);
    const auto signedPitch=resolveCreative1MgmRomAddress(
        negativeCorrection,0,524288);
    CHECK(signedPitch && signedPitch->pitch_correction==-7);
    // Format selection MUST come from RIFF INFO ifil in BOTH native
    // readers. Without ifil, this 92-byte named shdr is not a 48-byte
    // packed SBK and must be rejected, not guessed from coincidences.
    auto sf2MissingIfil=bankSf2;
    // Synthetic fixture contains exactly one "ifil" identifier.
    const std::array<std::uint8_t,4> ifilTag{'i','f','i','l'};
    const auto loc=std::search(sf2MissingIfil.begin(),sf2MissingIfil.end(),
                               ifilTag.begin(),ifilTag.end());
    CHECK(loc!=sf2MissingIfil.end());
    std::fill_n(loc,4,static_cast<std::uint8_t>('z'));
    CHECK(!resolveCreativeSbkLayers(sf2MissingIfil,0,0,60,90));
    CHECK(!resolveCreative1MgmRomAddress(sf2MissingIfil,0,524288));
    // Corrupt the source-format major: the reader must neither accept
    // undocumented SF version3 nor fall back to old bank48 layout.
    auto sf2UnknownVersion=bankSf2;
    const auto v=std::search(sf2UnknownVersion.begin(),sf2UnknownVersion.end(),
                             ifilTag.begin(),ifilTag.end());
    CHECK(v!=sf2UnknownVersion.end());
    const auto pos=static_cast<std::size_t>(v-sf2UnknownVersion.begin());
    CHECK(pos+10<sf2UnknownVersion.size());
    sf2UnknownVersion[pos+8]=3; // byte 8 after FourCC = major low-byte
    CHECK(!resolveCreativeSbkLayers(sf2UnknownVersion,0,0,60,90));
    CHECK(!resolveCreative1MgmRomAddress(sf2UnknownVersion,0,524288));
    std::cout<<"P002_CREATIVE_SBK_2_LAYER_NATIVE_CONTRACT_PASS cases=16 private_bytes=NONE\n";
}
