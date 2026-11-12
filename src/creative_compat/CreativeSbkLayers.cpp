#include "CreativeSbkLayers.h"
#include <array>
#include <string_view>
namespace awe64::creative {
namespace {
using Bytes=std::span<const std::uint8_t>;
struct Chunk { std::size_t begin=0,len=0; bool present=false; };
std::uint16_t u16(Bytes b,std::size_t off) noexcept {
    return std::uint16_t(b[off]) | (std::uint16_t(b[off+1])<<8);
}
std::uint32_t u32(Bytes b,std::size_t off) noexcept {
    return std::uint32_t(u16(b,off)) | (std::uint32_t(u16(b,off+2))<<16);
}
bool equals(Bytes b,std::size_t off,std::string_view s) noexcept {
    if(off>b.size() || s.size()>b.size()-off)return false;
    for(std::size_t i=0;i<s.size();++i)if(b[off+i]!=static_cast<std::uint8_t>(s[i]))return false;
    return true;
}
bool contains(Bytes b,Chunk c,std::string_view s) noexcept {
    if(!c.present || c.len<s.size())return false;
    for(std::size_t i=0;i<=c.len-s.size();++i)if(equals(b,c.begin+i,s))return true;
    return false;
}
template<class V>bool visitChunks(Bytes b,std::size_t begin,std::size_t end,V visit) noexcept {
    if(begin>end || end>b.size())return false;
    auto pos=begin;
    while(pos<end){
        if(end-pos<8)return false;
        const auto length=std::size_t(u32(b,pos+4));
        const auto data=pos+8;
        if(length>end-data)return false;
        if(!visit(pos,Chunk{data,length,true}))return false;
        pos=data+length;
        if(length&1u){if(pos==end)return false;++pos;}
    }
    return pos==end;
}
// SoundFont range word: inclusive low byte..high byte.
std::optional<bool> rangeOK(std::optional<std::uint16_t> raw,std::uint8_t value) noexcept {
    if(!raw)return true;
    const auto low=unsigned(*raw&255u),high=unsigned(*raw>>8);
    if(low>high || high>127)return std::nullopt;
    return value>=low && value<=high;
}
}
std::optional<SbkLayerSet> resolveCreativeSbkLayers(
    Bytes b,std::uint16_t bank,std::uint8_t program,
    std::uint8_t key,std::uint8_t velocity) noexcept {
    if(program>127 || key>127 || velocity==0 || velocity>127 || b.size()<12 ||
       !equals(b,0,"RIFF") || !equals(b,8,"sfbk") || u32(b,4)!=b.size()-8)
        return std::nullopt;
    Chunk info{},pdta{};
    if(!visitChunks(b,12,b.size(),[&](std::size_t p,Chunk c) noexcept {
        if(!equals(b,p,"LIST"))return true;
        if(c.len<4)return false;
        if(equals(b,c.begin,"INFO")){if(info.present)return false;info=c;}
        if(equals(b,c.begin,"pdta")){if(pdta.present)return false;pdta=c;}
        return true;
    }) || !info.present || !pdta.present)return std::nullopt;
    Chunk isng{},iprd{},irom{};
    if(!visitChunks(b,info.begin+4,info.begin+info.len,[&](std::size_t p,Chunk c) noexcept {
        if(equals(b,p,"isng")){if(isng.present)return false;isng=c;}
        if(equals(b,p,"IPRD")){if(iprd.present)return false;iprd=c;}
        if(equals(b,p,"irom")){if(irom.present)return false;irom=c;}
        return true;
    }) || !contains(b,isng,"EMU8000") || !contains(b,iprd,"SBAWE32") ||
       !contains(b,irom,"1MGM"))return std::nullopt;
    constexpr std::array<std::string_view,7> tags{"phdr","pbag","pgen","inst","ibag","igen","shdr"};
    constexpr std::array<std::size_t,7> strides{38,4,4,22,4,4,48};
    enum :std::size_t {PHDR,PBAG,PGEN,INST,IBAG,IGEN,SHDR};
    std::array<Chunk,7> table{};
    if(!visitChunks(b,pdta.begin+4,pdta.begin+pdta.len,[&](std::size_t p,Chunk c) noexcept {
        for(std::size_t i=0;i<tags.size();++i)if(equals(b,p,tags[i])){
            if(table[i].present)return false;
            table[i]=c;
            break;
        }
        return true;
    }))return std::nullopt;
    std::array<std::size_t,7> count{};
    for(std::size_t i=0;i<count.size();++i){
        if(!table[i].present || !table[i].len || table[i].len%strides[i])return std::nullopt;
        count[i]=table[i].len/strides[i];
    }
    if(count[PHDR]<2 || count[PBAG]<2 || count[INST]<2 ||
       count[IBAG]<2 || count[SHDR]<2)return std::nullopt;
    const auto word=[&](std::size_t id,std::size_t index,std::size_t offset) noexcept {
        return u16(b,table[id].begin+index*strides[id]+offset);
    };
    const auto gens=[&](std::size_t gen,std::size_t bag,std::size_t zone)->std::optional<SbkGeneratorScope> {
        if(zone+1>=count[bag])return std::nullopt;
        const auto first=word(bag,zone,0),last=word(bag,zone+1,0);
        if(first>last || last>count[gen])return std::nullopt;
        SbkGeneratorScope result;
        for(std::size_t i=first;i<last;++i){
            const auto op=word(gen,i,0);
            if(op>=64)return std::nullopt;
            result.raw[op]=word(gen,i,2);
        }
        return result;
    };
    SbkLayerSet found{};
    for(std::size_t pi=0;pi+1<count[PHDR];++pi){
        if(word(PHDR,pi,20)!=program || word(PHDR,pi,22)!=bank)continue;
        const auto first=word(PHDR,pi,24),last=word(PHDR,pi+1,24);
        if(first>=last || last>=count[PBAG])return std::nullopt;
        SbkGeneratorScope presetGlobal;
        for(std::size_t pb=first;pb<last;++pb){
            auto pg=gens(PGEN,PBAG,pb);if(!pg)return std::nullopt;
            if(!pg->get(41)){
                if(pb!=first || pg->get(43) || pg->get(44))return std::nullopt;
                presetGlobal=*pg;
                continue;
            }
            const auto presetKey=rangeOK(pg->get(43),key),presetVel=rangeOK(pg->get(44),velocity);
            if(!presetKey || !presetVel)return std::nullopt;
            if(!*presetKey || !*presetVel)continue;
            const auto instrument=std::size_t(*pg->get(41));
            if(instrument+1>=count[INST])return std::nullopt;
            const auto begin=word(INST,instrument,20),end=word(INST,instrument+1,20);
            if(begin>=end || end>=count[IBAG])return std::nullopt;
            SbkGeneratorScope instrumentGlobal;
            for(std::size_t ib=begin;ib<end;++ib){
                auto g=gens(IGEN,IBAG,ib);if(!g)return std::nullopt;
                if(!g->get(53)){
                    if(ib!=begin || g->get(43) || g->get(44))return std::nullopt;
                    instrumentGlobal=*g;
                }
            }
            for(std::size_t ib=begin;ib<end;++ib){
                auto g=gens(IGEN,IBAG,ib);if(!g)return std::nullopt;
                const auto sid=g->get(53);if(!sid)continue;
                const auto ik=rangeOK(g->get(43),key),iv=rangeOK(g->get(44),velocity);
                if(!ik || !iv)return std::nullopt;
                if(!*ik || !*iv)continue;
                if(found.count>=kMaxCreative1MgmLayers)return std::nullopt;
                auto& out=found.layers[found.count++];
                out.bank=bank;out.program=program;out.key=key;out.velocity=velocity;
                out.preset_bag=static_cast<std::uint16_t>(pb);
                out.instrument=static_cast<std::uint16_t>(instrument);
                out.instrument_bag=static_cast<std::uint16_t>(ib);
                out.rom_sample_selector=*sid;
                out.preset_global=presetGlobal;out.preset_local=*pg;
                out.instrument_global=instrumentGlobal;out.instrument_local=*g;
            }
        }
    }
    return found;
}

std::optional<SbkRomAddress> resolveCreative1MgmRomAddress(
    Bytes b, std::uint16_t sample_id, std::size_t rom_words) noexcept {
    if(b.size()<12 || !equals(b,0,"RIFF") || !equals(b,8,"sfbk") ||
       u32(b,4)!=b.size()-8 || rom_words==0)return std::nullopt;
    Chunk info{}, pdta{};
    if(!visitChunks(b,12,b.size(),[&](std::size_t p,Chunk c) noexcept {
        if(!equals(b,p,"LIST"))return true;
        if(c.len<4)return false;
        if(equals(b,c.begin,"INFO")){if(info.present)return false;info=c;}
        if(equals(b,c.begin,"pdta")){if(pdta.present)return false;pdta=c;}
        return true;
    }) || !info.present || !pdta.present)return std::nullopt;
    Chunk isng{},iprd{},irom{};
    if(!visitChunks(b,info.begin+4,info.begin+info.len,[&](std::size_t p,Chunk c) noexcept {
        if(equals(b,p,"isng")){if(isng.present)return false;isng=c;}
        if(equals(b,p,"IPRD")){if(iprd.present)return false;iprd=c;}
        if(equals(b,p,"irom")){if(irom.present)return false;irom=c;}
        return true;
    }) || !contains(b,isng,"EMU8000") || !contains(b,iprd,"SBAWE32") ||
       !contains(b,irom,"1MGM"))return std::nullopt;
    Chunk shdr{};
    if(!visitChunks(b,pdta.begin+4,pdta.begin+pdta.len,[&](std::size_t p,Chunk c) noexcept {
        if(equals(b,p,"shdr")){if(shdr.present)return false;shdr=c;}
        return true;
    }) || !shdr.present || !shdr.len || shdr.len%48u)return std::nullopt;
    // 48 bytes comprise 3 packed address records, each 4 uint32 words.
    const auto entries=shdr.len/16u;
    if(sample_id>=entries)return std::nullopt;
    const auto offset=shdr.begin+std::size_t(sample_id)*16u;
    const auto start=u32(b,offset),end=u32(b,offset+4);
    const auto loop_start=u32(b,offset+8),loop_end=u32(b,offset+12);
    if(!(start<end && start<=loop_start && loop_start<loop_end &&
         loop_end<=end && end<=rom_words))return std::nullopt;
    return SbkRomAddress{sample_id,start,end,loop_start,loop_end};
}
}
