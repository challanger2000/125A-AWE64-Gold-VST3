// P002 private handoff differential adapter. No proprietary bytes embedded.
// Input: one row = 148-byte hex + bank/key/velocity/CC7/CC11/pan/FX state.
// Output: one encoded descriptor hex per row; optional --also-ip appends
// original DATA3/IP hex for the private handoff's zero channel-tune state.
// Failures are nonzero.
#include "CreativeSf2VoiceDescriptor.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
using namespace awe64::creative;
namespace {
// Verify the complete original private 192,000-rate pitch transfer using
// the CURRENT compiled CreativeSf2VoiceDescriptor.cpp, not a Python port.
// Input: exactly 128 original DWORD bases and 128 original signed byte slopes.
// Output: 192000 lines, each exactly eight hex digits (unsigned 32-bit).
// The original source VXD is SHA-locked and parsed ONLY by the caller; no
// proprietary tables or pitch samples are included in the public binary.
int runPrivateSf2PitchMatrix() {
    OriginalCreativeSf2PitchTables tables{};
    for(auto& base:tables.base) {
        std::uint64_t value=0;
        if(!(std::cin>>value) || value>0xffffffffull)return 2;
        base=static_cast<std::uint32_t>(value);
    }
    for(auto& slope:tables.slope) {
        int value=0;
        if(!(std::cin>>value) || value<-128 || value>127)return 2;
        slope=static_cast<std::int8_t>(value);
    }
    std::string extra;
    if(std::cin>>extra)return 2;
    for(std::uint32_t rate=1;rate<=192000u;++rate) {
        const auto pitch=originalCreativeSf2SampleRateToDwPitch(rate,tables);
        if(!pitch)return 3;
        std::cout<<std::hex<<std::setw(8)<<std::setfill('0')
                 <<*pitch<<'\n';
    }
    return 0;
}
// PRIVATE original builder differential, not a synthetic SoundFont default:
// token count, then per voice: bank program key velocity sampleId,
// two 59 signed template WORD arrays (preset then instrument), then
// four 64 raw-generator lists in scope order preset global/local,
// instrument global/local. -1 means genuinely absent; raw65535 is valid.
// Prints only the freshly built first 118 descriptor bytes, which the
// private Python runner compares with independently executed x86 data.
int runPrivateSf2Builder() {
    std::size_t records=0;
    if(!(std::cin>>records) || records==0u || records>100000u)return 2;
    constexpr char digits[]="0123456789abcdef";
    for(std::size_t row=0;row<records;++row) {
        unsigned bank=0,program=0,key=0,velocity=0,sample=0;
        if(!(std::cin>>bank>>program>>key>>velocity>>sample) ||
           bank>65535u || program>127u || key>127u ||
           velocity==0u || velocity>127u || sample>65535u)return 2;
        SbkLayer layer{};
        layer.source_format=OriginalCreativeBankFormat::Format2Sf2;
        layer.bank=static_cast<std::uint16_t>(bank);
        layer.program=static_cast<std::uint8_t>(program);
        layer.key=static_cast<std::uint8_t>(key);
        layer.velocity=static_cast<std::uint8_t>(velocity);
        layer.rom_sample_selector=static_cast<std::uint16_t>(sample);
        std::array<std::int16_t,59> presetTemplate{},instrumentTemplate{};
        for(auto* selected:{&presetTemplate,&instrumentTemplate})
            for(auto& v:*selected) {
                std::int32_t word=0;
                if(!(std::cin>>word) || word<-32768 || word>32767)
                    return 2;
                v=static_cast<std::int16_t>(word);
            }
        for(auto* scope:{&layer.preset_global,&layer.preset_local,
                          &layer.instrument_global,&layer.instrument_local})
            for(auto& v:scope->raw) {
                std::int32_t raw=-1;
                if(!(std::cin>>raw) || raw<-1 || raw>65535)return 2;
                if(raw>=0)v=static_cast<std::uint16_t>(raw);
            }
        const auto built=candidateOriginalSf2GeneratorsWithVerifiedTemplates(
            layer,presetTemplate,instrumentTemplate);
        if(!built) {
            std::cerr<<"ORIGINAL_SF2_BUILDER_REJECTED_ROW="<<(row+1u)<<"\n";
            return 3;
        }
        for(std::size_t i=0;i<118u;++i){
            const auto b=(*built)[i];
            std::cout<<digits[b>>4]<<digits[b&15u];
        }
        std::cout<<'\n';
    }
    std::string extra;
    if(std::cin>>extra)return 2;
    return 0;
}
}
int main(const int argc,char**argv) {
    if(argc==2 && std::string_view(argv[1])=="--pitch-matrix")
        return runPrivateSf2PitchMatrix();
    if(argc==2 && std::string_view(argv[1])=="--build")
        return runPrivateSf2Builder();
    const bool withIp=argc==2 && std::string_view(argv[1])=="--also-ip";
    if(argc!=1 && !withIp)return 2;
    std::string line;
    std::size_t rows=0;
    while(std::getline(std::cin,line)) {
        if(line.empty())continue;
        std::istringstream input(line);
        std::string hex,extra;
        unsigned bank,key,vel,cc7,cc11,pan,chorus,reverb,device,flag;
        if(!(input>>hex>>bank>>key>>vel>>cc7>>cc11>>pan>>chorus>>reverb>>device>>flag) ||
           (input>>extra) || hex.size()!=kCreativeSf2VoiceDescriptorBytes*2u ||
           bank>65535u || key>255u || vel>255u || cc7>255u ||
           cc11>255u || pan>255u || chorus>255u || reverb>255u ||
           device>65535u || flag>255u) {
            std::cerr<<"INVALID_INPUT_ROW="<<(rows+1u)<<"\n";
            return 2;
        }
        CreativeSf2VoiceDescriptorBytes raw{};
        const auto nybble=[](const char c)->int {
            if(c>='0' && c<='9')return c-'0';
            if(c>='a' && c<='f')return c-'a'+10;
            if(c>='A' && c<='F')return c-'A'+10;
            return -1;
        };
        for(std::size_t i=0;i<raw.size();++i) {
            const int hi=nybble(hex[i*2u]),lo=nybble(hex[i*2u+1u]);
            if(hi<0 || lo<0)return 2;
            raw[i]=static_cast<std::uint8_t>((hi<<4)|lo);
        }
        OriginalCreativeVolumeTerms volume{};
        volume.cc7=static_cast<std::uint8_t>(cc7);
        volume.cc11=static_cast<std::uint8_t>(cc11);
        volume.device_state_additive_attenuation=static_cast<std::uint16_t>(device);
        const OriginalSf2VoiceStartConditions context{
            static_cast<std::uint16_t>(bank),
            static_cast<std::uint8_t>(key),static_cast<std::uint8_t>(vel),
            static_cast<std::uint8_t>(pan),static_cast<std::uint8_t>(chorus),
            static_cast<std::uint8_t>(reverb),static_cast<std::uint8_t>(flag),volume};
        const auto encoded=candidateOriginalSf2EncodedVoiceDescriptor(raw,context);
        if(!encoded){std::cerr<<"UNSUPPORTED_OR_INVALID_ROW="<<(rows+1u)<<"\n";return 3;}
        std::optional<std::uint16_t> ip{};
        if(withIp) {
            // The source-locked original bank handoff drives MIDI channel
            // tuning at zero for all four currently captured voices.
            // Do not use this mode for arbitrary tuned channel contexts.
            ip=candidateOriginalSf2InitialIpFromDescriptor(
                *encoded,static_cast<std::uint8_t>(key),0,0);
            if(!ip){
                std::cerr<<"UNSUPPORTED_IP_ROW="<<(rows+1u)<<"\n";
                return 3;
            }
        }
        constexpr char digits[]="0123456789abcdef";
        for(const auto b:*encoded)
            std::cout<<digits[b>>4]<<digits[b&15];
        if(withIp) {
            std::cout<<' ';
            for(unsigned n=4u;n>0u;--n)
                std::cout<<digits[(*ip>>((n-1u)*4u))&15u];
        }
        std::cout<<'\n';
        ++rows;
    }
    if(rows==0){std::cerr<<"NO_PRIVATE_TEST_VECTORS\n";return 4;}
    return 0;
}
