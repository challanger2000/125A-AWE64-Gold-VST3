#include "OriginalCreativeSf2MainCompiler.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>

namespace awe64::original_vxd {
namespace {
[[nodiscard]] constexpr std::uint16_t word(Descriptor const& d,
    std::size_t i) noexcept {
    return static_cast<std::uint16_t>(static_cast<unsigned>(d[2u*i])|
        (static_cast<unsigned>(d[2u*i+1u])<<8u));
}
[[nodiscard]] constexpr std::int32_t signed_word(Descriptor const& d,
    std::size_t i) noexcept {
    const auto x=word(d,i);
    return x<=0x7fffu?static_cast<std::int32_t>(x):
        static_cast<std::int32_t>(x)-65536;
}
[[nodiscard]] constexpr std::uint32_t dword(Descriptor const& d,
    std::size_t off) noexcept {
    return static_cast<std::uint32_t>(d[off])|
       (static_cast<std::uint32_t>(d[off+1u])<<8u)|
       (static_cast<std::uint32_t>(d[off+2u])<<16u)|
       (static_cast<std::uint32_t>(d[off+3u])<<24u);
}
[[nodiscard]] constexpr std::uint8_t attack_hardware(std::uint16_t v) noexcept {
    return static_cast<std::uint8_t>(v==128u?127u:(v&0xffu));
}
[[nodiscard]] constexpr std::uint16_t pack(const std::int32_t hi,
    const std::uint8_t lo) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(hi)<<8u)|lo);
}
}
bool compile_original_sf2_main(SourceVoice const& source,
    const std::uint8_t physical,const std::uint8_t key,
    OriginalVoiceStartSources const& v,
    OriginalVoiceProgram& out) noexcept {
    if(!source.source_verified || !v.original_descriptor_verified ||
       !v.original_device_state_verified ||
       !v.original_sample_placement_verified ||
       physical>=physical_voices || key>127u)return false;
    const auto& d=source.note_start_descriptor_verified
        ?source.note_start_descriptor:source.descriptor;
    // Original note-start LCOD decodes sampleType 1 (RAM/ROM normalized).
    // Type-2/4 stereo-pair worklist is a distinct earlier source stage.
    if(word(d,0x8cu/2u)!=1u || d[0x8au]>127u)return false;
    const std::uint32_t start=dword(d,0x76u),end=dword(d,0x7au);
    const std::uint32_t loop_s=dword(d,0x7eu),loop_e=dword(d,0x82u);
    const std::uint32_t mode=word(d,54u),q=word(d,9u),pan=word(d,17u);
    const std::uint32_t chorus=word(d,15u),reverb=word(d,16u),aux=word(d,18u);
    const std::uint32_t cutoff=word(d,8u),attenuation=word(d,19u);
    if(mode>3u || q>15u || pan>255u || chorus>255u ||
       reverb>255u || aux>255u || cutoff>255u ||
       attenuation>255u || word(d,26u)>128u || word(d,34u)>128u ||
       word(d,28u)>127u || word(d,29u)>127u ||
       word(d,36u)>255u || word(d,37u)>255u ||
       start<4u || start>=end)return false;
    // SBAWE.VXD LCOD+0x5B87 branches on mode bit 0 before loading
    // either loop address. +0x5BC8 uses end+4/end+8 for nonlooping
    // samples, including RAM; loop metadata cannot reject that path.
    if((mode&1u)!=0u && (loop_s<start || loop_s>=loop_e ||
       (loop_e>end && loop_e-end>2u)))return false;
    if(v.original_ram_sample) {
        if(start<0x200004u || end>0x1000000u ||
           ((mode&1u)!=0u && loop_e>0x1000000u))
            return false;
    }else if(end>0x80000u ||
             ((mode&1u)!=0u && loop_e>0x80000u))return false;
    const auto psst_low=(mode&1u)?loop_s-1u:end+4u;
    const auto csl_low=(mode&1u)?loop_e-1u:end+8u;
    if(psst_low>0xffffffu || csl_low>0xffffffu)return false;
    // Original PCOD already multiplied coarse-tune generator51 by 100.
    // Do not rescale it or approximate the x86 874-steps-per-cent pitch.
    const auto root_raw=signed_word(d,58u);
    const auto root=root_raw==-1?static_cast<std::int32_t>(d[0x8au]):root_raw;
    if(root<0 || root>127)return false;
    const std::uint32_t dw_pitch=dword(d,0x86u);
    if(dw_pitch>0xffffffu)return false;
    const std::int32_t correction=d[0x8bu]<128u?d[0x8bu]:
        static_cast<std::int32_t>(d[0x8bu])-256;
    const auto cents=static_cast<std::int64_t>(correction)+
       signed_word(d,52u)+v.original_channel_tune_1_cents+
       v.original_channel_tune_2_cents+signed_word(d,51u)+
       (static_cast<std::int64_t>(key)-root)*signed_word(d,56u);
    // LCOD+0x56CE..0x5753 uses wrapping DWORD IMUL/ADD, a SIGNED
    // upper clamp at +0x573E and SAR 8 at +0x574C. Negative values
    // are not rejected or clamped to zero. Preserve their low WORD.
    const auto wrapped_pitch=std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(static_cast<std::int64_t>(dw_pitch)+
                                   cents*874ll));
    const auto ip=static_cast<std::uint16_t>(
        std::min<std::int32_t>(wrapped_pitch,0xffffff)>>8u);
    if((word(d,34u)!=128u || word(d,33u)<0x8000u) &&
       v.original_voice_volume_target!=0u)return false;

    OriginalVoiceProgram candidate{};
    candidate.source_verified=true;
    auto& p=candidate.data;
    p[0]=0x0000ffffu;  // VTFT 32-bit stop/reset
    p[1]=0x0000ffffu;  // CVCF 32-bit stop/reset
    p[2]=pack(signed_word(d,35u),attack_hardware(word(d,34u)));
    p[3]=word(d,21u);
    p[4]=pack(signed_word(d,27u),attack_hardware(word(d,26u)));
    p[5]=pack(signed_word(d,29u),static_cast<std::uint8_t>(word(d,28u)));
    p[6]=word(d,23u);
    p[7]=ip;
    p[8]=(cutoff<<8u)|attenuation;
    p[9]=pack(signed_word(d,7u),static_cast<std::uint8_t>(word(d,11u)));
    const auto lfo1=static_cast<std::int32_t>(v.original_lfo1_pitch_device_0a)+
        static_cast<std::int32_t>(v.original_lfo1_pitch_device_0b)+
        signed_word(d,5u);
    p[10]=pack(std::min<std::int32_t>(127,lfo1),
               static_cast<std::uint8_t>(word(d,10u)));
    p[11]=pack(signed_word(d,13u),static_cast<std::uint8_t>(word(d,22u)));
    p[12]=pack(signed_word(d,6u),static_cast<std::uint8_t>(word(d,24u)));
    p[13]=(word(d,26u)==128u && word(d,25u)>=0x8000u)?0xbfffu:word(d,25u);
    p[14]=(word(d,34u)==128u && word(d,33u)>=0x8000u)?0xbfffu:word(d,33u);
    p[15]=0u; // PTRX reset
    p[16]=0u; // CPF reset
    p[17]=psst_low|(pan<<24u);
    p[18]=csl_low|(chorus<<24u);
    p[19]=start-4u;
    p[20]=0u; // Z1
    p[21]=0u; // Z2
    p[22]=(start-4u)|(q<<28u);
    const std::uint32_t filter_vol=
        (static_cast<std::uint32_t>(v.original_voice_volume_target)<<16u)|
        (cutoff<<8u);
    p[23]=filter_vol; // VTFT
    p[24]=filter_vol; // CVCF
    const auto tgt=static_cast<std::uint32_t>(v.original_pitch_target)<<16u;
    p[25]=tgt|(reverb<<8u)|aux; // PTRX pitch/reverb/aux
    p[26]=tgt; // CPF pitch
    p[27]=(static_cast<std::uint32_t>(word(d,37u))<<8u)|
          static_cast<std::uint32_t>(word(d,36u)); // DCYSUSV start/trigger
    out=candidate;
    return true;
}
} // namespace awe64::original_vxd
