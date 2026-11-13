#include "OriginalCt1745WavetableMixer.h"
#include <algorithm>
#include <limits>

namespace awe64::card {
namespace {
// COPIED BYTE-FOR-BYTE AS DOUBLE LITERALS from pinned original
// snd_sb.c blob 6d9a2da64e696237e0f3e4f210d75bc6e8ea0a63.
// No computed substitute attenuation or pan curve.
constexpr std::array<double,32> sb_att_2dbstep_5bits{{
     25.0,    32.0,    41.0,    51.0,    65.0,    82.0,   103.0,   130.0,
    164.0,   206.0,   260.0,   327.0,   412.0,   519.0,   653.0,   822.0,
   1036.0,  1304.0,  1641.0,  2067.0,  2602.0,  3276.0,  4125.0,  5192.0,
   6537.0,  8230.0, 10362.0, 13044.0, 16422.0, 20674.0, 26027.0, 32767.0
}};
constexpr std::array<double,16> sb_bass_treble_4bits{{
    0.199526231,0.25,0.316227766,0.398107170,0.5,0.63095734,0.794328234,1.0,
    0.0,0.25892541,0.584893192,1.0,1.511886431,2.16227766,3.0,4.011872336
}};
// Four unmodified filter *coefficient sets* from pinned
// src/include/86box/filters.h blob 572137ef02ef1c708e1b6def4487bb53f8230dcf.
// The recurrence and evaluation order below are the original 86Box
// low_iir, low_cut_iir, high_iir and high_cut_iir equations.
// Per-instance state avoids the original program's process-wide
// static x[5][2][3]/y[5][2][3] leaking between VST3 instances.
constexpr std::array<double,3> kLowA{{
    0.00049713569693400649,0.00099427139386801299,0.00049713569693400649
}};
constexpr std::array<double,3> kLowB{{
    1.00000000000000000000,-1.93522955470669530000,0.93726236021404663000
}};
constexpr std::array<double,3> kLowCutA{{
    0.96839970114733542000,-1.93679940229467080000,0.96839970114733542000
}};
constexpr std::array<double,3> kLowCutB{{
    1.00000000000000000000,-1.93522955471202770000,0.93726236021916731000
}};
constexpr std::array<double,3> kHighA{{
    0.72248704753064896000,-1.44497409506129790000,0.72248704753064896000
}};
constexpr std::array<double,3> kHighB{{
    1.00000000000000000000,-1.36640781670578510000,0.52352474706139873000
}};
constexpr std::array<double,3> kHighCutA{{
    0.03927726802250377400,0.07855453604500754700,0.03927726802250377400
}};
constexpr std::array<double,3> kHighCutB{{
    1.00000000000000000000,-1.36640781666419950000,0.52352474703279628000
}};
} // namespace

double OriginalCt1745WavetableMixer::OriginalForkBiquad::apply(
    const unsigned channel,const double sample,
    const std::array<double,3>& a,const std::array<double,3>& b) noexcept {
    // EXACT original filters.h recurrence: shifted in this order,
    // 3 sequential double-precision operations; no new IIR design.
    for (int n=2;n>0;--n) {
        x[channel][n]=x[channel][n-1];
        y[channel][n]=y[channel][n-1];
    }
    x[channel][0]=sample;
    y[channel][0]=a[0]*x[channel][0];
    for(int n=1;n<=2;++n)
        y[channel][0]+=a[n]*x[channel][n]-b[n]*y[channel][n];
    return y[channel][0];
}

void OriginalCt1745WavetableMixer::reset() noexcept {
    registers_.fill(0u);
    // sb_ct1745_mixer_write(index=0) exactly resets the wavetable
    // relevant card registers. Unrelated DSP/FM/CD ports are omitted.
    registers_[0x30]=registers_[0x31]=0xf8u;
    registers_[0x34]=registers_[0x35]=0xf8u;
    registers_[0x41]=registers_[0x42]=0x00u;
    registers_[0x44]=registers_[0x45]=0x80u;
    registers_[0x46]=registers_[0x47]=0x80u;
    low_={};lowCut_={};high_={};highCut_={};
    decode();
}

void OriginalCt1745WavetableMixer::decode() noexcept {
    // Directly transcribed from source sb_ct1745_mixer_write, not
    // approximated decibels. DSP and global card mixer paths omitted.
    for(unsigned ch=0;ch<2u;++ch) {
        master_[ch]=sb_att_2dbstep_5bits[registers_[0x30u+ch]>>3u]/32768.0;
        fm_[ch]=sb_att_2dbstep_5bits[registers_[0x34u+ch]>>3u]/32768.0;
        outputGain_[ch]=static_cast<double>(
            1u<<(registers_[0x41u+ch]>>6u));
        bass_[ch]=registers_[0x46u+ch]>>4u;
        treble_[ch]=registers_[0x44u+ch]>>4u;
    }
}

bool OriginalCt1745WavetableMixer::applyVerifiedOriginalRegister(
    const std::uint8_t reg,const std::uint8_t value,
    const bool sourceVerified) noexcept {
    if(!sourceVerified)return false;
    switch(reg) {
    case 0x30:case 0x31:case 0x34:case 0x35:
    case 0x41:case 0x42:case 0x44:case 0x45:
    case 0x46:case 0x47:
        registers_[reg]=value;
        break;
    // Original SB16 compatibility alias decoded by
    // sb_ct1745_mixer_write() without rounding or new DSP.
    case 0x02:
        registers_[0x30]=registers_[0x31]=
            static_cast<std::uint8_t>(((value&15u)<<4u)|8u);
        break;
    case 0x06:
        registers_[0x34]=registers_[0x35]=
            static_cast<std::uint8_t>(((value&15u)<<4u)|8u);
        break;
    case 0x22:
        registers_[0x30]=static_cast<std::uint8_t>((value&0xf0u)|8u);
        registers_[0x31]=static_cast<std::uint8_t>(((value&15u)<<4u)|8u);
        break;
    case 0x26:
        registers_[0x34]=static_cast<std::uint8_t>((value&0xf0u)|8u);
        registers_[0x35]=static_cast<std::uint8_t>(((value&15u)<<4u)|8u);
        break;
    default:
        return false;
    }
    decode();
    return true;
}

OriginalGoldFrame OriginalCt1745WavetableMixer::process(
    const std::int32_t chipLeft,const std::int32_t chipRight) noexcept {
    // Exact original snd_sb.c: sb_get_wavetable_buffer_sb16_awe32.
    double out_l=0.0;
    double out_r=0.0;
    out_l+=(static_cast<double>(chipLeft)*fm_[0]);
    out_r+=(static_cast<double>(chipRight)*fm_[1]);
    out_l*=master_[0];
    out_r*=master_[1];
    double bass_treble;
    if(bass_[0]!=8u) {
        bass_treble=sb_bass_treble_4bits[bass_[0]];
        if(bass_[0]>8u)
            out_l+=(low_.apply(0u,out_l,kLowA,kLowB)*bass_treble);
        else
            out_l=(out_l*bass_treble+
                   lowCut_.apply(0u,out_l,kLowCutA,kLowCutB)*(1.0-bass_treble));
    }
    if(bass_[1]!=8u) {
        bass_treble=sb_bass_treble_4bits[bass_[1]];
        if(bass_[1]>8u)
            out_r+=(low_.apply(1u,out_r,kLowA,kLowB)*bass_treble);
        else
            out_r=(out_r*bass_treble+
                   lowCut_.apply(1u,out_r,kLowCutA,kLowCutB)*(1.0-bass_treble));
    }
    if(treble_[0]!=8u) {
        bass_treble=sb_bass_treble_4bits[treble_[0]];
        if(treble_[0]>8u)
            out_l+=(high_.apply(0u,out_l,kHighA,kHighB)*bass_treble);
        else
            out_l=(out_l*bass_treble+
                   highCut_.apply(0u,out_l,kHighCutA,kHighCutB)*(1.0-bass_treble));
    }
    if(treble_[1]!=8u) {
        bass_treble=sb_bass_treble_4bits[treble_[1]];
        if(treble_[1]>8u)
            out_r+=(high_.apply(1u,out_r,kHighA,kHighB)*bass_treble);
        else
            // PINNED FORK SPECIAL CASE: the ORIGINAL snd_sb.c uses
            // out_l, not out_r, in the right treble-cut dry term.
            // Intentionally preserve this for exact fork parity.
            out_r=(out_l*bass_treble+
                   highCut_.apply(1u,out_r,kHighCutA,kHighCutB)*(1.0-bass_treble));
    }
    // Original sb_get_wavetable_buffer uses int32 sum, no clamp.
    // Source wavetable sink starts from a ZERO accumulator: just cast.
    return {static_cast<std::int32_t>(out_l*outputGain_[0]),
            static_cast<std::int32_t>(out_r*outputGain_[1])};
}

float OriginalCt1745WavetableMixer::preCardPcm16Tap(
    const std::int32_t sample) noexcept {
    return static_cast<float>(
        std::clamp(sample,std::int32_t(-32768),std::int32_t(32767)))/32768.0f;
}
} // namespace awe64::card
