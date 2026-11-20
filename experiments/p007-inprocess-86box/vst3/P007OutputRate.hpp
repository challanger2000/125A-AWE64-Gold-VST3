#pragma once
/* P007: host-output sample-rate conversion ONLY, after original EMU8000/86Box PCM.
 * Original 86Box WT_FREQ is fixed at 44100 Hz (src/include/86box/sound.h).
 * The original EMU8000 and all hardware-calibrated filter/envelope/DSP
 * algorithms are untouched. Studio Pro at 48/96 kHz must not interpret
 * original 44100 samples as if they were native host-rate samples.
 * At 44100 Hz the exact original float PCM is copied without interpolation.
 * No allocations, locks, system calls, or sample-rate adjustment inside 86Box.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace p007 {
class OutputRate final {
public:
    using Pull=std::uint32_t (*)(float*,std::uint32_t);
    static constexpr double kOriginalWavetableHz=44100.0;
    void configure(double hostHz) noexcept {
        if(!std::isfinite(hostHz)||hostHz<22050.0||hostHz>384000.0)
            hostHz=kOriginalWavetableHz;
        step_=kOriginalWavetableHz/hostHz;
        native_=hostHz==kOriginalWavetableHz;
        reset();
    }
    void reset() noexcept {
        phase_=0.0;primed_=false;
        a_[0]=a_[1]=b_[0]=b_[1]=0.0f;
    }
    [[nodiscard]] bool isNative() const noexcept {return native_;}
    [[nodiscard]] double inputStep() const noexcept {return step_;}
    /* Bounded real-time process method, single consumer: VST3 callback. */
    void render(Pull pull,float* left,float* right,std::uint32_t frames) noexcept {
        if(!left||!right)return;
        if(!pull){
            for(std::uint32_t i=0;i<frames;++i)left[i]=right[i]=0.0f;
            return;
        }
        if(native_){
            float block[512u*2u]{};
            for(std::uint32_t pos=0;pos<frames;){
                const auto n=std::min<std::uint32_t>(512u,frames-pos);
                const auto got=std::min(pull(block,n),n);
                for(std::uint32_t k=0;k<got;++k){
                    left[pos+k]=block[2u*k];right[pos+k]=block[2u*k+1u];
                }
                for(std::uint32_t k=got;k<n;++k)left[pos+k]=right[pos+k]=0.0f;
                pos+=n;
            }
            return;
        }
        for(std::uint32_t i=0;i<frames;++i){
            if(!primed_){
                if(pull(a_,1u)!=1u||pull(b_,1u)!=1u){
                    primed_=false;
                    left[i]=right[i]=0.0f;
                    continue;
                }
                phase_=0.0;primed_=true;
            }
            const float f=static_cast<float>(phase_);
            left[i]=a_[0]+(b_[0]-a_[0])*f;
            right[i]=a_[1]+(b_[1]-a_[1])*f;
            phase_+=step_;
            while(phase_>=1.0){
                phase_-=1.0;
                a_[0]=b_[0];a_[1]=b_[1];
                if(pull(b_,1u)!=1u){
                    primed_=false;
                    phase_=0.0;
                    break;
                }
            }
        }
    }
private:
    double step_=1.0;
    double phase_=0.0;
    float a_[2]{},b_[2]{};
    bool native_=true;
    bool primed_=false;
};
} // namespace p007
