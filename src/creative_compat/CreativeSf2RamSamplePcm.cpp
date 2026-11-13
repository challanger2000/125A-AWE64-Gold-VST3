#include "CreativeSf2RamSamplePcm.h"
#include "CreativeSbkLayers.h"
#include <array>
#include <limits>
#include <string_view>

namespace awe64::creative {
namespace {
using Bytes=std::span<const std::uint8_t>;
struct Chunk {std::size_t start=0u,size=0u;bool present=false;};
bool textAt(Bytes b,std::size_t offset,std::string_view s) noexcept {
    if(offset>b.size() || s.size()>b.size()-offset)return false;
    for(std::size_t i=0;i<s.size();++i)
        if(b[offset+i]!=static_cast<std::uint8_t>(s[i]))return false;
    return true;
}
std::uint16_t le16(Bytes b,std::size_t i) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(b[i])|
        (static_cast<unsigned>(b[i+1u])<<8u));
}
std::uint32_t le32(Bytes b,std::size_t i) noexcept {
    return static_cast<std::uint32_t>(le16(b,i))|
        (static_cast<std::uint32_t>(le16(b,i+2u))<<16u);
}
template<class F>
bool visit(Bytes b,std::size_t first,std::size_t end,F fn) noexcept {
    if(first>end || end>b.size())return false;
    for(std::size_t pos=first;pos<end;) {
        if(end-pos<8u)return false;
        const auto len=static_cast<std::size_t>(le32(b,pos+4u));
        const auto begin=pos+8u;
        if(len>end-begin)return false;
        if(!fn(pos,Chunk{begin,len,true}))return false;
        pos=begin+len;
        if((len&1u)!=0u) {
            if(pos==end)return false;
            ++pos;
        }
    }
    return true;
}
}
std::optional<OriginalSf2RamSamplePcmView>
candidateOriginalSf2ReadRamMonoSamplePcm(
    const Bytes sf2,
    const std::uint16_t sample_id,
    const std::size_t ram_words) noexcept {
    // A missing/invalid independent Creative RAM allocation capacity
    // must not accidentally turn an SF2 sample into a ROM address.
    if(sf2.size()<12u || !textAt(sf2,0u,"RIFF") ||
       !textAt(sf2,8u,"sfbk") ||
       le32(sf2,4u)!=sf2.size()-8u ||
       ram_words==0u || ram_words>0xE00000u)
        return std::nullopt;
    Chunk info{},sdta{},pdta{};
    const bool validTop=visit(sf2,12u,sf2.size(),
        [&](std::size_t pos,Chunk data) noexcept {
            if(!textAt(sf2,pos,"LIST"))return true;
            if(data.size<4u)return false;
            if(textAt(sf2,data.start,"INFO")) {
                if(info.present)return false;
                info=data;
            }else if(textAt(sf2,data.start,"sdta")) {
                if(sdta.present)return false;
                sdta=data;
            }else if(textAt(sf2,data.start,"pdta")) {
                if(pdta.present)return false;
                pdta=data;
            }
            return true;
        });
    if(!validTop || !info.present || !sdta.present || !pdta.present)
        return std::nullopt;
    Chunk ifil{},smpl{},sm24{},shdr{};
    if(!visit(sf2,info.start+4u,info.start+info.size,
        [&](std::size_t pos,Chunk data) noexcept {
            if(!textAt(sf2,pos,"ifil"))return true;
            if(ifil.present)return false;
            ifil=data;return true;
        }) ||
       !ifil.present || ifil.size!=4u ||
       le16(sf2,ifil.start)!=2u)
        return std::nullopt;
    if(!visit(sf2,sdta.start+4u,sdta.start+sdta.size,
        [&](std::size_t pos,Chunk data) noexcept {
            if(textAt(sf2,pos,"smpl")) {
                if(smpl.present)return false;
                smpl=data;
            }else if(textAt(sf2,pos,"sm24")) {
                if(sm24.present)return false;
                sm24=data;
            }
            return true;
        }) ||
       !smpl.present || sm24.present || smpl.size==0u ||
       (smpl.size&1u)!=0u ||
       smpl.size/2u>std::numeric_limits<std::uint32_t>::max())
        return std::nullopt;
    if(!visit(sf2,pdta.start+4u,pdta.start+pdta.size,
        [&](std::size_t pos,Chunk data) noexcept {
            if(!textAt(sf2,pos,"shdr"))return true;
            if(shdr.present)return false;
            shdr=data;
            return true;
        }) ||
       !shdr.present || shdr.size%46u!=0u ||
       shdr.size<2u*46u)
        return std::nullopt;
    const auto count=shdr.size/46u;
    // The final standard SoundFont 46-byte sample header is EOS.
    // A caller may NEVER play the terminal record as audio.
    if(sample_id>=count-1u)return std::nullopt;
    const auto base=shdr.start+static_cast<std::size_t>(sample_id)*46u;
    const auto first=le32(sf2,base+20u);
    const auto last=le32(sf2,base+24u);
    const auto loopStart=le32(sf2,base+28u);
    const auto loopEnd=le32(sf2,base+32u);
    const auto hz=le32(sf2,base+36u);
    const auto root=sf2[base+40u];
    const auto correction=static_cast<std::int8_t>(sf2[base+41u]);
    const auto linked=le16(sf2,base+42u);
    const auto sampleType=le16(sf2,base+44u);
    const auto poolWords=smpl.size/2u;
    if(sampleType!=1u || linked!=0u || root>127u ||
       hz==0u || first>=last || last>poolWords ||
       loopStart<first || loopStart>=loopEnd || loopEnd>last ||
       last-first>ram_words)
        return std::nullopt;
    const auto begin=smpl.start+static_cast<std::size_t>(first)*2u;
    const auto countBytes=static_cast<std::size_t>(last-first)*2u;
    // Bounds already established by last<=poolWords; explicit
    // double-check to keep an unchanged/no-op-on-invalid policy.
    if(begin>sf2.size() || countBytes>sf2.size()-begin)
        return std::nullopt;
    return OriginalSf2RamSamplePcmView{
        sample_id,first,last,loopStart,loopEnd,hz,root,correction,
        static_cast<std::uint32_t>(poolWords),
        sf2.subspan(begin,countBytes)
    };
}
std::optional<OriginalSf2RamNotePcmPlan>
candidateOriginalSf2RamMonoNotePcmPlan(
    const std::span<const std::uint8_t> bank_bytes,
    const std::uint16_t bank,
    const std::uint8_t program,
    const std::uint8_t key,
    const std::uint8_t velocity,
    const std::size_t ram_words) noexcept {
    // This reuses the separately verified Creative SF2 preset/zone
    // decoder and its source six-descriptor limit; no new or guessed
    // per-key sample selection rule is introduced.
    const auto matched=candidateStandardSf2RamLayers(
        bank_bytes,bank,program,key,velocity);
    if(!matched || ram_words==0u)return std::nullopt;
    OriginalSf2RamNotePcmPlan result{};
    result.bank=bank;result.program=program;
    result.key=key;result.velocity=velocity;
    const std::size_t n=matched->count<kOriginalCreativeRamNoteMaxLayers
        ? matched->count:kOriginalCreativeRamNoteMaxLayers;
    for(std::size_t i=0;i<n;++i) {
        const auto& layer=matched->layers[i];
        if(layer.source_format!=OriginalCreativeBankFormat::Format2Sf2)
            return std::nullopt;
        const auto pcm=candidateOriginalSf2ReadRamMonoSamplePcm(
            bank_bytes,layer.rom_sample_selector,ram_words);
        // Reject the ENTIRE MIDI NoteOn plan if any authorized
        // instrument layer lacks a real 16-bit RAM sample. Do not
        // partially map the other layers to arbitrary DRAM locations.
        if(!pcm)return std::nullopt;
        result.layers[i]=*pcm;
    }
    result.count=static_cast<std::uint8_t>(n);
    return result;
}
} // namespace awe64::creative
