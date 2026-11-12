#include "CreativeSf2OriginalTargets.h"
#include <array>
#include <cstdint>

namespace awe64::creative {
namespace {
std::uint16_t word(const CreativeSf2VoiceDescriptorBytes& b,
                   const unsigned id) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(b[2u*id]) |
        (static_cast<unsigned>(b[2u*id+1u])<<8u));
}
std::int32_t signedWord(const CreativeSf2VoiceDescriptorBytes& b,
                        const unsigned id) noexcept {
    const auto w=word(b,id);
    return w<=0x7fffu?static_cast<std::int32_t>(w):
                      static_cast<std::int32_t>(w)-65536;
}
// x86 CMP unsigned >0xffff from actual LCOD+5B63 and +5D8C;
// negative wrapped DWORDs saturate to 0xffff, NOT to 0.
std::uint32_t capU16(const std::uint32_t v) noexcept {
    return v>0xffffu?0xffffu:v;
}
}
std::optional<OriginalSf2VerifiedFinalTargetState>
candidateOriginalSf2ComputeFinalTargetsFromVxdState(
    const CreativeSf2VoiceDescriptorBytes& note,
    const OriginalSf2VxdFinalTargetInputs& source) noexcept {
    if(!source.source_original_device_voice_state_verified ||
       !source.original_lcod_2fa0_16word_table_verified ||
       word(note,0x8cu/2u)!=0x0001u ||
       word(note,34u)>128u)
        return std::nullopt;

    // Actual LCOD+0x5B50..0x5B7E: original per-channel/physical-voice
    // note tuning starts with 32-bit wrapping addition, *unsigned* cap.
    const auto base=capU16(
        static_cast<std::uint32_t>(
            static_cast<std::int32_t>(source.channel_word_plus_0e))+
        static_cast<std::uint32_t>(source.allocated_voice_word_plus_0e)+
        source.channel_dword_plus_14);

    // Actual LCOD+0x5D7B..0x5D95: signed generator 7 *16 is ADDED to
    // source base, then a second unsigned cap (also x86 wrap semantics).
    const auto signed_delta=signedWord(note,7u)*16;
    const std::uint32_t normalized=capU16(
        base+static_cast<std::uint32_t>(signed_delta));

    // Actual LCOD+0x5DE5..+0x5E5A: integer-only 2^(n>>12), three
    // successive source 10000-divisor corrections controlled by bits
    // 11,10,9, then +25% and unsigned 16-bit cap. The source 32-bit
    // multiplication operands stay below signed overflow for normalized
    // <=0xffff. No log2, pow, float or DSP patch.
    std::uint32_t value=1u<<(normalized>>12u);
    if((normalized&0x0800u)!=0u)value+=(value*0x102eu)/10000u;
    if((normalized&0x0400u)!=0u)value+=(value*0x0764u)/10000u;
    if((normalized&0x0200u)!=0u)value+=(value*0x0389u)/10000u;
    value+=value>>2u;
    const auto pitch=static_cast<std::uint16_t>(capU16(value));

    // Actual LCOD+0x5E5D..+0x5ECE: special amplitude attack128:
    // signed gen19 -> x86 absolute low nibble as lookup index;
    // x86 signed /16 truncated toward zero becomes SHR WORD shift
    // count. Original table is caller-owned, SHA-pinned LCOD+0x2FA0.
    std::uint16_t volume=0u;
    if(word(note,34u)==128u && word(note,33u)>=0x8000u) {
        const auto attenuation=signedWord(note,19u);
        const auto absolute=attenuation<0?-attenuation:attenuation;
        const auto index=static_cast<unsigned>(absolute)&15u;
        const auto shift=static_cast<unsigned>(
            static_cast<std::uint8_t>(attenuation/16))&31u;
        volume=shift>=16u?0u:static_cast<std::uint16_t>(
            source.source_lcod_2fa0_table[index]>>shift);
    }
    return OriginalSf2VerifiedFinalTargetState{true,volume,pitch};
}
} // namespace awe64::creative
