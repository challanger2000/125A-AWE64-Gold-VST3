#pragma once
// SYNTHETIC source/driver state only. No Creative file/table/sample bytes.
#include "OriginalCreativeSf2RuntimeBank.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace awe64::original_vxd::test {
using Bytes=std::vector<std::uint8_t>;
inline void u16(Bytes& b,std::uint32_t v) {b.push_back(static_cast<std::uint8_t>(v));b.push_back(static_cast<std::uint8_t>(v>>8u));}
inline void u32(Bytes& b,std::uint32_t v) {u16(b,v);u16(b,v>>16u);}
inline void chunk(Bytes& b,const char* id,Bytes const& payload) {
    for(unsigned i=0;i<4u;++i)b.push_back(static_cast<std::uint8_t>(id[i]));
    u32(b,static_cast<std::uint32_t>(payload.size()));b.insert(b.end(),payload.begin(),payload.end());
    if(payload.size()&1u)b.push_back(0u);
}
inline void list(Bytes& b,const char* id,Bytes const& payload) {
    Bytes data;for(unsigned i=0;i<4u;++i)data.push_back(static_cast<std::uint8_t>(id[i]));
    data.insert(data.end(),payload.begin(),payload.end());chunk(b,"LIST",data);
}
inline Bytes runtimeSf2(bool modulated=false) {
    Bytes info,version;u16(version,2u);u16(version,1u);chunk(info,"ifil",version);
    Bytes ph,pb,pg,in,ib,ig,im,sh,pcm,pd,sd;
    // Two distinct PHDR rows intentionally share bank/program. A resolver
    // that selects only by bytes would combine four layers incorrectly.
    for(unsigned i=0;i<3u;++i) {
        ph.insert(ph.end(),20u,0u);u16(ph,i==2u?0u:7u);u16(ph,0u);u16(ph,i);
        ph.insert(ph.end(),12u,0u);
        u16(pb,i);u16(pb,0u);
        in.insert(in.end(),20u,0u);u16(in,2u*i);
        if(i<2u){u16(pg,41u);u16(pg,i);}
    }
    for(unsigned i=0;i<5u;++i) {
        u16(ib,i);u16(ib,modulated && i!=0u?1u:0u);
        if(i<4u){u16(ig,53u);u16(ig,i);}
    }
    if(modulated) {u16(im,2u);u16(im,8u);u16(im,50u);u16(im,0u);u16(im,0u);}
    im.insert(im.end(),10u,0u);
    for(unsigned i=0;i<5u;++i) {
        const auto start=16u+256u*i;
        sh.insert(sh.end(),20u,0u);u32(sh,start);u32(sh,start+240u);
        u32(sh,start+16u);u32(sh,start+224u);u32(sh,44100u);
        sh.push_back(60u);sh.push_back(0u);
        u16(sh,i==2u?3u:i==3u?2u:0u);u16(sh,i==2u?4u:i==3u?2u:1u);
    }
    for(unsigned i=0;i<1536u;++i)
        u16(pcm,static_cast<std::uint16_t>(static_cast<std::int16_t>(
            (i%64u)<32u?12000:-12000)));
    chunk(pd,"phdr",ph);chunk(pd,"pbag",pb);chunk(pd,"pgen",pg);
    chunk(pd,"inst",in);chunk(pd,"ibag",ib);chunk(pd,"igen",ig);chunk(pd,"imod",im);
    chunk(pd,"shdr",sh);chunk(sd,"smpl",pcm);
    Bytes root{'s','f','b','k'};list(root,"INFO",info);list(root,"sdta",sd);list(root,"pdta",pd);
    Bytes bytes{'R','I','F','F'};u32(bytes,static_cast<std::uint32_t>(root.size()));
    bytes.insert(bytes.end(),root.begin(),root.end());return bytes;
}
inline OriginalSf2SourceTables runtimeTables(std::uint32_t base=0x20000eu) {
    OriginalSf2SourceTables t{};
    t.original_generator_templates_verified=true;t.original_pitch_tables_verified=true;
    t.original_sample_ram_placement_verified=true;t.ram_base_word=base;
    t.original_stereo_descriptor_conversion_verified=true;
    t.instrument_template[58]=-1;t.instrument_template[56]=100;
    t.instrument_template[54]=1;t.instrument_template[8]=180;
    for(auto op:{25u,26u,33u,34u})t.instrument_template[op]=-12000;
    t.instrument_template[29]=127;t.instrument_template[37]=127;
    return t;
}
inline OriginalSf2RuntimeResources runtimeResources() {
    OriginalSf2RuntimeResources r{};
    r.original_device_state_verified=true;r.original_volume_tables_verified=true;
    r.original_format1_verified=true;r.original_format1=true;
    r.volume_positive16.fill(0xffffu);r.volume_negative15.fill(0xffffu);
    for(unsigned i=0;i<128u;++i) {
        r.velocity[i]=static_cast<std::uint8_t>((127u-i)/2u);
        r.cc7[i]=static_cast<std::uint8_t>((127u-i)/2u);
        r.cc11[i]=static_cast<std::uint8_t>((127u-i)/3u);
    }
    return r;
}
inline MidiEngine runtimeState() {
    MidiEngine e{};e.device.original_pan_device_mode_verified=true;
    e.original_reset_callback_absent_verified=true;
    for(auto& c:e.channel) {
        c.original_fine_tune_verified=true;c.original_coarse_tune_verified=true;
        c.original_modulation_byte_44d_verified=true;
    }
    for(auto& v:e.device.voices) {
        v.blocked=true;v.original_pitch_base_verified=true;v.pan_state_verified=true;
        v.volume_terms_verified=true;v.original_modulation_word_verified=true;
    }
    return e;
}
struct RuntimeFixture {
    Bytes bytes=runtimeSf2();
    std::array<OriginalSf2RuntimeFile,2> files{{{bytes,runtimeTables()},
        {bytes,runtimeTables(0x24000eu)}}};
    std::array<OriginalPresetTableRow,3> row5{{{1u,7u,1u,17u},{1u,0u,1u,19u},{0xffffu,0u,0u,0u}}};
    std::array<OriginalPresetTableRow,3> row6{{{1u,0u,2u,21u},{1u,7u,2u,23u},{0xffffu,0u,0u,0u}}};
    std::array<OriginalPresetTableRow,2> drum{{{1u,128u,128u,25u},{0xffffu,0u,0u,0u}}};
    std::array<OriginalProgramBankSlot,original_program_bank_slots> slots{};
    std::array<OriginalSf2RuntimePresetBinding,5> bindings{{
        {true,5u,0u,0u,1u,0u,7u,0x12345678u},
        {true,5u,1u,0u,0u,0u,7u,0x87654321u},
        {true,6u,0u,1u,0u,0u,7u,0x23456789u},
        {true,6u,1u,1u,1u,0u,7u,0x3456789au},
        {true,129u,0u,0u,1u,0u,7u,0x456789abu}}};
    OriginalSf2RuntimeResources resources=runtimeResources();
    RuntimeFixture() {slots[5].rows=row5;slots[6].rows=row6;slots[129].rows=drum;}
    RuntimeFixture(RuntimeFixture const&)=delete;
    RuntimeFixture& operator=(RuntimeFixture const&)=delete;
};
// Word API test double only. No synthesis: VTFT is deliberately idle.
struct RuntimeChip {
    std::array<std::array<std::array<std::uint32_t,32>,8>,3> registers{};
    std::uint8_t selector=0;
    std::uint64_t writes=0,reads=0;
    bool good=true,fail_write=false;
    bool ready() const noexcept {return good;}
    void writePort(std::uint16_t port,std::uint16_t value) noexcept {
        ++writes;if(fail_write){good=false;return;}
        if(port==0xe22u){selector=static_cast<std::uint8_t>(value);return;}
        unsigned bank=0,shift=0;
        if(port==0x620u || port==0x622u){bank=0u;shift=port==0x622u?16u:0u;}
        else if(port==0xa20u || port==0xa22u){bank=1u;shift=port==0xa22u?16u:0u;}
        else if(port==0xe20u)bank=2u;
        else {good=false;return;}
        auto& word=registers[bank][(selector>>5u)&7u][selector&31u];
        word=(word&~(0xffffu<<shift))|(static_cast<std::uint32_t>(value)<<shift);
    }
    std::uint16_t readPort(std::uint16_t port) noexcept {
        ++reads;
        if((port==0x620u || port==0x622u) && (selector>>5u)==3u)return 0u;
        unsigned bank=0,shift=0;
        if(port==0x620u || port==0x622u){bank=0u;shift=port==0x622u?16u:0u;}
        else if(port==0xa20u || port==0xa22u){bank=1u;shift=port==0xa22u?16u:0u;}
        else if(port==0xe20u)bank=2u;
        else {good=false;return 0u;}
        return static_cast<std::uint16_t>(registers[bank][(selector>>5u)&7u][selector&31u]>>shift);
    }
};
} // namespace awe64::original_vxd::test
