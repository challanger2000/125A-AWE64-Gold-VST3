// Test-only stream adapter. The source tables are private run-time inputs.
// Never link this executable into the plugin or publish its input file.
#include "OriginalCreativeVolumeController.hpp"
#include <array>
#include <fstream>
#include <iostream>
using namespace awe64::original_vxd;
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::array<std::uint8_t,384> tables{};
    std::ifstream file(argv[1],std::ios::binary);
    if(!file.read(reinterpret_cast<char*>(tables.data()),tables.size()) ||
       file.peek()!=std::char_traits<char>::eof())return 3;
    OriginalNoteVolumeResources source{};
    source.original_velocity_table_verified=true;
    source.velocity_table={tables.data(),128u};
    auto& cc=source.controllers;
    cc.verified=true;cc.device_additive_verified=true;cc.format1_verified=true;
    cc.cc11_table_128=tables.data()+128u;cc.cc7_table_128=tables.data()+256u;
    unsigned velocity,volume,expression,additive,format1,rom;
    int amplitude;
    while(std::cin>>velocity>>amplitude>>volume>>expression>>additive>>format1>>rom) {
        if(velocity>255u || amplitude<-32768 || amplitude>32767 ||
           volume>255u || expression>255u || additive>65535u ||
           format1>1u || rom>1u)return 4;
        cc.device_additive=static_cast<std::uint16_t>(additive);
        cc.format1=format1!=0u;
        const auto result=original_note_attenuation(source,
            static_cast<std::uint8_t>(velocity),static_cast<std::int16_t>(amplitude),
            static_cast<std::uint8_t>(volume),static_cast<std::uint8_t>(expression),rom!=0u);
        if(result)std::cout<<static_cast<unsigned>(*result)<<'\n';
        else std::cout<<"REJECT\n";
    }
    return std::cin.eof()?0:5;
}
