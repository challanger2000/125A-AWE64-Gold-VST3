// P006 temporary AUDIO SOURCE for UNPATCHED existing 86Box fork.
// Reads the fork's AWE32_WAV 16-bit/44100/stereo capture while running.
// Audio is ORIGINAL pre-card-mixer EMU8000 PCM from snd_emu8k_trace.c.
// High latency: original 86Box source fflush only ~once/second.
// No driver port, no substitute synth, no private assets.
#include "P006Transport.hpp"
#ifndef _WIN32
#error P006WaveTail is a Windows-x64 source adapter
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <windows.h>

namespace {
constexpr char kMapName[]="Local\\125A_P006_AUDIO_DEMO";
constexpr std::uint64_t kDataOffset=44u;
constexpr DWORD kBlockBytes=4096u;
std::uint16_t u16(std::uint8_t const* b) noexcept {
    return static_cast<std::uint16_t>(static_cast<unsigned>(b[0]) |
         (static_cast<unsigned>(b[1])<<8u));
}
std::uint32_t u32(std::uint8_t const* b) noexcept {
    return static_cast<std::uint32_t>(b[0])|
        static_cast<std::uint32_t>(b[1])<<8u|
        static_cast<std::uint32_t>(b[2])<<16u|
        static_cast<std::uint32_t>(b[3])<<24u;
}
bool isForkWave(std::array<std::uint8_t,44u> const& b) noexcept {
    return b[0]=='R'&&b[1]=='I'&&b[2]=='F'&&b[3]=='F'&&
        b[8]=='W'&&b[9]=='A'&&b[10]=='V'&&b[11]=='E'&&
        b[12]=='f'&&b[13]=='m'&&b[14]=='t'&&b[15]==' '&&
        u32(&b[16])==16u && u16(&b[20])==1u && u16(&b[22])==2u &&
        u32(&b[24])==P006_RATE && u32(&b[28])==P006_RATE*4u &&
        u16(&b[32])==4u && u16(&b[34])==16u &&
        b[36]=='d'&&b[37]=='a'&&b[38]=='t'&&b[39]=='a';
}
bool position(HANDLE file,std::uint64_t offset) noexcept {
    LARGE_INTEGER off{},now{};
    off.QuadPart=static_cast<LONGLONG>(offset);
    return SetFilePointerEx(file,off,&now,FILE_BEGIN)!=FALSE &&
        now.QuadPart==off.QuadPart;
}
std::uint64_t diskLength(HANDLE file) noexcept {
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(file,&size)||size.QuadPart<0)return 0u;
    return static_cast<std::uint64_t>(size.QuadPart);
}
struct Mapping {
    HANDLE handle=INVALID_HANDLE_VALUE;
    P006AudioShared* ring=nullptr;
    void disconnect() noexcept {
        if(ring)UnmapViewOfFile(ring);
        ring=nullptr;
        if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);
        handle=INVALID_HANDLE_VALUE;
    }
    bool connect() noexcept {
        if(ring)return p006::valid(*ring);
        handle=OpenFileMappingA(FILE_MAP_WRITE|FILE_MAP_READ,FALSE,kMapName);
        if(!handle){handle=INVALID_HANDLE_VALUE;return false;}
        ring=static_cast<P006AudioShared*>(MapViewOfFile(handle,
            FILE_MAP_WRITE|FILE_MAP_READ,0u,0u,sizeof(P006AudioShared)));
        if(!ring||!p006::valid(*ring)){
            disconnect();return false;
        }
        return true;
    }
    ~Mapping(){disconnect();}
};
}
int main(int argc,char** argv) {
    if(argc!=2||!argv[1]||!*argv[1]) {
        std::puts("Usage: P006_WAV_Tail.exe C:\\path\\existing_AWE32_WAV_capture.wav");
        std::puts("Start VST3 first, then 86Box with AWE32_WAV set.");
        return 2;
    }
    std::puts("P006 ORIGINAL 86Box AWE32_WAV -> native VST3");
    std::puts("WARNING: 86Box wav capture flushes only about once per second.");
    std::puts("Use the direct fork hook for future low-latency audio.");
    HANDLE file=INVALID_HANDLE_VALUE;
    std::uint64_t cursor=kDataOffset;
    std::array<std::uint8_t,kBlockBytes> bytes{};
    std::array<float,kBlockBytes/2u> floats{};
    Mapping map{};
    std::uint64_t framesSent=0u;
    for(;;) {
        if(!map.connect()){Sleep(20u);continue;}
        if(file==INVALID_HANDLE_VALUE) {
            file=CreateFileA(argv[1],GENERIC_READ,
                FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file==INVALID_HANDLE_VALUE){Sleep(50u);continue;}
            std::array<std::uint8_t,44u> header{};
            DWORD n=0;
            if(!ReadFile(file,header.data(),static_cast<DWORD>(header.size()),
                        &n,nullptr)||n!=header.size()||!isForkWave(header)){
                std::puts("P006 BAD/WIP 86Box AWE32_WAV header, retrying");
                CloseHandle(file);file=INVALID_HANDLE_VALUE;
                Sleep(50u);continue;
            }
            cursor=kDataOffset;
            std::puts("P006 original fork WAV recognized (44.1kHz, PCM16 stereo).");
        }
        const auto sz=diskLength(file);
        if(sz<kDataOffset||sz<cursor) {
            // Original 86Box restarted and its trace was truncated.
            CloseHandle(file);file=INVALID_HANDLE_VALUE;
            cursor=kDataOffset;Sleep(50u);continue;
        }
        const auto unread=sz-cursor;
        const DWORD nbytes=static_cast<DWORD>(
            std::min<std::uint64_t>(unread,kBlockBytes)&~3ull);
        if(nbytes==0u){Sleep(5u);continue;}
        const auto occupancy=static_cast<std::uint32_t>(
            p006::load(map.ring->write_frames)) -
            static_cast<std::uint32_t>(p006::load(map.ring->read_frames));
        if(occupancy>=P006_AUDIO_CAPACITY){Sleep(3u);continue;}
        if(!position(file,cursor)){
            CloseHandle(file);file=INVALID_HANDLE_VALUE;continue;
        }
        DWORD got=0u;
        if(!ReadFile(file,bytes.data(),nbytes,&got,nullptr)){
            CloseHandle(file);file=INVALID_HANDLE_VALUE;continue;
        }
        const auto frames=static_cast<std::uint32_t>(got/4u);
        if(frames==0u){Sleep(5u);continue;}
        for(std::uint32_t i=0u;i<frames*2u;++i) {
            const auto word=u16(bytes.data()+i*2u);
            const auto signedValue=std::bit_cast<std::int16_t>(word);
            floats[i]=static_cast<float>(signedValue)*(1.f/32768.f);
        }
        const auto accepted=p006::write(*map.ring,floats.data(),frames);
        cursor+=static_cast<std::uint64_t>(accepted)*4u;
        framesSent+=accepted;
        if(accepted<frames)Sleep(3u);
        if(framesSent>=P006_RATE) {
            std::printf("P006 original WAV audio received: %llu frames\n",
                static_cast<unsigned long long>(framesSent));
            framesSent=0u;
        }
    }
}
