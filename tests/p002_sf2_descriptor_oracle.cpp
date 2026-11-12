// P002 private handoff differential adapter. No proprietary bytes embedded.
// Input: one row = 148-byte hex + bank/key/velocity/CC7/CC11/pan/FX state.
// Output: one encoded descriptor hex per row; optional --also-ip appends
// original DATA3/IP hex for the private handoff's zero channel-tune state.
// Failures are nonzero.
#include "CreativeSf2VoiceDescriptor.h"
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
using namespace awe64::creative;
int main(const int argc,char**argv) {
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
