#include "OriginalCreativeSf2SourceReader.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace awe64::original_vxd;
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"SF2_READER_FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
using B=std::vector<std::uint8_t>;
static void u16(B& b,std::uint16_t v){b.push_back(static_cast<std::uint8_t>(v));b.push_back(static_cast<std::uint8_t>(v>>8));}
static void u32(B& b,std::uint32_t v){u16(b,static_cast<std::uint16_t>(v));u16(b,static_cast<std::uint16_t>(v>>16));}
static void chunk(B& b,const char (&name)[5],B const& d){for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(name[i]));u32(b,static_cast<std::uint32_t>(d.size()));b.insert(b.end(),d.begin(),d.end());if(d.size()&1u)b.push_back(0u);}
static void list(B& b,const char (&name)[5],B const& d){B x;for(unsigned i=0;i<4;++i)x.push_back(static_cast<std::uint8_t>(name[i]));x.insert(x.end(),d.begin(),d.end());chunk(b,"LIST",x);}
static void phdr(B& b,std::uint16_t program,std::uint16_t bank,std::uint16_t bag){b.insert(b.end(),20u,0u);u16(b,program);u16(b,bank);u16(b,bag);b.insert(b.end(),12u,0u);}
static void inst(B& b,std::uint16_t bag){b.insert(b.end(),20u,0u);u16(b,bag);}
static void bag(B& b,std::uint16_t gen,std::uint16_t mod=0u){u16(b,gen);u16(b,mod);}
static void gen(B& b,std::uint16_t op,std::uint16_t amount){u16(b,op);u16(b,amount);}
static void sample(B& b,std::uint32_t start,std::uint32_t end,std::uint32_t ls,std::uint32_t le,std::uint16_t link,std::uint16_t kind){b.insert(b.end(),20u,0u);u32(b,start);u32(b,end);u32(b,ls);u32(b,le);u32(b,44100u);b.push_back(60u);b.push_back(0u);u16(b,link);u16(b,kind);}
struct Fixture {B file{};std::size_t pbag_offset=0u;std::size_t igen_offset=0u;std::size_t smpl_offset=0u;};
static Fixture make_fixture(std::uint16_t sampleTypeA=2u,std::uint16_t sampleTypeB=4u,bool modulated=false,bool padded_loop_end=false){
    B info,ifil;u16(ifil,2);u16(ifil,1);chunk(info,"ifil",ifil);
    B pd,ph,pb,pg,in,ib,ig,im,sh,sampleData(2400u,0u);
    phdr(ph,7u,1u,0u);phdr(ph,0u,0u,2u);
    bag(pb,0u);bag(pb,1u);bag(pb,3u);
    gen(pg,8u,100u);gen(pg,43u,static_cast<std::uint16_t>((65u<<8u)|60u));gen(pg,41u,0u);
    inst(in,0u);inst(in,3u);
    bag(ib,0u,0u);bag(ib,1u,0u);bag(ib,4u,modulated?1u:0u);bag(ib,7u,modulated?1u:0u);
    gen(ig,8u,88u);
    gen(ig,43u,static_cast<std::uint16_t>((64u<<8u)|60u));
    gen(ig,44u,static_cast<std::uint16_t>((127u<<8u)|1u));
    gen(ig,53u,0u);
    gen(ig,43u,static_cast<std::uint16_t>((64u<<8u)|60u));
    gen(ig,44u,static_cast<std::uint16_t>((127u<<8u)|1u));
    gen(ig,53u,1u);
    sample(sh,100u,300u,150u,padded_loop_end?302u:250u,1u,sampleTypeA);
    sample(sh,400u,600u,450u,550u,0u,sampleTypeB);
    sample(sh,1200u,1200u,1200u,1200u,0u,1u);
    chunk(pd,"phdr",ph);chunk(pd,"pbag",pb);chunk(pd,"pgen",pg);
    if(modulated) {
        // One 10-byte instrument modulator for the first zone, followed
        // by an ordinary terminal imod record.
        u16(im,2u);u16(im,8u);u16(im,50u);u16(im,0u);u16(im,0u);
    }
    im.insert(im.end(),10u,0u);
    chunk(pd,"inst",in);chunk(pd,"ibag",ib);chunk(pd,"igen",ig);
    chunk(pd,"imod",im);
    chunk(pd,"shdr",sh);
    B sd;chunk(sd,"smpl",sampleData);
    B body;for(char c:"sfbk"){if(c)body.push_back(static_cast<std::uint8_t>(c));}
    list(body,"INFO",info);list(body,"sdta",sd);list(body,"pdta",pd);
    Fixture fixture;for(char c:"RIFF"){if(c)fixture.file.push_back(static_cast<std::uint8_t>(c));}
    u32(fixture.file,static_cast<std::uint32_t>(body.size()));
    fixture.file.insert(fixture.file.end(),body.begin(),body.end());return fixture;
}
int main(){
    auto f=make_fixture();
    auto r=select_sf2_raw_layers(f.file,1u,7u,60u,100u);
    REQUIRE(r.status==Sf2ReadStatus::Ok && r.count==2u);
    REQUIRE(r.layers[0].sample_id==0u && r.layers[1].sample_id==1u);
    REQUIRE(r.layers[0].sample.sample_type==2u && r.layers[1].sample.sample_type==4u);
    REQUIRE(r.layers[0].sample.sample_link==1u && r.layers[1].sample.sample_link==0u);
    REQUIRE(r.layers[0].preset_global.has(8u) && r.layers[0].preset_global.get(8u)==100u);
    REQUIRE(r.layers[0].instrument_global.has(8u) && r.layers[0].instrument_global.get(8u)==88u);
    REQUIRE(r.layers[0].preset_local.has(43u) && r.layers[0].instrument_local.has(53u));
    REQUIRE(r.layers[1].preset_row==0u && r.layers[1].instrument_bag==2u);
    REQUIRE(select_sf2_raw_layers(f.file,1u,7u,65u,100u).status==Sf2ReadStatus::NoMatch);
    REQUIRE(select_sf2_raw_layers(f.file,1u,7u,60u,0u).status==Sf2ReadStatus::Malformed);
    REQUIRE(select_sf2_raw_layers(f.file,3u,7u,60u,100u).status==Sf2ReadStatus::NoMatch);
    REQUIRE(select_sf2_raw_layers(f.file,1u,7u,60u,100u).count==2u);
    auto broken=f.file;broken.pop_back();
    REQUIRE(select_sf2_raw_layers(broken,1u,7u,60u,100u).status==Sf2ReadStatus::Malformed);
    broken=f.file;broken[4u]=0xffu;broken[5u]=0xffu;broken[6u]=0xffu;broken[7u]=0x7fu;
    REQUIRE(select_sf2_raw_layers(broken,1u,7u,60u,100u).status==Sf2ReadStatus::Malformed);
    // RAM samples require 'sdta/smpl'; no external ROM assumption.
    f=make_fixture(0x8002u,0x8004u);
    r=select_sf2_raw_layers(f.file,1u,7u,60u,100u);
    REQUIRE(r.status==Sf2ReadStatus::Ok && r.count==2u);
    // A bogus terminal pointer must not be accepted by the scan.
    broken=f.file;
    for(std::size_t i=0;i+4u<broken.size();++i){
        if(broken[i]=='p' && broken[i+1u]=='b' && broken[i+2u]=='a' && broken[i+3u]=='g'){
            const auto terminal=i+8u+8u;broken[terminal]=0xffu;broken[terminal+1u]=0x7fu;break;
        }
    }
    REQUIRE(select_sf2_raw_layers(broken,1u,7u,60u,100u).status==Sf2ReadStatus::Malformed);
    f=make_fixture(2u,4u,true);
    r=select_sf2_raw_layers(f.file,1u,7u,60u,100u);
    REQUIRE(r.status==Sf2ReadStatus::Ok && r.count==2u);
    REQUIRE(r.layers[0].instrument_local_mod.count==1u);
    REQUIRE(r.layers[0].instrument_local_mod.entries[0].source==2u);
    REQUIRE(r.layers[0].instrument_local_mod.entries[0].destination==8u);
    REQUIRE(r.layers[0].instrument_local_mod.entries[0].amount==50);
    REQUIRE(r.layers[1].instrument_local_mod.count==0u);
    // Actual Creative-owner bank has seven samples whose loopEnd is end+2.
    // The source reader must preserve both fields rather than rejecting or
    // silently clamping them. Only the *verified* note-start path chooses
    // the active looping/nonlooping branch.
    const auto padded=make_fixture(2u,4u,false,true);
    auto padded_result=select_sf2_raw_layers(padded.file,1u,7u,60u,100u);
    REQUIRE(padded_result.status==Sf2ReadStatus::Ok && padded_result.count==2u);
    REQUIRE(padded_result.layers[0].sample.end==300u);
    REQUIRE(padded_result.layers[0].sample.loop_end==302u);
    // More than the owner-observed two guard words is NOT authenticated.
    auto outside_guard=padded.file;
    for(std::size_t i=0;i+8u<outside_guard.size();++i){
        if(outside_guard[i]=='s' && outside_guard[i+1u]=='h' &&
           outside_guard[i+2u]=='d' && outside_guard[i+3u]=='r'){
            const std::size_t loopend=i+8u+32u;
            outside_guard[loopend]=0x30u;outside_guard[loopend+1u]=0x01u;
            break;
        }
    }
    REQUIRE(select_sf2_raw_layers(outside_guard,1u,7u,60u,100u).status==Sf2ReadStatus::Unsupported);
    std::puts("SF2_SOURCE_READER_PASS");
}
