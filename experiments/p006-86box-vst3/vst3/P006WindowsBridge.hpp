#pragma once
// Native Windows-x64 86Box companion process IPC.
// The audio callback NEVER calls Windows pipe/file/process APIs.
#include "P006Transport.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
namespace p006 {
class AudioMapping final {
    HANDLE mapping_=nullptr;
    P006AudioShared* data_=nullptr;
public:
    AudioMapping()=default;
    AudioMapping(AudioMapping const&)=delete;
    AudioMapping& operator=(AudioMapping const&)=delete;
    ~AudioMapping(){close();}
    [[nodiscard]] bool create(char const* name) noexcept {
        if(data_||mapping_||!name||!*name)return false;
        mapping_=CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,
            PAGE_READWRITE,0u,static_cast<DWORD>(sizeof(P006AudioShared)),name);
        if(!mapping_)return false;
        if(GetLastError()==ERROR_ALREADY_EXISTS){
            CloseHandle(mapping_);mapping_=nullptr;return false;
        }
        data_=static_cast<P006AudioShared*>(MapViewOfFile(mapping_,
            FILE_MAP_READ|FILE_MAP_WRITE,0u,0u,sizeof(P006AudioShared)));
        if(!data_){CloseHandle(mapping_);mapping_=nullptr;return false;}
        init(*data_);
        return true;
    }
    void close() noexcept {
        if(data_){UnmapViewOfFile(data_);data_=nullptr;}
        if(mapping_){CloseHandle(mapping_);mapping_=nullptr;}
    }
    [[nodiscard]] P006AudioShared* data() const noexcept{return data_;}
};
class MidiPipe final {
    static constexpr std::uint32_t kCapacity=2048u;
    static constexpr std::uint32_t kMask=kCapacity-1u;
    struct Queue {
        std::array<P006MidiPacket,kCapacity> slots{};
        std::atomic<std::uint32_t> produced{0},consumed{0};
    } queue_{};
    std::thread worker_{};
    std::atomic<bool> stopping_{false};
    std::atomic<std::uint32_t> lost_{0},sent_{0};
    std::uint8_t sequence_=0u;  // only written by DAW RT thread
    char pipeName_[128]{};
    void work() noexcept {
        HANDLE pipe=INVALID_HANDLE_VALUE;
        while(!stopping_.load(std::memory_order_acquire)){
            if(pipe==INVALID_HANDLE_VALUE){
                pipe=CreateFileA(pipeName_,GENERIC_READ|GENERIC_WRITE,
                    0u,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(pipe==INVALID_HANDLE_VALUE){Sleep(10u);continue;}
            }
            const auto rd=queue_.consumed.load(std::memory_order_relaxed);
            const auto wr=queue_.produced.load(std::memory_order_acquire);
            if(rd==wr){Sleep(1u);continue;}
            const auto& packet=queue_.slots[rd&kMask];
            DWORD n=0u;
            const BOOL ok=WriteFile(pipe,&packet,P006_PACKET_SIZE,&n,nullptr);
            if(!ok||n!=P006_PACKET_SIZE){
                CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;
                Sleep(10u);continue;
            }
            queue_.consumed.store(rd+1u,std::memory_order_release);
            sent_.fetch_add(1u,std::memory_order_relaxed);
        }
        if(pipe!=INVALID_HANDLE_VALUE)CloseHandle(pipe);
    }
public:
    MidiPipe()=default;
    MidiPipe(MidiPipe const&)=delete;
    MidiPipe& operator=(MidiPipe const&)=delete;
    ~MidiPipe(){stopNonRealtime();}
    [[nodiscard]] bool startNonRealtime(char const* name) noexcept {
        if(worker_.joinable()||!name)return false;
        std::size_t n=0u;
        while(n+1u<sizeof(pipeName_)&&name[n]){pipeName_[n]=name[n];++n;}
        if(n==0u||name[n])return false;
        pipeName_[n]='\0';
        stopping_.store(false,std::memory_order_release);
        try{worker_=std::thread([this]{work();});}
        catch(...){return false;}
        return true;
    }
    void stopNonRealtime() noexcept {
        stopping_.store(true,std::memory_order_release);
        if(worker_.joinable()){
            // Cancel a blocked synchronous WriteFile on Win11 before join.
            (void)CancelSynchronousIo(
                reinterpret_cast<HANDLE>(worker_.native_handle()));
            worker_.join();
        }
    }
    // Bounded no-heap SPSC producer in VST3 process(), never Windows I/O.
    [[nodiscard]] bool submit(std::uint8_t status,std::uint8_t d1,
                              std::uint8_t d2) noexcept {
        P006MidiPacket p{};
        if(!pack(p,sequence_++,status,d1,d2))return false;
        const auto wr=queue_.produced.load(std::memory_order_relaxed);
        const auto rd=queue_.consumed.load(std::memory_order_acquire);
        if(wr-rd>=kCapacity){
            lost_.fetch_add(1u,std::memory_order_relaxed);return false;
        }
        queue_.slots[wr&kMask]=p;
        queue_.produced.store(wr+1u,std::memory_order_release);
        return true;
    }
    [[nodiscard]] std::uint32_t lost() const noexcept {
        return lost_.load(std::memory_order_relaxed);
    }
};
} // namespace p006
#endif
