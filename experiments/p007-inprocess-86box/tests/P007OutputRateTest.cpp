#include "P007OutputRate.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <algorithm>
namespace {
constexpr unsigned kFrames=10000u;
std::array<float,kFrames*2u> src{};
unsigned cursor=0u;
unsigned pull(float* out,unsigned requested) {
    const unsigned got=std::min(requested,kFrames-cursor);
    for(unsigned i=0;i<got*2u;++i)out[i]=src[cursor*2u+i];
    cursor+=got;
    return got;
}
void resetInput(){
    cursor=0u;
    for(unsigned i=0;i<kFrames;++i){
        src[2u*i]=static_cast<float>(i)*0.015625f;
        src[2u*i+1u]=-static_cast<float>(i)*0.03125f;
    }
}
void testRate(double hz){
    resetInput();p007::OutputRate rate;rate.configure(hz);
    std::array<float,257> left{},right{};
    unsigned count=0u;
    for(unsigned block=0;block<10u;++block) {
        rate.render(pull,left.data(),right.data(),257u);
        for(unsigned i=0;i<257u;++i,++count) {
            const double expected=double(count)*(44100.0/hz);
            assert(std::abs(double(left[i])-expected*0.015625)<0.002);
            assert(std::abs(double(right[i])+expected*0.03125)<0.002);
        }
    }
    assert(std::abs(double(cursor)-double(count)*44100.0/hz)<4.0);
    std::printf("P007_ORIGINAL_44100_TO_HOST_%.0f_TEST_PASS source=%u out=%u\n",
                hz,cursor,count);
}
void testUnderrun(){
    resetInput();p007::OutputRate rate;rate.configure(48000.0);
    std::array<float,40> left{},right{};
    cursor=kFrames-2u;
    rate.render(pull,left.data(),right.data(),40u);
    for(unsigned i=10u;i<40u;++i)assert(left[i]==0.0f&&right[i]==0.0f);
    assert(cursor==kFrames);
    rate.reset();resetInput();
    rate.render(pull,left.data(),right.data(),40u);
    assert(left[0]==0.0f&&left[1]>0.0f);
    std::puts("P007_HOST_RATE_UNDERRUN_AND_REPRIME_PASS");
}
void testInvalidHost(){
    p007::OutputRate rate;
    rate.configure(NAN);assert(rate.isNative());
    rate.configure(INFINITY);assert(rate.isNative());
    rate.configure(0.0);assert(rate.isNative());
    rate.configure(48000.0);assert(!rate.isNative());
    rate.configure(44100.0);assert(rate.isNative());
    std::puts("P007_HOST_RATE_INVALID_CONFIGURATION_SAFE_PASS");
}
}
int main(){
    testRate(44100.0);testRate(48000.0);
    testRate(96000.0);testRate(32000.0);
    testUnderrun();testInvalidHost();
    return 0;
}
