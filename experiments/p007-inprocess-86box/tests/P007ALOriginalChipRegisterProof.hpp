#pragma once
// P007AM: passive, voice-aware decoder of the original EMU8000 port stream.
// Registers are addressed through the 0xE22 pointer:
// low five bits = original Creative-allocated voice 0..31,
// bits 5..7 = EMU8000 register index. A voice-0-only test gives false failures
// when original Creative's polyphonic allocator rotates through other voices.
// This header is ONLY for private QA of a real Win95+Creative+original 86Box run.
#include <cstdint>

namespace p007::original_chip_register_proof {
struct Capture {
    static constexpr unsigned kMissingVoice=32u;
    unsigned voice=kMissingVoice;
    std::uint16_t pointer=0xffffu;
    std::uint16_t pefe=0u,volumeRelease=0u,filterRelease=0u;
    std::uint16_t filterNoteSustain=0u;
    bool hasPefe=false,hasVolumeRelease=false,hasFilterRelease=false;
    bool hasFilterNoteSustain=false;
    bool conflict=false;

    void observe(std::uint16_t port,std::uint16_t value) noexcept {
        if(port==0x0e22u) { pointer=value;return; }
        // The EMU8K keeps the E22 pointer until another pointer write.
        const unsigned reg=pointer&0x00e0u;
        const unsigned addressedVoice=pointer&0x001fu;
        const bool isPefe=(port==0x0e20u && reg==0x0040u);
        const bool isVolumeRelease=(port==0x0a20u && reg==0x00a0u &&
                                    (value&0x8000u)!=0u);
        const bool isFilterRelease=(port==0x0a20u && reg==0x00e0u &&
                                    (value&0x8000u)!=0u);
        const bool isFilterNoteSustain=(port==0x0a20u && reg==0x00e0u &&
                                        (value&0x8000u)==0u);
        if(!isPefe&&!isVolumeRelease&&!isFilterRelease&&
           !isFilterNoteSustain)return;
        if(voice==kMissingVoice) voice=addressedVoice;
        if(voice!=addressedVoice) { conflict=true;return; }
        if(isPefe) {
            if(hasPefe && pefe!=value)conflict=true;
            pefe=value;hasPefe=true;
        }
        if(isVolumeRelease) {
            if(hasVolumeRelease && volumeRelease!=value)conflict=true;
            volumeRelease=value;hasVolumeRelease=true;
        }
        if(isFilterRelease) {
            if(hasFilterRelease && filterRelease!=value)conflict=true;
            filterRelease=value;hasFilterRelease=true;
        }
        if(isFilterNoteSustain) {
            if(hasFilterNoteSustain && filterNoteSustain!=value)conflict=true;
            filterNoteSustain=value;hasFilterNoteSustain=true;
        }
    }
    bool complete() const noexcept {
        return voice<kMissingVoice && hasPefe && hasVolumeRelease &&
               hasFilterRelease && !conflict;
    }
};
} // namespace p007::original_chip_register_proof
