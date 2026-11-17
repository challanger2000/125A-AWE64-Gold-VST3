// Private x86 differential adapter; original lookup bytes are runtime inputs.
#include "OriginalCreativeSf2TargetMath.hpp"
#include <array>
#include <fstream>
#include <iostream>
using namespace awe64::original_vxd::detail;
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::array<unsigned char,62> bytes{};
    std::ifstream file(argv[1],std::ios::binary);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()) ||
       file.peek()!=std::char_traits<char>::eof())return 3;
    std::array<std::uint16_t,31> table{};
    for(unsigned i=0;i<31u;++i)
        table[i]=static_cast<std::uint16_t>(static_cast<unsigned>(bytes[2u*i])|
                     (static_cast<unsigned>(bytes[2u*i+1u])<<8u));
    unsigned base;
    int tune,bend,generator7,attenuation;
    while(std::cin>>base>>tune>>bend>>generator7>>attenuation) {
        if(base>65535u || tune<-32768 || tune>32767 ||
           generator7<-32768 || generator7>32767 ||
           attenuation<-32768 || attenuation>32767)return 4;
        const auto pitch=note_pitch_targets(static_cast<std::uint16_t>(base),
            static_cast<std::int16_t>(tune),bend,static_cast<std::int16_t>(generator7));
        const auto volume=attack_volume_target(static_cast<std::int16_t>(attenuation),
            {table.data()+15u,16u},{table.data(),15u});
        if(!volume)return 5;
        std::cout<<pitch.initial_ip<<' '<<pitch.pitch_target<<' '<<*volume<<'\n';
    }
    return std::cin.eof()?0:6;
}
