#include "OriginalCreativeSf2SourceReader.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace awe64::original_vxd {
namespace {
using Bytes=std::span<const std::uint8_t>;
struct Area {std::size_t off=0,len=0;bool yes=false;};
[[nodiscard]] bool has(Bytes b,std::size_t p,std::size_t n) noexcept {
    return p<=b.size() && n<=b.size()-p;
}
[[nodiscard]] std::uint16_t r16(Bytes b,std::size_t p) noexcept {
    return static_cast<std::uint16_t>(b[p])|
           static_cast<std::uint16_t>(static_cast<unsigned>(b[p+1u])<<8u);
}
[[nodiscard]] std::uint32_t r32(Bytes b,std::size_t p) noexcept {
    return static_cast<std::uint32_t>(r16(b,p))|
           (static_cast<std::uint32_t>(r16(b,p+2u))<<16u);
}
[[nodiscard]] bool tag(Bytes b,std::size_t p,char const (&str)[5]) noexcept {
    return has(b,p,4u) && b[p]==static_cast<std::uint8_t>(str[0]) &&
       b[p+1u]==static_cast<std::uint8_t>(str[1]) &&
       b[p+2u]==static_cast<std::uint8_t>(str[2]) &&
       b[p+3u]==static_cast<std::uint8_t>(str[3]);
}
template<class F>
[[nodiscard]] bool visit(Bytes b,std::size_t start,std::size_t end,F&& f) noexcept {
    if(start>end || end>b.size())return false;
    auto p=start;
    while(p<end) {
        if(end-p<8u)return false;
        const auto length=static_cast<std::size_t>(r32(b,p+4u));
        const auto content=p+8u;
        if(length>end-content || !f(p,Area{content,length,true}))return false;
        const auto padded=length+(length&1u);
        if(padded>end-content)return false;
        p=content+padded;
    }
    return p==end;
}
struct Tables {
    Bytes data{};
    Area phdr{},pbag{},pgen{},pmod{},inst{},ibag{},igen{},imod{},shdr{},smpl{};
    std::size_t np=0,nb=0,ng=0,npm=0,ni=0,nib=0,nig=0,nim=0,ns=0;
    [[nodiscard]] std::uint16_t pbag_gen(std::size_t i)const noexcept {return r16(data,pbag.off+4u*i);}
    [[nodiscard]] std::uint16_t pbag_mod(std::size_t i)const noexcept {return r16(data,pbag.off+4u*i+2u);}
    [[nodiscard]] std::uint16_t ibag_gen(std::size_t i)const noexcept {return r16(data,ibag.off+4u*i);}
    [[nodiscard]] std::uint16_t ibag_mod(std::size_t i)const noexcept {return r16(data,ibag.off+4u*i+2u);}
};
[[nodiscard]] bool parse(Bytes bytes,Tables& t) noexcept {
    if(bytes.size()<12u || !tag(bytes,0u,"RIFF") || !tag(bytes,8u,"sfbk") ||
       static_cast<std::uint64_t>(r32(bytes,4u))+8u!=bytes.size())return false;
    Area info{},pdta{},sdta{};
    const bool lists=visit(bytes,12u,bytes.size(),[&](std::size_t p,Area a) noexcept {
        if(!tag(bytes,p,"LIST"))return true;
        if(a.len<4u)return false;
        if(tag(bytes,a.off,"INFO")){if(info.yes)return false; info=a;}
        if(tag(bytes,a.off,"pdta")){if(pdta.yes)return false;pdta=a;}
        if(tag(bytes,a.off,"sdta")){if(sdta.yes)return false;sdta=a;}
        return true;
    });
    if(!lists || !info.yes || !pdta.yes)return false;
    Area ifil{};
    if(!visit(bytes,info.off+4u,info.off+info.len,[&](std::size_t p,Area a) noexcept {
        if(tag(bytes,p,"ifil")){if(ifil.yes)return false;ifil=a;}
        return true;
    }) || !ifil.yes || ifil.len!=4u || r16(bytes,ifil.off)!=2u)return false;
    const auto ok=visit(bytes,pdta.off+4u,pdta.off+pdta.len,[&](std::size_t p,Area a) noexcept {
        Area* slot=nullptr;
        if(tag(bytes,p,"phdr"))slot=&t.phdr;
        else if(tag(bytes,p,"pbag"))slot=&t.pbag;
        else if(tag(bytes,p,"pgen"))slot=&t.pgen;
        else if(tag(bytes,p,"pmod"))slot=&t.pmod;
        else if(tag(bytes,p,"inst"))slot=&t.inst;
        else if(tag(bytes,p,"ibag"))slot=&t.ibag;
        else if(tag(bytes,p,"igen"))slot=&t.igen;
        else if(tag(bytes,p,"imod"))slot=&t.imod;
        else if(tag(bytes,p,"shdr"))slot=&t.shdr;
        if(slot){if(slot->yes)return false;*slot=a;}
        return true;
    });
    if(!ok || !t.phdr.yes || !t.pbag.yes || !t.pgen.yes || !t.inst.yes ||
       !t.ibag.yes || !t.igen.yes || !t.shdr.yes ||
       t.phdr.len%38u || t.pbag.len%4u || t.pgen.len%4u ||
       t.inst.len%22u || t.ibag.len%4u || t.igen.len%4u ||
       t.shdr.len%46u ||
       (t.pmod.yes && t.pmod.len%10u) || (t.imod.yes && t.imod.len%10u))return false;
    t.data=bytes;
    t.np=t.phdr.len/38u;t.nb=t.pbag.len/4u;t.ng=t.pgen.len/4u;
    t.ni=t.inst.len/22u;t.nib=t.ibag.len/4u;t.nig=t.igen.len/4u;
    t.ns=t.shdr.len/46u;
    t.npm=t.pmod.yes?t.pmod.len/10u:0u;
    t.nim=t.imod.yes?t.imod.len/10u:0u;
    if(t.np<2u || t.nb<2u || t.ni<2u || t.nib<2u || t.ns<2u)return false;
    // All sentinel indexes must fit, even if a malformed non-selected
    // instrument is never requested. Enforce increasing finite tables.
    if(r16(bytes,t.phdr.off+38u*(t.np-1u)+24u)>=t.nb ||
       r16(bytes,t.inst.off+22u*(t.ni-1u)+20u)>=t.nib ||
       t.pbag_gen(t.nb-1u)>t.ng || t.ibag_gen(t.nib-1u)>t.nig ||
       t.pbag_mod(t.nb-1u)>t.npm || t.ibag_mod(t.nib-1u)>t.nim)return false;
    if(sdta.yes) {
        if(!visit(bytes,sdta.off+4u,sdta.off+sdta.len,[&](std::size_t p,Area a) noexcept {
            if(tag(bytes,p,"smpl")){if(t.smpl.yes)return false;t.smpl=a;}
            return true;
        }))return false;
        if(t.smpl.yes && (t.smpl.len&1u)!=0u)return false;
    }
    return true;
}
[[nodiscard]] bool get_scope(Tables const& t,bool preset,std::size_t bag,
    Sf2GeneratorScope& out,Sf2ModulatorScope& mods) noexcept {
    const auto bags=preset?t.nb:t.nib;
    if(bag+1u>=bags)return false;
    const auto first=preset?t.pbag_gen(bag):t.ibag_gen(bag);
    const auto last=preset?t.pbag_gen(bag+1u):t.ibag_gen(bag+1u);
    const auto fm=preset?t.pbag_mod(bag):t.ibag_mod(bag);
    const auto lm=preset?t.pbag_mod(bag+1u):t.ibag_mod(bag+1u);
    if(first>last || last>(preset?t.ng:t.nig) || fm>lm ||
       lm>(preset?t.npm:t.nim) || static_cast<std::size_t>(lm-fm)>sf2_max_scope_modulators)return false;
    if(fm!=lm) {
        const auto mbase=preset?t.pmod.off:t.imod.off;
        if(!(preset?t.pmod.yes:t.imod.yes))return false;
        for(auto mi=fm;mi<lm;++mi) {
            const auto at=mbase+static_cast<std::size_t>(mi)*10u;
            auto& record=mods.entries[mods.count++];
            record.source=r16(t.data,at);
            record.destination=r16(t.data,at+2u);
            record.amount=static_cast<std::int16_t>(r16(t.data,at+4u));
            record.amount_source=r16(t.data,at+6u);
            record.transform=r16(t.data,at+8u);
        }
    }
    const auto base=preset?t.pgen.off:t.igen.off;
    for(std::size_t i=first;i<last;++i) {
        const auto op=r16(t.data,base+4u*i);
        if(op>=59u) {
            // EndOper (60) and future unverified generators cannot
            // silently influence the original source descriptor.
            return false;
        }
        out.present|=std::uint64_t{1}<<op;
        out.raw[op]=r16(t.data,base+4u*i+2u);
        if(!preset && op==53u)break; // source PCOD stops at sampleID
    }
    return true;
}
[[nodiscard]] bool matches(Sf2GeneratorScope const& s,unsigned op,
    std::uint8_t value) noexcept {
    if(!s.has(op))return true;
    const auto x=s.get(op);
    const auto lo=static_cast<unsigned>(x&255u);
    const auto hi=static_cast<unsigned>(x>>8u);
    return lo<=hi && hi<=127u && value>=lo && value<=hi;
}
[[nodiscard]] Sf2ReadStatus sample(Tables const& t,std::size_t id,Sf2SampleHeader& s) noexcept {
    if(id+1u>=t.ns)return Sf2ReadStatus::Malformed; // EOS is not a voice
    const auto p=t.shdr.off+id*46u;
    s.start=r32(t.data,p+20u);s.end=r32(t.data,p+24u);
    s.loop_start=r32(t.data,p+28u);s.loop_end=r32(t.data,p+32u);
    s.sample_rate=r32(t.data,p+36u);
    s.original_pitch=t.data[p+40u];s.correction=static_cast<std::int8_t>(t.data[p+41u]);
    s.sample_link=r16(t.data,p+42u);s.sample_type=r16(t.data,p+44u);
    const auto kind=s.sample_type&0x7fffu;
    if((kind!=1u && kind!=2u && kind!=4u) ||
       s.start>=s.end || s.sample_rate==0u || s.original_pitch>127u)
        return Sf2ReadStatus::Malformed;
    const bool ram=(s.sample_type&0x8000u)==0u;
    if(ram && (!t.smpl.yes || s.end>t.smpl.len/2u))
        return Sf2ReadStatus::Malformed;
    if((kind==2u || kind==4u) &&
       (s.sample_link==id || static_cast<std::size_t>(s.sample_link)+1u>=t.ns))
        return Sf2ReadStatus::Malformed;
    // Seven samples in the actual owner's original Win95-loaded bank have
    // loopEnd == end+2. Original LCOD+0x5B87/0x5B90/0x5BC8 uses the two
    // loop coordinates ONLY when sampleModes bit0 is set; otherwise it
    // programs end+4/end+8. Keep the *raw* header (never clamp it).
    // Allow only the observed +2 guard-word geometry, and require that the
    // PCM pool actually holds those guard words. The descriptor compiler
    // separately checks the effective loop mode and hardware RAM bounds.
    if(s.start>s.loop_start || s.loop_start>=s.loop_end ||
       (s.loop_end>s.end && s.loop_end-s.end>2u) ||
       (ram && s.loop_end>t.smpl.len/2u))
        return Sf2ReadStatus::Unsupported;
    return Sf2ReadStatus::Ok;
}
}
std::optional<std::span<const std::uint8_t>>
source_sf2_raw_sample_pool(Bytes bytes) noexcept {
    Tables t{};
    if(!parse(bytes,t) || !t.smpl.yes)return std::nullopt;
    return bytes.subspan(t.smpl.off,t.smpl.len);
}
Sf2LayerSelection select_sf2_raw_layers(Bytes bytes,
    std::uint16_t bank,std::uint8_t program,
    std::uint8_t key,std::uint8_t velocity) noexcept {
    Sf2LayerSelection result{};
    if(program>127u || key>127u || velocity==0u || velocity>127u)return result;
    Tables t{};
    if(!parse(bytes,t))return result;
    result.status=Sf2ReadStatus::NoMatch;
    for(std::size_t pi=0;pi+1u<t.np;++pi) {
        const auto ph=t.phdr.off+38u*pi;
        if(r16(t.data,ph+20u)!=program || r16(t.data,ph+22u)!=bank)continue;
        const auto b0=r16(t.data,ph+24u);
        const auto b1=r16(t.data,ph+38u+24u);
        if(b0>=b1 || b1>=t.nb) {result.status=Sf2ReadStatus::Malformed;return result;}
        Sf2GeneratorScope pg{};Sf2ModulatorScope pgmods{};
        for(auto pb=b0;pb<b1;++pb) {
            Sf2GeneratorScope local{};Sf2ModulatorScope localmods{};
            if(!get_scope(t,true,pb,local,localmods)) {result.status=Sf2ReadStatus::Unsupported;return result;}
            if(!local.has(41u)) {
                if(pb!=b0 || local.has(43u) || local.has(44u)){
                    result.status=Sf2ReadStatus::Malformed;return result;
                }
                pg=local;pgmods=localmods;continue;
            }
            if(!matches(local,43u,key) || !matches(local,44u,velocity))continue;
            const auto instrument=local.get(41u);
            if(static_cast<std::size_t>(instrument)+1u>=t.ni){result.status=Sf2ReadStatus::Malformed;return result;}
            const auto ins=t.inst.off+22u*instrument;
            const auto ib0=r16(t.data,ins+20u),ib1=r16(t.data,ins+22u+20u);
            if(ib0>=ib1 || ib1>=t.nib){result.status=Sf2ReadStatus::Malformed;return result;}
            Sf2GeneratorScope ig{};Sf2ModulatorScope igmods{};
            for(auto ib=ib0;ib<ib1;++ib) {
                Sf2GeneratorScope g{};Sf2ModulatorScope gmods{};
                if(!get_scope(t,false,ib,g,gmods)){result.status=Sf2ReadStatus::Unsupported;return result;}
                if(!g.has(53u)) {
                    if(ib!=ib0 || g.has(43u) || g.has(44u)){
                        result.status=Sf2ReadStatus::Malformed;return result;
                    }
                    ig=g;igmods=gmods;continue;
                }
                if(!matches(g,43u,key) || !matches(g,44u,velocity))continue;
                if(result.count>=sf2_max_selected_layers){result.status=Sf2ReadStatus::TooManyLayers;return result;}
                auto& out=result.layers[result.count];
                const auto sh=sample(t,g.get(53u),out.sample);
                if(sh!=Sf2ReadStatus::Ok){result.status=sh;return result;}
                out.bank=bank;out.program=program;out.key=key;out.velocity=velocity;
                out.preset_row=static_cast<std::uint16_t>(pi);out.preset_bag=pb;
                out.instrument=instrument;out.instrument_bag=ib;
                out.sample_id=g.get(53u);
                out.preset_global=pg;out.preset_local=local;
                out.instrument_global=ig;out.instrument_local=g;
                out.preset_global_mod=pgmods;out.preset_local_mod=localmods;
                out.instrument_global_mod=igmods;out.instrument_local_mod=gmods;
                ++result.count;
            }
        }
    }
    if(result.count!=0u)result.status=Sf2ReadStatus::Ok;
    return result;
}
} // namespace awe64::original_vxd
