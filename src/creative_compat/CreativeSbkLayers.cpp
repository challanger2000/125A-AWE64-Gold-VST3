#include "CreativeSbkLayers.h"
#include <array>
#include <algorithm>
#include <cstdint>
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
std::optional<std::int16_t>
candidateOriginalSf2InstrumentLoadGenerator(
    const std::uint16_t op,const std::int16_t raw) noexcept {
    if(op>58u)return std::nullopt;
    const auto x=static_cast<std::int32_t>(raw);
    std::int32_t output=x;
    // Exact integer-only ORIGINAL PCOD +0x3985, no SoundFont pow/log
    // approximations; C++ signed integer division truncates toward zero.
    switch(op) {
        case 5u:case 6u:case 7u:
            output=(x*6991+32768)/65536;break;
        case 8u:output=(x+15)/29-153;break;
        case 9u:output=x/12;break;
        case 10u:output=(x*2329+32768)/65536;break;
        case 11u:output=(x*1165+32768)/65536;break;
        case 13u:output=(x*128)/120;break; // x<<7 without signed shift UB
        case 29u:case 37u:output=127+(-127*x)/1000;break;
        case 51u:output=100*x;break;
        default:break;
    }
    // The authentic original bank compiler stores the low WORD of
    // each converter output in its internal compressed generator table.
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(output));
}
std::optional<std::int16_t>
candidateOriginalSf2PresetDeltaGenerator(
    const std::uint16_t op,const std::int16_t raw) noexcept {
    const auto converted=candidateOriginalSf2InstrumentLoadGenerator(op,raw);
    if(!converted)return std::nullopt;
    // Original PCOD+0x3AF7: a subset of preset generators are OFFSETS
    // relative to the instrument's load converter's C(op,0).
    switch(op) {
        case 5u:case 6u:case 7u:case 8u:case 9u:case 10u:
        case 11u:case 13u:case 29u:case 37u:case 46u:case 47u:
        case 49u:case 51u:case 52u:case 56u:case 58u: {
            const auto zero=candidateOriginalSf2InstrumentLoadGenerator(op,0);
            if(!zero)return std::nullopt;
            const auto delta=static_cast<std::int32_t>(*converted)-
                             static_cast<std::int32_t>(*zero);
            return static_cast<std::int16_t>(static_cast<std::uint16_t>(delta));
        }
        default:return converted;
    }
}
std::optional<std::int16_t>
candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
    const SbkLayer& layer,const std::uint16_t generator_id,
    const std::optional<std::int16_t> preset_default,
    const std::optional<std::int16_t> instrument_default) noexcept {
    // Original format-2 PCOD conversion and separate signed-saturated
    // preset/instrument merge. Only numerical VOICE fields supported by
    // original PCOD conversion/delta branches are eligible. Structural
    // operators (key/velocity ranges, instrument/sample IDs, key number,
    // sample mode, exclusive class and root key) are NOT additive fields.
    // This subset is not proof of default-template or complete voice parity.
    if(layer.source_format!=OriginalCreativeBankFormat::Format2Sf2)
        return std::nullopt;
    switch(generator_id) {
        case 5u: case 6u: case 7u:  // modulation pitch destinations
        case 8u: case 9u:           // filter cutoff and resonance
        case 10u: case 11u:         // modulation filter destinations
        case 13u:                   // mod-LFO volume
        case 21u: case 22u: case 23u: case 24u: // LFO delay / frequency
        case 25u: case 26u: case 27u: case 28u: case 29u: case 30u:
        case 31u: case 32u: case 33u: case 34u: case 35u: case 36u:
        case 37u: case 38u: case 39u: case 40u: // envelope and key scaling
        case 48u:                   // attenuation/instVol
        case 51u: case 52u:         // coarse/fine tune
            break;
        default:return std::nullopt;
    }
    const auto preset=layer.preset_local.get(generator_id).has_value()
        ? layer.preset_local.get(generator_id)
        : layer.preset_global.get(generator_id);
    const auto inst=layer.instrument_local.get(generator_id).has_value()
        ? layer.instrument_local.get(generator_id)
        : layer.instrument_global.get(generator_id);
    const auto p=preset
        ? candidateOriginalSf2PresetDeltaGenerator(
            generator_id,static_cast<std::int16_t>(*preset)):preset_default;
    const auto i=inst
        ? candidateOriginalSf2InstrumentLoadGenerator(
            generator_id,static_cast<std::int16_t>(*inst)):instrument_default;
    if(!p || !i)return std::nullopt;
    const auto joined=std::clamp<std::int32_t>(
        static_cast<std::int32_t>(*p)+static_cast<std::int32_t>(*i),
        -32768,32767);
    return static_cast<std::int16_t>(joined);
}
std::optional<std::int16_t>
candidateOriginalSf2InstVolFromExplicitScopes(
    const SbkLayer& layer,
    const std::optional<std::int16_t> preset_default,
    const std::optional<std::int16_t> instrument_default) noexcept {
    // Descriptor +0x60 is generator48; unlike original FORMAT-1's
    // (127-x)*75/10 this format-2 conversion is identity and additive.
    return candidateOriginalSf2MergedVoiceGeneratorFromExplicitScopes(
        layer,48u,preset_default,instrument_default);
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
    Chunk isng{},iprd{},irom{},ifil{};
    if(!visitChunks(b,info.begin+4,info.begin+info.len,[&](std::size_t p,Chunk c) noexcept {
        if(equals(b,p,"isng")){if(isng.present)return false;isng=c;}
        if(equals(b,p,"IPRD")){if(iprd.present)return false;iprd=c;}
        if(equals(b,p,"irom")){if(irom.present)return false;irom=c;}
        if(equals(b,p,"ifil")){if(ifil.present)return false;ifil=c;}
        return true;
    }) || !contains(b,isng,"EMU8000") || !contains(b,iprd,"SBAWE32") ||
       !contains(b,irom,"1MGM"))return std::nullopt;
    constexpr std::array<std::string_view,7> tags{"phdr","pbag","pgen","inst","ibag","igen","shdr"};
    std::array<std::size_t,7> strides{38,4,4,22,4,4,48};
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
    // Original owner SYNTHGM.SBK declares ifil=1.0, with 48-byte
    // shdr containing THREE packed 16-byte ROM address records.
    // The separately verified original ROM SF2 declares ifil=2.0
    // with STANDARD 46-byte named sample headers. Never pick a stride
    // just by divisibility: format is determined from the INFO tag.
    OriginalCreativeBankFormat sourceFormat=OriginalCreativeBankFormat::Format1Sbk;
    if(ifil.present) {
        if(ifil.len!=4)return std::nullopt;
        const auto major=u16(b,ifil.begin);
        if(major==2u) {
            sourceFormat=OriginalCreativeBankFormat::Format2Sf2;
            strides[SHDR]=46;
        } else if(major!=1u)return std::nullopt;
    }
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
            // Real original format-2 VXD instrument builder stops at
            // first sampleId; a late keyRange (Bird Tweet preset 123)
            // MUST NOT retroactively exclude the selected voice.
            if(sourceFormat==OriginalCreativeBankFormat::Format2Sf2 &&
               gen==IGEN && op==53u)break;
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
                out.source_format=sourceFormat;
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
    Chunk isng{},iprd{},irom{},ifil{};
    if(!visitChunks(b,info.begin+4,info.begin+info.len,[&](std::size_t p,Chunk c) noexcept {
        if(equals(b,p,"isng")){if(isng.present)return false;isng=c;}
        if(equals(b,p,"IPRD")){if(iprd.present)return false;iprd=c;}
        if(equals(b,p,"irom")){if(irom.present)return false;irom=c;}
        if(equals(b,p,"ifil")){if(ifil.present)return false;ifil=c;}
        return true;
    }) || !contains(b,isng,"EMU8000") || !contains(b,iprd,"SBAWE32") ||
       !contains(b,irom,"1MGM"))return std::nullopt;
    Chunk shdr{};
    if(!visitChunks(b,pdta.begin+4,pdta.begin+pdta.len,[&](std::size_t p,Chunk c) noexcept {
        if(equals(b,p,"shdr")){if(shdr.present)return false;shdr=c;}
        return true;
    }) || !shdr.present || !shdr.len)return std::nullopt;
    std::size_t stride=16u, addressStart=0u;
    if(ifil.present) {
        if(ifil.len!=4)return std::nullopt;
        const auto major=u16(b,ifil.begin);
        if(major==2u) {
            // Standard 46-byte SF2 shdr: 20-byte name precedes four
            // ROM word addresses. The final header is EOS.
            if(shdr.len%46u)return std::nullopt;
            stride=46u;addressStart=20u;
        } else if(major==1u) {
            if(shdr.len%48u)return std::nullopt;
            stride=16u;
        } else return std::nullopt;
    } else {
        // Historical synthetic fixtures omit ifil: accept only
        // unambiguously old 48-byte original SBK geometry.
        if(shdr.len%48u)return std::nullopt;
    }
    const auto entries=shdr.len/stride;
    if(sample_id>=entries)return std::nullopt;
    if(addressStart==20u) {
        // Format-2 SF2 has a 46-byte SHDR including sampleType +44.
        // This ROM-specific resolver cannot resolve ordinary RAM samples,
        // linked stereo pairs or terminal EOS records as ROM audio.
        // 0x8001 = ROM mono, as in the verified Creative 1MGM SF2 case.
        if(std::size_t(sample_id)+1u>=entries ||
           u16(b,shdr.begin+std::size_t(sample_id)*stride+44u)!=0x8001u)
            return std::nullopt;
    }
    const auto offset=shdr.begin+std::size_t(sample_id)*stride+addressStart;
    const auto start=u32(b,offset),end=u32(b,offset+4);
    const auto loop_start=u32(b,offset+8),loop_end=u32(b,offset+12);
    if(!(start<end && start<=loop_start && loop_start<loop_end &&
         loop_end<=end && end<=rom_words))return std::nullopt;
    return SbkRomAddress{sample_id,start,end,loop_start,loop_end};
}
}
