// Private-oracle adapter. Inputs are supplied at run time; no driver, ROM,
// sample data or captured register values are compiled into this executable.
#include "OriginalCreativeSf2MainCompiler.hpp"
#include <cstdint>
#include <iostream>
using namespace awe64::original_vxd;
namespace {
void word(Descriptor& d,unsigned offset,std::uint32_t value) {
    d[offset]=static_cast<std::uint8_t>(value);
    d[offset+1u]=static_cast<std::uint8_t>(value>>8u);
}
void dword(Descriptor& d,unsigned offset,std::uint32_t value) {
    word(d,offset,value);word(d,offset+2u,value>>16u);
}
}
int main() {
    std::uint32_t mode,start,end,loopStart,loopEnd,key,root,pitch;
    std::int32_t correction,fine,coarse,scale,tune1,tune2;
    while(std::cin>>mode>>start>>end>>loopStart>>loopEnd>>key>>root>>pitch
                  >>correction>>fine>>coarse>>scale>>tune1>>tune2) {
        SourceVoice voice{};voice.source_verified=true;
        auto& d=voice.descriptor;
        word(d,0x8cu,1u);word(d,0x6cu,mode);
        dword(d,0x76u,start);dword(d,0x7au,end);
        dword(d,0x7eu,loopStart);dword(d,0x82u,loopEnd);
        dword(d,0x86u,pitch);d[0x8au]=60u;
        d[0x8bu]=static_cast<std::uint8_t>(correction);
        word(d,0x74u,root);word(d,0x68u,static_cast<std::uint32_t>(fine));
        word(d,0x66u,static_cast<std::uint32_t>(coarse));
        word(d,0x70u,static_cast<std::uint32_t>(scale));
        OriginalVoiceStartSources state{};
        state.original_descriptor_verified=true;
        state.original_device_state_verified=true;
        state.original_sample_placement_verified=true;
        state.original_ram_sample=start>=0x200000u;
        state.original_channel_tune_1_cents=static_cast<std::int16_t>(tune1);
        state.original_channel_tune_2_cents=static_cast<std::int16_t>(tune2);
        OriginalVoiceProgram out{};
        if(!compile_original_sf2_main(voice,0u,static_cast<std::uint8_t>(key),state,out))
            std::cout<<"REJECT\n";
        else
            std::cout<<out.data[7]<<' '<<(out.data[17]&0xffffffu)<<' '
                     <<(out.data[18]&0xffffffu)<<' '<<out.data[19]<<'\n';
    }
    return std::cin.eof()?0:2;
}
