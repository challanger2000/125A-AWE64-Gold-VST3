// P005: independent cross-check of the two existing native Creative
// Mode-1 allocators, one using LIVE port callbacks and the other an
// authenticated-state snapshot. SYNTHETIC ONLY, not original Win95 proof.
// No CPU timing, DSP changes, recorded port replies, or sample assets.
#include "OriginalCreativeVxdLive.hpp"
#include "CreativeVxdAdditionalAllocatorModes.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace awe64;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"P005_MODE1_DIFF_FAIL:%d %s\n",__LINE__,#x);return 1;}}while(false)
struct Chip {
    std::uint8_t selected=0u;
    std::array<std::uint32_t,32> vtft{};
    std::array<unsigned,32> reads{};
    static bool out8(void* p,std::uint16_t port,std::uint8_t value) noexcept {
        auto& c=*static_cast<Chip*>(p);
        if(port!=0x0e22u)return false;
        c.selected=value;return true;
    }
    static bool out16(void* p,std::uint16_t port,std::uint16_t value) noexcept {
        auto& c=*static_cast<Chip*>(p);
        return port==0x0a20u && (c.selected&0xe0u)==0xa0u &&
               value==0xffu;
    }
    static bool in16(void*,std::uint16_t,std::uint16_t&) noexcept {
        return false; // all rows use idle key marker; no DCYSUSV read
    }
    static bool in32(void* p,std::uint16_t port,std::uint32_t& value) noexcept {
        auto& c=*static_cast<Chip*>(p);
        if(port!=0x0620u || (c.selected&0xe0u)!=0x60u)return false;
        const auto v=c.selected&31u;
        ++c.reads[v];
        value=c.vtft[v];
        return true;
    }
    original_vxd::PortIo io() noexcept {
        return {this,out8,out16,in16,in32,nullptr};
    }
};
int main(){
    unsigned cases=0u,selected=0u,empty=0u;
    for(unsigned capacity=2u;capacity<=8u;++capacity) {
        for(unsigned mode : {2u,4u}) {
            for(unsigned mask=0;mask<(1u<<capacity);++mask) {
                // Both all-idle and deliberately varied nonzero VTFTs
                // cover the immediate and weighted score branches.
                for(unsigned pattern : {0u,1u,2u}) {
                    original_vxd::DeviceState live{};
                    live.physical_capacity=static_cast<std::uint16_t>(capacity);
                    live.mode_count=mode;
                    std::array<creative::OriginalWin95AllocatorVoiceSnapshot,
                               creative::kOriginalWin95PhysicalVoiceCapacity> snaps{};
                    Chip chip{};
                    for(unsigned voice=0;voice<capacity;++voice) {
                        const auto is_blocked=((mask>>voice)&1u)!=0u;
                        const unsigned score=(pattern==0u ? 0u :
                            pattern==1u ? 30u+17u*voice :
                            200u+((voice*voice+7u*mask)%31u));
                        live.voices[voice].blocked=is_blocked;
                        live.voices[voice].key_marker=0x0ffeu;
                        live.voices[voice].sample_end=0u;
                        chip.vtft[voice]=score<<16u;
                        auto& snap=snaps[voice];
                        snap.complete=!is_blocked; // missing chip only on
                                                    // gated rows in mode 2
                        if(mode>2u)snap.complete=true;
                        snap.blocked_when_few_voices=is_blocked;
                        snap.internal_key_marker=0x0ffeu;
                        snap.volume_target_hi16=static_cast<std::uint16_t>(score);
                        snap.stored_sample_end=0u;
                    }
                    const auto actual=original_vxd::allocate_pair_mode1(
                        live,chip.io());
                    const auto expected=creative::candidateOriginalWin95Mode1Pair(
                        snaps,static_cast<std::uint16_t>(capacity),mode);
                    CHECK(actual.status==original_vxd::Status::Found ||
                          actual.status==original_vxd::Status::NoCandidate);
                    CHECK((actual.status==original_vxd::Status::Found)==
                          expected.has_value());
                    if(expected) {
                        CHECK(actual.first==expected->first_voice);
                        CHECK(actual.second==expected->second_voice);
                        CHECK(actual.priority==expected->priority);
                        CHECK(actual.immediate==expected->immediate_zero_pair);
                        ++selected;
                    }else ++empty;
                    for(unsigned v=0;v<capacity;++v) {
                        const auto gated=mode<=2u &&
                            (snaps[v].blocked_when_few_voices ||
                             (v+1u<capacity &&
                              snaps[v+1u].blocked_when_few_voices));
                        // Every source-gated row has NO chip read.
                        if(gated)CHECK(chip.reads[v]==0u);
                        else CHECK(chip.reads[v]==1u ||
                                   actual.immediate);
                    }
                    ++cases;
                }
            }
        }
    }
    CHECK(cases==3048u);
    CHECK(selected>0u && empty>0u);
    std::printf("P005_MODE1_SNAPSHOT_VS_LIVE_PASS cases=%u selected=%u empty=%u\n",
                cases,selected,empty);
}
