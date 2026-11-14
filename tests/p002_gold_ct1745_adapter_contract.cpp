// Contract vectors independently measured by executing the ORIGINAL
// pinned 86Box snd_sb.c function in
// tests/p002_fork_awe64_wavetable_mixer_path.c.
// No private Windows95/Creative ROM, no DSP approximations.
#include "OriginalCt1745WavetableMixer.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

using awe64::card::OriginalCt1745WavetableMixer;
#define CHECK(x) do { if(!(x)) { \
    std::fprintf(stderr,"P002_CT1745_SOURCE_VECTOR_FAIL line=%d %s\n",__LINE__,#x);\
    return 1; } } while(0)

int main() {
    OriginalCt1745WavetableMixer card;
    for(const auto reg: {0x30u,0x31u,0x34u,0x35u})
        CHECK(card.registerValue(static_cast<std::uint8_t>(reg))==0xf8u);
    for(const auto reg: {0x41u,0x42u})
        CHECK(card.registerValue(static_cast<std::uint8_t>(reg))==0u);
    for(const auto reg: {0x44u,0x45u,0x46u,0x47u})
        CHECK(card.registerValue(static_cast<std::uint8_t>(reg))==0x80u);
    const std::array<std::int32_t,8> input{
        -40000,40000,-32768,32767,-1,1,12345,-12345
    };
    const std::array<std::int32_t,8> forkReset{
        -39997,39997,-32766,32765,0,0,12344,-12344
    };
    for(std::size_t i=0;i<input.size();i+=2u) {
        const auto p=card.process(input[i],input[i+1]);
        CHECK(p.left==forkReset[i] && p.right==forkReset[i+1]);
    }
    CHECK(card.floatSink(forkReset[1])==static_cast<float>(39997)/32768.f);
    CHECK(card.floatSink(forkReset[1])>1.f);
    CHECK(card.preCardPcm16Tap(40000)==32767.f/32768.f);
    CHECK(card.preCardPcm16Tap(-40000)==-1.f);
    CHECK(card.preCardPcm16Tap(12345)==12345.f/32768.f);
    CHECK(!card.applyVerifiedOriginalRegister(0x34u,0x00u,false));
    CHECK(card.registerValue(0x34u)==0xf8u);
    CHECK(!card.applyVerifiedOriginalRegister(0x32u,0x00u,true));
    CHECK(card.applyVerifiedOriginalRegister(0x34u,0x00u,true));
    const std::array<std::int32_t,8> originalLeftMin{
        -30,39997,-24,32765,0,0,9,-12344
    };
    for(std::size_t i=0;i<input.size();i+=2u) {
        const auto p=card.process(input[i],input[i+1]);
        CHECK(p.left==originalLeftMin[i] && p.right==originalLeftMin[i+1]);
    }
    card.reset();
    CHECK(card.applyVerifiedOriginalRegister(0x46u,0xf0u,true));
    auto bass=card.process(-40000,40000);
    CHECK(bass.left!=forkReset[0]);
    CHECK(bass.right==forkReset[1]);  // original per-channel tone branch
    // Source CT1745 right Treble Cut special case, 86Box snd_sb.c:591:
    // dry term uses out_l, not out_r.  First reset filter frame can
    // be checked algebraically against the exact pinned filter coeff.
    card.reset();
    CHECK(card.applyVerifiedOriginalRegister(0x45u,0x00u,true));
    const auto asymmetric=card.process(-40000,40000);
    const double preL=static_cast<double>(-40000)*(32767.0/32768.0)*
                                                    (32767.0/32768.0);
    const double preR=static_cast<double>(40000)*(32767.0/32768.0)*
                                                   (32767.0/32768.0);
    const double ratio=0.199526231;
    const auto forkRight=static_cast<std::int32_t>(
        preL*ratio+
        (0.03927726802250377400*preR)*(1.0-ratio));
    CHECK(asymmetric.right==forkRight);
    CHECK(asymmetric.left==forkReset[0]);
    // Per-instance original filter state: two plugins must not share
    // the original PC-emulator's static global IIR samples.
    OriginalCt1745WavetableMixer independent;
    CHECK(independent.applyVerifiedOriginalRegister(0x46u,0xf0u,true));
    const auto first=independent.process(-40000,40000);
    CHECK(first.left==bass.left && first.right==bass.right);
    CHECK(card.applyVerifiedOriginalRegister(0x22u,0x00u,true));
    CHECK(card.registerValue(0x30u)==0x08u);
    CHECK(card.registerValue(0x31u)==0x08u);
    CHECK(card.applyVerifiedOriginalRegister(0x26u,0xffu,true));
    CHECK(card.registerValue(0x34u)==0xf8u);
    CHECK(card.registerValue(0x35u)==0xf8u);
    std::puts("P002_PINNED_FORK_CT1745_POST_CHIP_ADAPTER_REFERENCE_PASS");
    std::puts("real_original_snd_sb_c_vectors=TRUE original_float_no_pcm16_clamp=TRUE");
    std::puts("original_right_treble_cut_asymmetry=RETAINED per_instance_filters=TRUE");
    std::puts("original_pre_card_pcm16_tap=PRESERVED source_unverified_writes=REJECTED");
}
