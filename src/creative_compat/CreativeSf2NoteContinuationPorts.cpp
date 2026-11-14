#include "CreativeSf2NoteContinuationPorts.h"
#include <array>
#include <cstdint>
namespace awe64::creative {
namespace {
std::uint16_t raw(const CreativeSf2VoiceDescriptorBytes& b,
                  const unsigned gen) noexcept {
    return static_cast<std::uint16_t>(static_cast<unsigned>(b[gen*2u])|
       (static_cast<unsigned>(b[gen*2u+1u])<<8));
}
std::int32_t signedWord(const CreativeSf2VoiceDescriptorBytes& b,
                        const unsigned gen) noexcept {
    const auto n=raw(b,gen);
    return n<=0x7fffu?static_cast<std::int32_t>(n):
                      static_cast<std::int32_t>(n)-65536;
}
std::uint16_t bytePair(const std::int32_t hi,
                       const std::uint8_t lo) noexcept {
    // Original x86 uses SHL EAX,8; OR (unsigned BYTE), and writes AX.
    // uint32 arithmetic avoids UB on a negative signed high word.
    return static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(hi)<<8u)|lo);
}
}
std::optional<
    std::array<OriginalWin95VxdPortWrite,kOriginalSf2AfterPitchPortCount>>
candidateOriginalSf2AfterPitchRegisters(
    const CreativeSf2VoiceDescriptorBytes& note,
    const std::uint8_t voice,
    const OriginalSf2VerifiedLfoChannelAmounts& state) noexcept {
    if(!state.source_original_state_verified ||
       voice>=kOriginalCreativeMusicVoices ||
       raw(note,0x8cu/2u)!=0x0001u ||
       raw(note,26u)>128u || raw(note,34u)>128u)
        return std::nullopt;
    const auto ptr=[voice](const unsigned reg) noexcept {
        return OriginalWin95VxdPortWrite{
            0xe22u,static_cast<std::uint32_t>((reg<<5u)|voice),8u};
    };
    const auto w16=[](const std::uint16_t port,const std::uint16_t value) noexcept {
        return OriginalWin95VxdPortWrite{port,value,16u};
    };
    const auto w32=[](const std::uint32_t value) noexcept {
        return OriginalWin95VxdPortWrite{0x620u,value,32u};
    };
    // Original LCOD+0x5CCF: PEFE high gen7, low BYTE gen11.
    const auto pefe=bytePair(signedWord(note,7u),
                             static_cast<std::uint8_t>(raw(note,11u)));
    // LCOD+0x5CEB: original channel bytes at VXD state+0x0A,+0x0B,
    // plus signed generator5, HIGH word saturating only >127. Unlike
    // a generic MIDI-LFO conversion, the two private device inputs are
    // required explicitly; never silently assume them to be zero.
    auto lfo1_pitch=static_cast<std::int32_t>(state.lfo1_pitch_device_0a)+
        static_cast<std::int32_t>(state.lfo1_pitch_device_0b)+
        signedWord(note,5u);
    if(lfo1_pitch>127)lfo1_pitch=127;
    const auto fmmod=bytePair(lfo1_pitch,
                              static_cast<std::uint8_t>(raw(note,10u)));
    // LCOD+0x5D20/0x5D3C, high signed generator, low BYTE generator.
    const auto tremfrq=bytePair(signedWord(note,13u),
                               static_cast<std::uint8_t>(raw(note,22u)));
    const auto fm2frq2=bytePair(signedWord(note,6u),
                               static_cast<std::uint8_t>(raw(note,24u)));
    // LCOD+0x5D58, special mod ATTACK128 AND delay >=0x8000:
    // source explicitly writes 0xBFFF to DATA1 index6 ENVVAL.
    // Otherwise source writes the encoded delay g25 unchanged.
    const auto envval=(raw(note,26u)==128u && raw(note,25u)>=0x8000u)
        ?static_cast<std::uint16_t>(0xbfffu):raw(note,25u);
    // LCOD+0x5E5D, independently the identical condition for volume
    // ATTACK128/delay writes 0xBFFF to DATA1 index4 ENVVOL.
    const auto envvol=(raw(note,34u)==128u && raw(note,33u)>=0x8000u)
        ?static_cast<std::uint16_t>(0xbfffu):raw(note,33u);
    return std::array<OriginalWin95VxdPortWrite,
                      kOriginalSf2AfterPitchPortCount>{{
        ptr(2u),w16(0xe20u,pefe),      // DATA3 PEFE
        ptr(3u),w16(0xe20u,fmmod),     // DATA3 FMMOD
        ptr(4u),w16(0xe20u,tremfrq),   // DATA3 TREMFRQ
        ptr(5u),w16(0xe20u,fm2frq2),  // DATA3 FM2FRQ2
        ptr(6u),w16(0xa20u,envval),    // DATA1 ENVVAL
        ptr(4u),w16(0xa20u,envvol),    // DATA1 ENVVOL
        ptr(1u),w32(0u),              // DATA0 PTRX source reset
        ptr(0u),w32(0u)               // DATA0 CPF source reset
    }};
}
} // namespace awe64::creative
