#include "CreativeSf2PresetInventory.h"
#include <algorithm>
#include <cstring>
#include <limits>
namespace awe64::creative {
namespace {
using Bytes=std::span<const std::uint8_t>;
struct Chunk{std::size_t at=0,len=0;bool valid=false;};
bool is(Bytes s,std::size_t p,const char* literal)noexcept {
    return p<=s.size() && s.size()-p>=4 &&
           std::memcmp(s.data()+p,literal,4)==0;
}
std::uint16_t w(Bytes b,std::size_t p)noexcept {
    return std::uint16_t(b[p]) | (std::uint16_t(b[p+1])<<8);
}
std::uint32_t d(Bytes b,std::size_t p)noexcept {
    return std::uint32_t(w(b,p)) | (std::uint32_t(w(b,p+2))<<16);
}
template<class Fn>
bool walk(Bytes b,std::size_t begin,std::size_t end,Fn visit){
    if(begin>end||end>b.size())return false;
    auto pos=begin;
    while(pos<end){
        if(end-pos<8)return false;
        const auto len=std::size_t(d(b,pos+4));
        const auto data=pos+8;
        if(len>end-data)return false;
        if(!visit(pos,Chunk{data,len,true}))return false;
        pos=data+len;
        if(len&1u){
            if(pos==end)return false;
            // RIFF padding is not an additional byte of chunk content.
            ++pos;
        }
    }
    return pos==end;
}
}
std::optional<Sf2PresetInventory>
inspectOriginalSoundFontPresetInventory(const Bytes riff){
    if(riff.size()<12u || !is(riff,0u,"RIFF") ||
       !is(riff,8u,"sfbk") ||
       d(riff,4u)!=riff.size()-8u)return std::nullopt;
    Chunk pdta{};
    if(!walk(riff,12u,riff.size(),[&](const std::size_t pos,Chunk chunk) {
        if(!is(riff,pos,"LIST"))return true;
        if(chunk.len<4u)return false;
        if(is(riff,chunk.at,"pdta")) {
            if(pdta.valid)return false;
            pdta=chunk;
        }
        return true;
    }) || !pdta.valid)return std::nullopt;
    Chunk phdr{},pbag{};
    if(!walk(riff,pdta.at+4u,pdta.at+pdta.len,
         [&](const std::size_t pos,Chunk c){
             if(is(riff,pos,"phdr")){
                 if(phdr.valid)return false;
                 phdr=c;
             }else if(is(riff,pos,"pbag")){
                 if(pbag.valid)return false;
                 pbag=c;
             }
             return true;
         }) || !phdr.valid || !pbag.valid ||
       phdr.len<2u*38u || phdr.len%38u ||
       pbag.len<2u*4u || pbag.len%4u)return std::nullopt;
    constexpr std::size_t kMaxRealPresets=8192u;
    const auto rows=phdr.len/38u;
    const auto bags=pbag.len/4u;
    if(rows-1u>kMaxRealPresets || rows>std::numeric_limits<std::uint16_t>::max() ||
       bags>std::numeric_limits<std::uint16_t>::max())return std::nullopt;
    // SoundFont phdr must END in one actual EOP record; it does not
    // represent playable preset 128 and is never enumerated.
    const auto end=phdr.at+(rows-1u)*38u;
    if(std::memcmp(riff.data()+end,"EOP",3)!=0 ||
       riff[end+3u]!='\0')return std::nullopt;
    Sf2PresetInventory out;
    out.reserve(rows-1u); // activation only; ZERO allocations in process()
    std::uint16_t previous=0u;
    for(std::size_t row=0;row<rows;++row){
        const auto at=phdr.at+row*38u;
        const auto next=w(riff,at+24u);
        if(next<previous || next>=bags ||
           (row>0u && next==previous && row<rows-1u))
            return std::nullopt;
        previous=next;
        if(row==rows-1u)break;
        const auto program=w(riff,at+20u);
        const auto bank=w(riff,at+22u);
        const auto following=w(riff,at+38u+24u);
        if(program>127u || bank>16383u ||
           next>=following || following>=bags ||
           hasExactSoundFontPreset(out,bank,static_cast<std::uint8_t>(program)))
            return std::nullopt;
        Sf2PresetIdentity entry{};
        std::memcpy(entry.name.data(),riff.data()+at,20u);
        entry.name[20]='\0';
        entry.original_phdr_row=static_cast<std::uint16_t>(row);
        entry.bank=bank;
        entry.program=static_cast<std::uint8_t>(program);
        entry.first_pbag=next;
        entry.last_pbag=following;
        out.push_back(entry);
    }
    if(out.size()!=rows-1u || out.empty())return std::nullopt;
    return out;
}
bool hasExactSoundFontPreset(
    const std::span<const Sf2PresetIdentity> entries,
    const std::uint16_t bank,const std::uint8_t program) noexcept {
    return std::any_of(entries.begin(),entries.end(),
        [&](const Sf2PresetIdentity& p){
            return p.bank==bank && p.program==program;
        });
}
} // namespace awe64::creative
