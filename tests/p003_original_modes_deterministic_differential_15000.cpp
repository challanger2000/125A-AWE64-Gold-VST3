// P003 deterministic x86-source-derived differential allocator regression.
// 15,000 complete synthetic driver/chip snapshots, all three genuine original
// dispatcher modes. No private Creative bytes or recordings are used in CI.
// The reference algorithms below are INDEPENDENT of production functions.
// This validates source-control-flow consistency, NOT hardware/PCM identity.
#include "CreativeVxdAdditionalAllocatorModes.h"
#include "CreativeVxdMonoVoiceRanking.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>

namespace ac=awe64::creative;
using V=ac::OriginalWin95AllocatorVoiceSnapshot;
using M=ac::OriginalWin95MonoVoiceSnapshot;
using P=ac::OriginalWin95PairSelection;
using S=ac::OriginalWin95MonoSelection;
using R=ac::OriginalWin95MonoSelectionReason;

static std::uint32_t seed=0x27c0ffe5u;
std::uint32_t rnd(){
    seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
    return seed;
}
std::uint32_t score(std::uint16_t marker, std::uint16_t vtft,
                    std::uint16_t dcys) {
    std::uint32_t s=vtft;
    if(marker!=0x0ffeu){
        s+=(marker&0x1000u)?0x1200u:0x200u;
        if((marker&0xffu)==0xffu)s+=0x300u;
        else if(!(dcys&0x8000u))s+=0x1300u;
    }
    return s;
}
bool blocked(const V& v,std::uint32_t count) {
    return count<=2u && v.blocked_when_few_voices;
}
bool done(const V& v) {
    return v.stored_sample_end && v.stored_sample_end<=(v.chip_current_address&0xffffffu);
}
std::optional<P> original_pair(
 const std::array<V,32>& v,unsigned cap,std::uint32_t mode) {
    if(cap<2||cap>32)return {};
    std::uint32_t best=0xfffffu,prev=0xfffffu;
    std::optional<P> selected;
    // Source LCOD+3FE8..+4167, original descending voice loop.
    for(unsigned j=cap;j>0;--j){
        unsigned i=j-1; const auto &x=v[i];
        if(blocked(x,mode) || (i+1<cap&&blocked(v[i+1],mode)) ||
           x.internal_key_marker>=0x2000u) {
            prev=0xfffffu;continue;
        }
        std::uint32_t cur=score(x.internal_key_marker,x.volume_target_hi16,x.envelope_status_reg);
        if(done(x))cur=0;
        std::uint32_t sum=prev+cur;
        if(i+1<cap && sum==0u)return P{static_cast<std::uint8_t>(i),
                                    static_cast<std::uint8_t>(i+1),0,true};
        std::uint32_t candidate=sum+2*std::max(prev,cur)+
                                (x.pair_extra_penalty?0xc00u:0u);
        if(i+1<cap && candidate<=best){
            best=candidate;selected=P{static_cast<std::uint8_t>(i),
                                    static_cast<std::uint8_t>(i+1),candidate,false};
        }
        prev=cur;
    }
    return selected;
}
std::optional<S> original_range(const std::array<V,32>& v,
                                unsigned beg,unsigned end,
                                unsigned cap,std::uint32_t mode) {
    if(!cap||cap>32)return {};
    end=std::min(end,cap);
    if(beg>=end)return {};
    std::uint32_t best=0xffffffffu;
    std::optional<S> chosen;
    // Source LCOD+416C..+42A2, original ascending loop.
    for(unsigned i=beg;i<end;++i){
        const auto &x=v[i];
        if(blocked(x,mode))continue;
        if(x.internal_key_marker<0x2000u){
            std::uint32_t cur=score(x.internal_key_marker,x.volume_target_hi16,x.envelope_status_reg);
            if(x.internal_key_marker==0x0ffeu && cur==0u)
                return S{static_cast<std::uint8_t>(i),R::CompletelyIdle,0u};
            if(cur<=best){
                best=cur;chosen=S{static_cast<std::uint8_t>(i),R::LowestEnvelopePriority,cur};
            }
        }
        if(done(x))return S{static_cast<std::uint8_t>(i),R::SampleEndReached,0u};
    }
    return chosen;
}
std::optional<S> original_mode0(const std::array<V,32>& v,
                                unsigned cap,std::uint32_t mode) {
    if(!cap||cap>32)return {};
    std::uint32_t best=0xffffffffu;
    std::optional<S> chosen;
    // Original +3EA1, triple interleaved scan lanes 0,1,2.
    for(unsigned lane=0;lane<3;++lane){
        for(unsigned i=lane;i<cap;i+=3){
            const auto &x=v[i];
            if(blocked(x,mode))continue;
            if(x.internal_key_marker<0x2000u){
                auto cur=score(x.internal_key_marker,x.volume_target_hi16,x.envelope_status_reg);
                if(x.internal_key_marker==0x0ffeu && cur==0u)
                    return S{static_cast<std::uint8_t>(i),R::CompletelyIdle,0u};
                if(cur<=best){
                    best=cur;chosen=S{static_cast<std::uint8_t>(i),R::LowestEnvelopePriority,cur};
                }
            }
            if(done(x))return S{static_cast<std::uint8_t>(i),R::SampleEndReached,0u};
        }
    }
    return chosen;
}
bool eq(const std::optional<P>& a,const std::optional<P>& b) {
    if(bool(a)!=bool(b))return false;
    return !a||(a->first_voice==b->first_voice&&
                 a->second_voice==b->second_voice&&
                 a->priority==b->priority&&
                 a->immediate_zero_pair==b->immediate_zero_pair);
}
bool eq(const std::optional<S>& a,const std::optional<S>& b) {
    if(bool(a)!=bool(b))return false;
    return !a||(a->physical_voice==b->physical_voice&&
                 a->reason==b->reason&&a->priority==b->priority);
}
int main(){
    constexpr std::array<std::uint16_t,12> marks{
        0x0ffe,0x0ffe,0x0ffe,0x2000,0x2001,0x01ff,
        0x10ff,0x0100,0x1100,0x0000,0x1fff,0x1ffe
    };
    for(unsigned test=0;test<15000;++test) {
        std::array<V,32> v{};
        const auto mode=std::array<std::uint32_t,6>{1,2,3,4,32,2}[rnd()%6];
        unsigned capacity=(rnd()%32)+1;
        for(auto& x:v){
            x.complete=true;
            x.internal_key_marker=marks[rnd()%marks.size()];
            x.volume_target_hi16=static_cast<std::uint16_t>(rnd()%65536u);
            if((rnd()%13u)==0)x.volume_target_hi16=0u;
            x.envelope_status_reg=(rnd()%3u)?0x8000u:0u;
            x.blocked_when_few_voices=(rnd()%9u)==0u;
            x.pair_extra_penalty=(rnd()%5u)==0u;
            x.stored_sample_end=(rnd()%4u)?0u:((rnd()%5000u)+1u);
            x.chip_current_address=rnd()%6500u;
        }
        auto pair_ref=original_pair(v,capacity,mode);
        auto pair_impl=ac::candidateOriginalWin95Mode1Pair(v,static_cast<std::uint16_t>(capacity),mode);
        if(!eq(pair_ref,pair_impl)) {
            std::printf("P003_DIFF_MODE1 test=%u cap=%u mode=%u\n",test,capacity,mode);
            return 1;
        }
        unsigned beg=rnd()%capacity;
        unsigned end=beg+1u+(rnd()%(capacity-beg));
        auto range_ref=original_range(v,beg,end,capacity,mode);
        auto range_impl=ac::candidateOriginalWin95Mode2Range(
            v,(end<<8u)|beg,static_cast<std::uint16_t>(capacity),mode);
        if(!eq(range_ref,range_impl)) {
            std::printf("P003_DIFF_MODE2 test=%u cap=%u mode=%u beg=%u end=%u\n",
                        test,capacity,mode,beg,end);
            return 2;
        }
        unsigned mono_limit=std::min(capacity,30u);
        std::array<M,ac::kOriginalCreativeMusicVoices> mono{};
        for(unsigned i=0;i<mono_limit;++i){
            mono[i].complete=true;
            mono[i].internal_key_marker=v[i].internal_key_marker;
            mono[i].volume_target_hi16=v[i].volume_target_hi16;
            mono[i].envelope_status_reg=v[i].envelope_status_reg;
            mono[i].stored_sample_end=v[i].stored_sample_end;
            mono[i].chip_current_address=v[i].chip_current_address;
            mono[i].blocked_when_few_voices=v[i].blocked_when_few_voices;
        }
        auto mono_ref=original_mode0(v,mono_limit,mode);
        auto mono_impl=ac::candidateOriginalWin95Mode0MonoVoice(mono,mono_limit,mode);
        if(!eq(mono_ref,mono_impl)){
            std::printf("P003_DIFF_MODE0_LEGACY30 test=%u cap=%u mode=%u\n",
                        test,capacity,mode);
            return 3;
        }
        // Unlike legacy 30-voice shorthand, the ORIGINAL +3ED4
        // dispatcher is bounded by verified device+0x06 up to 32.
        const auto full_ref=original_mode0(v,capacity,mode);
        ac::OriginalWin95AllocatorDispatchInput input{};
        input.independently_verified_source_state=true;
        input.original_x86_dispatch_mode=0u;
        input.original_device_physical_capacity=
            static_cast<std::uint16_t>(capacity);
        input.original_device_mode_count=mode;
        input.independently_verified_mode0_scan_ceiling=
            static_cast<std::uint8_t>(capacity);
        const auto full_impl=ac::candidateOriginalWin95VxdDispatchAllModes(v,input);
        if(!full_impl ||
           full_impl->voice_count!=(full_ref?1u:0u) ||
           (full_ref &&
            (full_impl->physical_voices[0]!=full_ref->physical_voice ||
             full_impl->priority!=full_ref->priority))) {
            std::printf("P003_DIFF_MODE0_PHYSICAL32 test=%u cap=%u mode=%u\n",
                        test,capacity,mode);
            return 4;
        }
    }
    std::puts("P003_SOURCE_DERIVED_MODE0_MODE1_MODE2_DIFFERENTIAL_15000_PASS"
              " no_original_live_runtime=YES no_private_data=YES");
    return 0;
}
