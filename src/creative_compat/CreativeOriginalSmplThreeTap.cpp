#include "CreativeOriginalSmplThreeTap.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace awe64::creative {
std::optional<std::vector<std::int16_t>>
p003PredictOwnerObservedThreeTapRamWords(
    const std::span<const std::uint8_t> bytes) {
    if(bytes.size()<6u || bytes.size()%2u!=0u ||
       bytes.size()/2u>0xe00000u)
        return std::nullopt;
    const auto n=bytes.size()/2u;
    try {
        std::vector<std::int16_t> source(n),output(n,0);
        std::vector<std::uint8_t> preserve_zeros(n,0);
        for(std::size_t i=0;i<n;++i) {
            const std::uint16_t w=static_cast<std::uint16_t>(
                std::uint16_t(bytes[2u*i]) |
                (std::uint16_t(bytes[2u*i+1u])<<8u));
            source[i]=std::bit_cast<std::int16_t>(w);
        }
        if(source.front()!=0 || source.back()!=0)
            return std::nullopt;
        for(std::size_t i=0u;i<n;) {
            if(source[i]!=0) {
                ++i;
                continue;
            }
            auto j=i+1u;
            while(j<n && source[j]==0)++j;
            if(j-i>=3u)
                for(auto at=i;at<j;++at)preserve_zeros[at]=1u;
            i=j;
        }
        // Exact signed floor division, rather than relying on
        // implementation-defined signed right shift of negative values.
        for(std::size_t i=1u;i+1u<n;++i) {
            if(preserve_zeros[i])continue;
            const std::int32_t v=6*std::int32_t(source[i])-
                                  std::int32_t(source[i-1u])-
                                  std::int32_t(source[i+1u]);
            const std::int32_t rounded=v>=0?v/8:-((-v+7)/8);
            if(rounded<-32768 || rounded>32767)
                return std::nullopt;
            output[i]=static_cast<std::int16_t>(rounded);
        }
        return output;
    }catch(...) {
        return std::nullopt;
    }
}
} // namespace awe64::creative
