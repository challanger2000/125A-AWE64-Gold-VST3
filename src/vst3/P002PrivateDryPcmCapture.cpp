#include "P002PrivateDryPcmCapture.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <array>
#include <cstdint>
namespace awe64::vst3 {
// A separate Win32 encoder is independently tested with synthetic PCM.
// No ROM, Creative tables, audio source, VST3 SDK or DSP in this unit.
bool savePrivateNativeDryWav(
    const std::filesystem::path& destination,
    const std::span<const std::int16_t> samples,
    const bool overflow) noexcept {
    if(destination.empty() || !destination.is_absolute() ||
       overflow || samples.size()<512u || (samples.size()%2u)!=0u ||
       samples.size()>44100u*2u*30u)
        return false;
    const auto bytes=static_cast<std::uint32_t>(
        samples.size()*sizeof(std::int16_t));
    // Standard 44-byte RIFF/WAVE PCM16 stereo 44100 Hz. Windows
    // x64 is little-endian; write explicit byte order for the header.
    std::array<std::uint8_t,44> header{};
    const auto bytes4=[&](const unsigned at,const std::uint32_t v) noexcept {
        for(unsigned n=0;n<4;++n)
            header[at+n]=static_cast<std::uint8_t>(v>>(8u*n));
    };
    const auto bytes2=[&](const unsigned at,const std::uint16_t v) noexcept {
        header[at]=static_cast<std::uint8_t>(v);
        header[at+1u]=static_cast<std::uint8_t>(v>>8u);
    };
    header[0]='R';header[1]='I';header[2]='F';header[3]='F';
    bytes4(4u,36u+bytes);
    header[8]='W';header[9]='A';header[10]='V';header[11]='E';
    header[12]='f';header[13]='m';header[14]='t';header[15]=' ';
    bytes4(16u,16u);bytes2(20u,1u);bytes2(22u,2u);
    bytes4(24u,44100u);bytes4(28u,44100u*4u);
    bytes2(32u,4u);bytes2(34u,16u);
    header[36]='d';header[37]='a';header[38]='t';header[39]='a';
    bytes4(40u,bytes);
    HANDLE h=CreateFileW(destination.c_str(),GENERIC_WRITE,0u,nullptr,
                         CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return false;
    DWORD written=0u;
    bool ok=WriteFile(h,header.data(),static_cast<DWORD>(header.size()),
                      &written,nullptr)!=0 &&
            written==header.size();
    if(ok) {
        written=0u;
        ok=WriteFile(h,samples.data(),bytes,&written,nullptr)!=0 &&
           written==bytes;
    }
    if(!CloseHandle(h))ok=false;
    if(!ok)DeleteFileW(destination.c_str());
    return ok;
}

} // namespace awe64::vst3
