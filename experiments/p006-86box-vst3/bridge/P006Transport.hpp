#pragma once
#include "P006Wire.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
namespace p006 {
inline std::int32_t load(volatile std::int32_t& value) noexcept {
#ifdef _WIN32
    static_assert(sizeof(LONG)==sizeof(std::int32_t));
    return static_cast<std::int32_t>(InterlockedCompareExchange(
        reinterpret_cast<volatile LONG*>(&value),0,0));
#else
    return std::atomic_ref<std::int32_t>(
        const_cast<std::int32_t&>(value)).load(std::memory_order_acquire);
#endif
}
inline void store(volatile std::int32_t& value,std::int32_t next) noexcept {
#ifdef _WIN32
    InterlockedExchange(reinterpret_cast<volatile LONG*>(&value),
        static_cast<LONG>(next));
#else
    std::atomic_ref<std::int32_t>(const_cast<std::int32_t&>(value))
        .store(next,std::memory_order_release);
#endif
}
inline void increment(volatile std::int32_t& value,std::int32_t amount) noexcept {
#ifdef _WIN32
    InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(&value),
        static_cast<LONG>(amount));
#else
    std::atomic_ref<std::int32_t>(const_cast<std::int32_t&>(value))
        .fetch_add(amount,std::memory_order_relaxed);
#endif
}
inline void init(P006AudioShared& r) noexcept {
    r.magic=P006_ABI_MAGIC;r.version=P006_ABI_VERSION;
    r.rate=P006_RATE;r.channels=P006_CHANNELS;
    r.capacity=P006_AUDIO_CAPACITY;r.format=1u;
    store(r.write_frames,0);store(r.read_frames,0);
    store(r.overrun_frames,0);store(r.underrun_frames,0);
}
inline bool valid(P006AudioShared const& r) noexcept {
    return r.magic==P006_ABI_MAGIC && r.version==P006_ABI_VERSION &&
        r.rate==P006_RATE && r.channels==P006_CHANNELS &&
        r.capacity==P006_AUDIO_CAPACITY && r.format==1u;
}
// 86Box one-producer; nonblocking and no overwrite of unread data.
inline std::uint32_t write(P006AudioShared& r,const float* interleaved,
                           std::uint32_t frames) noexcept {
    if(!valid(r) || (frames!=0 && interleaved==nullptr))return 0;
    const auto wr=static_cast<std::uint32_t>(load(r.write_frames));
    const auto rd=static_cast<std::uint32_t>(load(r.read_frames));
    const auto occupancy=wr-rd;
    if(occupancy>P006_AUDIO_CAPACITY)return 0;
    const auto accepted=std::min(frames,P006_AUDIO_CAPACITY-occupancy);
    for(std::uint32_t f=0;f<accepted;++f){
        const auto i=((wr+f)&(P006_AUDIO_CAPACITY-1u))*2u;
        r.samples[i]=interleaved[2u*f];r.samples[i+1u]=interleaved[2u*f+1u];
    }
    if(accepted!=0u)store(r.write_frames,static_cast<std::int32_t>(wr+accepted));
    if(accepted<frames)increment(r.overrun_frames,
        static_cast<std::int32_t>(frames-accepted));
    return accepted;
}
// DAW one-consumer; silence for missing frames, no IO/wait/heap in process.
inline std::uint32_t read(P006AudioShared& r,float* left,float* right,
                          std::uint32_t frames) noexcept {
    if(left==nullptr||right==nullptr)return 0;
    if(!valid(r)){
        for(std::uint32_t i=0;i<frames;++i){left[i]=0.f;right[i]=0.f;}
        return 0;
    }
    const auto wr=static_cast<std::uint32_t>(load(r.write_frames));
    const auto rd=static_cast<std::uint32_t>(load(r.read_frames));
    const auto occupancy=wr-rd;
    const auto available=occupancy<=P006_AUDIO_CAPACITY?
        std::min(frames,occupancy):0u;
    for(std::uint32_t f=0;f<available;++f){
        const auto i=((rd+f)&(P006_AUDIO_CAPACITY-1u))*2u;
        left[f]=r.samples[i];right[f]=r.samples[i+1u];
    }
    for(std::uint32_t f=available;f<frames;++f){left[f]=0.f;right[f]=0.f;}
    if(available!=0u)store(r.read_frames,static_cast<std::int32_t>(rd+available));
    if(available<frames)increment(r.underrun_frames,
        static_cast<std::int32_t>(frames-available));
    return available;
}
inline bool pack(P006MidiPacket& p,std::uint8_t seq,std::uint8_t status,
                 std::uint8_t d1,std::uint8_t d2) noexcept {
    if(status<0x80u||status>=0xf0u||d1>127u||d2>127u)return false;
    p={P006_PACKET_MAGIC,seq,status,d1,d2,0u};
    p.crc=static_cast<std::uint8_t>(0x5au^p.magic^p.seq^p.status^p.d1^p.d2);
    return true;
}
inline bool validate(P006MidiPacket const& p) noexcept {
    return p.magic==P006_PACKET_MAGIC && p.status>=0x80u && p.status<0xf0u &&
        p.d1<128u && p.d2<128u &&
        p.crc==static_cast<std::uint8_t>(0x5au^p.magic^p.seq^p.status^p.d1^p.d2);
}
} // namespace p006
