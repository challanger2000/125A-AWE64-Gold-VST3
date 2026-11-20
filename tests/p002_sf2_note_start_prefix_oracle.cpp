// P002 private genuine-original SF2 NOTE START port oracle. No original
// Creative data or binary is embedded. Operates on source-verified 148-byte
// post-start records and separately observed physical voice IDs.
//
// stdin, one row: <296 hex digits> <0..127 key> <0..29 physical voice>
// stdout: one line of exactly 18 typed original OUTs encoded as
//         "port:width_bits:value" in hexadecimal, space-separated.
// This is a comparison adapter, NOT an original x86 driver emulator.
#include "CreativeSf2NoteStartPorts.h"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
using namespace awe64::creative;

namespace {
int nibble(const char c) noexcept {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
}
int main() {
    std::string line;
    std::size_t count=0;
    while(std::getline(std::cin,line)) {
        if(line.empty())continue;
        std::istringstream input(line);
        std::string hex,extra;
        unsigned key=0,voice=0;
        if(!(input>>hex>>key>>voice) || (input>>extra) ||
           hex.size()!=kCreativeSf2VoiceDescriptorBytes*2u ||
           key>127u || voice>=kOriginalCreativeMusicVoices) {
            std::cerr<<"INVALID_ORIGINAL_SF2_PREFIX_ROW="<<(count+1u)<<"\n";
            return 2;
        }
        CreativeSf2VoiceDescriptorBytes b{};
        for(std::size_t i=0;i<b.size();++i) {
            const int hi=nibble(hex[i*2u]);
            const int lo=nibble(hex[i*2u+1u]);
            if(hi<0 || lo<0)return 2;
            b[i]=static_cast<std::uint8_t>((hi<<4)|lo);
        }
        // Input data is exclusively source SHA-verified by the private
        // caller. Reject unknown SF2 descriptor states in native builder.
        const auto ports=candidateOriginalSf2NoteStartRegisterPrefix(
            b,static_cast<std::uint8_t>(key),
            static_cast<std::uint8_t>(voice),true,0,0);
        if(!ports){
            std::cerr<<"UNSUPPORTED_ORIGINAL_SF2_PREFIX_ROW="<<(count+1u)<<"\n";
            return 3;
        }
        for(std::size_t i=0;i<ports->size();++i){
            const auto& p=(*ports)[i];
            if(i!=0u)std::cout<<' ';
            std::cout<<std::hex<<std::setfill('0')
                     <<std::setw(4)<<p.port<<':'
                     <<std::setw(2)<<p.access_width_bits<<':'
                     <<std::setw(8)<<p.value;
        }
        std::cout<<std::dec<<'\n';
        ++count;
    }
    if(count==0){
        std::cerr<<"NO_PRIVATE_SF2_PREFIX_ROWS\n";
        return 4;
    }
    return 0;
}
