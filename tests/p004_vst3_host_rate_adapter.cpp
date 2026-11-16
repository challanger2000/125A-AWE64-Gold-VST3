#include "P004HostRateAdapter.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <vector>

using awe64::vst3::P004HostRateAdapter;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"P004_RATE_FAIL line %d: %s\n",__LINE__,#x); return 1; }} while(false)
namespace {
using Stereo=P004HostRateAdapter::Stereo;
std::vector<Stereo> run(std::uint32_t rate,bool chunked,
                        std::uint64_t& nativeCalls) {
    P004HostRateAdapter converter;
    if(!converter.configure(rate))return {};
    nativeCalls=0u;
    constexpr std::size_t frames=8192u;
    std::vector<Stereo> result;
    result.reserve(frames);
    const auto readNative=[&]() noexcept {
        const double t=double(nativeCalls)/44100.;
        ++nativeCalls;
        return Stereo{
            static_cast<float>(0.75*std::sin(2.*std::numbers::pi*1000.*t)),
            static_cast<float>(0.5*std::cos(2.*std::numbers::pi*1000.*t))};
    };
    for(std::size_t i=0;i<frames;) {
        const std::size_t count=chunked?
            std::min<std::size_t>({frames-i,1u+(i%73u),64u}):
            frames-i;
        for(std::size_t j=0;j<count;++j)result.push_back(converter.next(readNative));
        i+=count;
    }
    if(converter.producedHostFrames()!=frames ||
       converter.consumedSourceFrames()!=nativeCalls)
        return {};
    return result;
}
}
int main() {
    CHECK(P004HostRateAdapter::supports(44100u));
    CHECK(!P004HostRateAdapter::supports(32000u));
    CHECK(!P004HostRateAdapter::supports(192000u));
    for(const auto rate:{48000u,88200u,96000u}) {
        std::uint64_t contiguousCalls=0u,chunkedCalls=0u;
        const auto contiguous=run(rate,false,contiguousCalls);
        const auto chunked=run(rate,true,chunkedCalls);
        CHECK(contiguous.size()==8192u && chunked.size()==contiguous.size());
        CHECK(contiguousCalls==chunkedCalls);
        // Real host callback sizes cannot influence EMU8000 clock or PCM.
        for(std::size_t i=0;i<contiguous.size();++i)
            CHECK(contiguous[i].left==chunked[i].left &&
                  contiguous[i].right==chunked[i].right);
        const auto expected=1u+(8191u*44100u)/rate;
        CHECK(contiguousCalls==expected);
        const auto latency=double(P004HostRateAdapter::kGroupDelayNative)/44100.;
        // Steady-state sine amplitude and phase are measured separately
        // for both channels. No gain compensation in EMU8000/CT1745.
        for(std::size_t i=512u;i<contiguous.size();++i) {
            const double t=double(i)/double(rate)-latency;
            const auto expectedLeft=0.75*std::sin(2.*std::numbers::pi*1000.*t);
            const auto expectedRight=0.5*std::cos(2.*std::numbers::pi*1000.*t);
            CHECK(std::abs(double(contiguous[i].left)-expectedLeft)<0.012);
            CHECK(std::abs(double(contiguous[i].right)-expectedRight)<0.012);
        }
        // A host boundary event must precede the first unrendered chip
        // frame after that boundary. No future-note preread.
        P004HostRateAdapter boundary;
        CHECK(boundary.configure(rate));
        std::uint32_t sourceAtEvent=0;
        const auto unit=[&]() noexcept {
            ++sourceAtEvent;
            return Stereo{1.f,-1.f};
        };
        for(int i=0;i<79;++i)(void)boundary.next(unit);
        CHECK(sourceAtEvent==1u+(78u*44100u)/rate);
        CHECK(boundary.latencySamples()==
              (P004HostRateAdapter::kGroupDelayNative*rate+44099u)/44100u);
        boundary.reset();
        CHECK(boundary.producedHostFrames()==0u &&
              boundary.consumedSourceFrames()==0u);
        // A zero-frame parameter flush must not advance the converter:
        CHECK(boundary.producedHostFrames()==0u);
        (void)boundary.next(unit);
        CHECK(boundary.consumedSourceFrames()==1u);
    }
    std::puts("P004_HOST_RATE_ADAPTER_48_882_96_KHZ_PARTITION_CLOCK_AND_SINE_PASS");
    return 0;
}
