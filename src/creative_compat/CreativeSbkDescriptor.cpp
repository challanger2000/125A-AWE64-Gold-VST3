#include "CreativeSbkDescriptor.h"
#include <array>
#include <string_view>

namespace awe64::creative {
namespace {
using Bytes = std::span<const std::uint8_t>;
struct Chunk { std::size_t at=0, length=0; bool present=false; };
std::uint16_t u16(Bytes b, std::size_t p) noexcept {
    return std::uint16_t(b[p]) | (std::uint16_t(b[p+1])<<8);
}
std::uint32_t u32(Bytes b, std::size_t p) noexcept {
    return std::uint32_t(u16(b,p)) | (std::uint32_t(u16(b,p+2))<<16);
}
bool tag(Bytes b, std::size_t p, std::string_view name) noexcept {
    if (p>b.size() || name.size()>b.size()-p) return false;
    for (std::size_t i=0;i<name.size();++i)
        if (b[p+i]!=static_cast<std::uint8_t>(name[i])) return false;
    return true;
}
bool contains(Bytes b, Chunk c, std::string_view needle) noexcept {
    if (!c.present || needle.empty() || c.length<needle.size()) return false;
    for (std::size_t i=0;i<=c.length-needle.size();++i)
        if (tag(b,c.at+i,needle)) return true;
    return false;
}
template<class Visitor>
bool walk(Bytes b, std::size_t begin, std::size_t end, Visitor visit) noexcept {
    if (begin>end || end>b.size()) return false;
    std::size_t p=begin;
    while (p<end) {
        if (end-p<8) return false;
        const auto n=static_cast<std::size_t>(u32(b,p+4));
        const auto content=p+8;
        if (n>end-content) return false;
        const Chunk c{content,n,true};
        if (!visit(p,c)) return false;
        p=content+n;
        if (n&1u) {
            if (p>=end) return false;
            ++p;
        }
    }
    return p==end;
}
struct Generators {
    std::optional<std::uint16_t> cutoff, attack, key_range, velocity_range, sample_id, instrument, attenuation;
};
void addGenerator(Generators& g, std::uint16_t op, std::uint16_t v) noexcept {
    switch(op) {
    case 8: g.cutoff=v; break;
    case 34: g.attack=v; break;
    case 43: g.key_range=v; break;
    case 44: g.velocity_range=v; break;
    case 53: g.sample_id=v; break;
    case 41: g.instrument=v; break;
    case 48: g.attenuation=v; break;
    default: break;
    }
}
} // namespace

std::optional<SbkZoneDescriptor> resolveCreativeSbkZone(
    Bytes b, std::uint16_t bank, std::uint8_t program, std::uint8_t key,
    std::uint8_t velocity) noexcept {
    if (program>127 || key>127 || velocity==0 || velocity>127 || b.size()<12 || !tag(b,0,"RIFF") ||
        !tag(b,8,"sfbk") || u32(b,4)!=b.size()-8) return std::nullopt;
    Chunk info{}, pdta{};
    if (!walk(b,12,b.size(),[&](std::size_t p,Chunk c) noexcept {
        if (!tag(b,p,"LIST")) return true;
        if (c.length<4) return false;
        if (tag(b,c.at,"INFO")) {
            if (info.present) return false;
            info=c;
        } else if (tag(b,c.at,"pdta")) {
            if (pdta.present) return false;
            pdta=c;
        }
        return true;
    }) || !info.present || !pdta.present) return std::nullopt;

    Chunk isng{}, iprd{};
    if (!walk(b,info.at+4,info.at+info.length,[&](std::size_t p,Chunk c) noexcept {
        if (tag(b,p,"isng")) { if (isng.present) return false; isng=c; }
        if (tag(b,p,"IPRD")) { if (iprd.present) return false; iprd=c; }
        return true;
    }) || !contains(b,isng,"EMU8000") || !contains(b,iprd,"SBAWE32"))
        return std::nullopt;

    constexpr std::array<std::string_view,7> names{
        "phdr","pbag","pgen","inst","ibag","igen","shdr"};
    constexpr std::array<std::size_t,7> stride{38,4,4,22,4,4,48};
    enum : std::size_t { PHDR, PBAG, PGEN, INST, IBAG, IGEN, SHDR };
    std::array<Chunk,7> tables{};
    if (!walk(b,pdta.at+4,pdta.at+pdta.length,[&](std::size_t p,Chunk c) noexcept {
        for(std::size_t t=0;t<names.size();++t) if(tag(b,p,names[t])) {
            if(tables[t].present) return false;
            tables[t]=c;
            break;
        }
        return true;
    })) return std::nullopt;
    std::array<std::size_t,7> counts{};
    for(std::size_t i=0;i<counts.size();++i) {
        if(!tables[i].present || tables[i].length==0 ||
           tables[i].length%stride[i]!=0) return std::nullopt;
        counts[i]=tables[i].length/stride[i];
    }
    if (counts[PHDR]<2 || counts[PBAG]<2 || counts[INST]<2 || counts[IBAG]<2 ||
        counts[SHDR]<2) return std::nullopt;
    const auto word=[&](std::size_t table,std::size_t index,std::size_t offset) noexcept {
        return u16(b,tables[table].at+index*stride[table]+offset);
    };
    const auto readGen=[&](std::size_t genTable,std::size_t bagTable,
                           std::size_t bag)->std::optional<Generators> {
        if(bag+1>=counts[bagTable]) return std::nullopt;
        const auto first=word(bagTable,bag,0),last=word(bagTable,bag+1,0);
        if(first>last || last>counts[genTable]) return std::nullopt;
        Generators g;
        for(std::size_t i=first;i<last;++i)
            addGenerator(g,word(genTable,i,0),word(genTable,i,2));
        return g;
    };
    // A SoundFont range is inclusive, encoded low | high<<8. A malformed
    // range rejects a bank rather than silently selecting a wrong layer.
    const auto eligible=[&](std::optional<std::uint16_t> raw,
                            std::uint8_t value)->std::optional<bool> {
        if (!raw) return true;
        const auto lo=static_cast<std::uint8_t>(*raw&0xffu);
        const auto hi=static_cast<std::uint8_t>(*raw>>8);
        if (lo>hi || hi>127) return std::nullopt;
        return value>=lo && value<=hi;
    };
    std::optional<SbkZoneDescriptor> found;
    for(std::size_t preset=0;preset+1<counts[PHDR];++preset) {
        if(word(PHDR,preset,20)!=program || word(PHDR,preset,22)!=bank) continue;
        const auto begin=word(PHDR,preset,24),end=word(PHDR,preset+1,24);
        if(begin>=end || end>=counts[PBAG]) return std::nullopt;
        for(std::size_t pb=begin;pb<end;++pb) {
            auto pg=readGen(PGEN,PBAG,pb);
            if(!pg) return std::nullopt;
            const auto presetKey=eligible(pg->key_range,key);
            const auto presetVelocity=eligible(pg->velocity_range,velocity);
            if(!presetKey || !presetVelocity) return std::nullopt;
            if(!*presetKey || !*presetVelocity) continue;
            if(!pg->instrument) continue;
            const auto inst=static_cast<std::size_t>(*pg->instrument);
            if(inst+1>=counts[INST]) return std::nullopt;
            const auto ibegin=word(INST,inst,20),iend=word(INST,inst+1,20);
            if(ibegin>=iend || iend>=counts[IBAG]) return std::nullopt;
            Generators globals;
            for(std::size_t ib=ibegin;ib<iend;++ib) {
                auto g=readGen(IGEN,IBAG,ib);
                if(!g) return std::nullopt;
                if(!g->sample_id) {
                    if(g->key_range || g->velocity_range) return std::nullopt;
                    if(g->cutoff) globals.cutoff=g->cutoff;
                    if(g->attack) globals.attack=g->attack;
                }
            }
            for(std::size_t ib=ibegin;ib<iend;++ib) {
                auto g=readGen(IGEN,IBAG,ib);
                if(!g) return std::nullopt;
                if(!g->sample_id) continue;
                // The final shdr record is an EOS terminal, not a sample.
                if(*g->sample_id>=counts[SHDR]-1) return std::nullopt;
                const auto instrumentVelocity=eligible(g->velocity_range,velocity);
                if(!instrumentVelocity) return std::nullopt;
                if(!*instrumentVelocity) continue;
                const auto range=g->key_range.value_or(0x7F00);
                const auto lo=static_cast<std::uint8_t>(range&0xFFu);
                const auto hi=static_cast<std::uint8_t>(range>>8);
                if(lo>hi || hi>127) return std::nullopt;
                if(key<lo || key>hi) continue;
                const auto cutoff=g->cutoff ? g->cutoff : globals.cutoff;
                if(!cutoff || *cutoff>127 || found) return std::nullopt;
                found=SbkZoneDescriptor{bank,program,key,velocity,static_cast<std::uint16_t>(inst),
                    static_cast<std::uint16_t>(ib),lo,hi,*g->sample_id,
                    static_cast<std::uint8_t>(*cutoff),static_cast<std::uint8_t>(*cutoff*2u),
                    globals.cutoff,g->cutoff,pg->attenuation,globals.attack};
            }
        }
    }
    return found;
}
} // namespace awe64::creative
