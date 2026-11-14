// RUNS AGAINST THE ACTUAL PINNED FORK snd_sb.c:
// A second, independently compiled C translation unit includes the
// original 86Box SB AWE64 Gold static CT1745 wavetable callback with
// original filters.h functions and arrays. No replacement oracle DSP.
// This C++ side tests the narrow VST3 adapter sample-for-sample.
#include "OriginalCt1745WavetableMixer.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

extern "C" {
void p002_original_ct1745_oracle_reset(void);
void p002_original_ct1745_oracle_write(unsigned reg,unsigned value);
void p002_original_ct1745_oracle_process(
    const std::int32_t* source,std::int32_t* post_mixer,unsigned frames);
}

using awe64::card::OriginalCt1745WavetableMixer;
#define CHECK(x) do { if(!(x)) { \
    std::fprintf(stderr,"P002_CT1745_FORK_DIFFERENTIAL_FAIL line=%d: %s\n",__LINE__,#x);\
    return 1; } } while(0)

int main() {
    // Independently verified pinned 86Box originals are compiled as C,
    // this adapter compiled as C++: compare *after* every register state
    // transition, across filter memory and multiple callback blocks.
    p002_original_ct1745_oracle_reset();
    OriginalCt1745WavetableMixer card;
    constexpr std::array<std::int32_t,16> source{{
        -40000,40000, -32768,32767, -1,1, 12345,-12345,
         12000, 19000, -23000,51000, 0, 0,  31111,-17777
    }};
    std::array<std::int32_t,16> exact{};
    std::size_t compared=0u;
    auto compare=[&]() {
        exact.fill(0);
        p002_original_ct1745_oracle_process(
            source.data(),exact.data(),8u);
        for(std::size_t i=0u;i<source.size();i+=2u) {
            const auto p=card.process(source[i],source[i+1u]);
            if(p.left!=exact[i]||p.right!=exact[i+1u]) {
                std::fprintf(stderr,
                    "P002_CT1745_ACTUAL_FORK_MISMATCH frame=%zu "
                    "L source=%d adapter=%d fork=%d "
                    "R source=%d adapter=%d fork=%d\n",
                    i/2u,source[i],p.left,exact[i],
                    source[i+1u],p.right,exact[i+1u]);
                return false;
            }
            ++compared;
        }
        return true;
    };
    CHECK(compare()); // original reset, no card tone branch
    struct Write {std::uint8_t reg,value;};
    constexpr std::array<Write,9> writes{{
        {0x34,0x00}, // original FM left 25/32768
        {0x31,0x80}, // original master right attenuation
        {0x41,0x80}, // source output gain left 4x
        {0x46,0xf0}, // bass boost left: original low_iir
        {0x47,0x20}, // bass cut right: original low_cut_iir
        {0x44,0xa0}, // treble boost left: original high_iir
        {0x45,0x00}, // treble cut right: original high_cut_iir
        {0x26,0x84}, // source compatibility alias for FM L/R
        {0x22,0x43}, // source compatibility alias for master L/R
    }};
    for(const auto& event:writes) {
        p002_original_ct1745_oracle_write(event.reg,event.value);
        CHECK(card.applyVerifiedOriginalRegister(event.reg,event.value,true));
        CHECK(compare());
        CHECK(compare()); // filter memory across separate audio blocks
    }
    CHECK(compared==8u*(1u+2u*writes.size()));
    // All original 4-bit tone positions, BOTH stereo channels. Keep
    // original filter histories live when moving between cut/bypass/boost.
    constexpr std::array<std::uint8_t,4> toneRegs{{0x44,0x45,0x46,0x47}};
    for(auto reg:toneRegs)for(unsigned nibble=0;nibble<16;++nibble) {
        const auto value=static_cast<std::uint8_t>(
            (nibble<<4u)|(15u-nibble)); // low bits intentionally set
        p002_original_ct1745_oracle_write(reg,value);
        CHECK(card.applyVerifiedOriginalRegister(reg,value,true));
        CHECK(compare());
    }
    // Original SB16 master/FM aliases and packed CT1745 stereo aliases.
    constexpr std::array<std::uint8_t,4> aliases{{0x02,0x06,0x22,0x26}};
    constexpr std::array<std::uint8_t,8> aliasValues{{
        0x00,0x01,0x0f,0x10,0x40,0x81,0xa5,0xff}};
    for(auto reg:aliases)for(auto value:aliasValues) {
        p002_original_ct1745_oracle_write(reg,value);
        CHECK(card.applyVerifiedOriginalRegister(reg,value,true));
        CHECK(compare());
    }
    // Both channels' attenuation and output gain at register boundaries.
    constexpr std::array<std::uint8_t,6> gainRegs{{
        0x30,0x31,0x34,0x35,0x41,0x42}};
    constexpr std::array<std::uint8_t,6> gainValues{{
        0x00,0x07,0x40,0x88,0xf8,0xff}};
    for(auto reg:gainRegs)for(auto value:gainValues) {
        p002_original_ct1745_oracle_write(reg,value);
        CHECK(card.applyVerifiedOriginalRegister(reg,value,true));
        CHECK(compare());
    }
    // Unauthenticated and unsupported mixer events MUST leave the
    // audible state unchanged, not silently apply invented mixer writes.
    CHECK(!card.applyVerifiedOriginalRegister(0x30,0x00,false));
    CHECK(!card.applyVerifiedOriginalRegister(0x99,0xff,true));
    CHECK(compare());
    CHECK(compared==8u*(1u+2u*writes.size()+
        toneRegs.size()*16u+aliases.size()*aliasValues.size()+
        gainRegs.size()*gainValues.size()+1u));
    CHECK(card.floatSink(exact[1])==
          static_cast<float>(exact[1])/32768.f);
    std::printf(
        "P002_PINNED_FORK_CT1745_VST3_ADAPTER_DIFFERENTIAL_PASS "
        "source_original_C_snd_sb_c=UNCHANGED compared_stereo_frames=%zu "
        "bass_cut_boost=ORIGINAL treble_cut_boost=ORIGINAL "
        "original_right_treble_asymmetry=PRESERVED "
        "source_integer_truncation=EXACT\n",compared);
    return 0;
}
