#pragma once
// Original Creative AWE32/64 Win95 WinMM NRPN according to the 1996
// Developer Information Pack and Sound Blaster AWE32 FAQ, section H.
// This is a controller translator ONLY; no replacement for EMU8000 DSP.
#include <array>
#include <algorithm>
#include <cstdint>

namespace p007::creative_nrpn {
inline constexpr unsigned count=12u;
// P165 physical knob order: Amp ADSR, Filter Env ADSR, Cutoff,
// Resonance, Reverb Send, Chorus Send.
inline constexpr std::array<std::uint8_t,count> lsb{
    11,13,14,15, 5,7,8,9, 21,22, 26,25
};
// Range of documented original Creative "Actual Value" offsets.
inline constexpr std::array<std::uint16_t,count> maximum{
    5940,5940,127,5940, 5940,5940,127,5940, 127,127, 255,255
};
struct Control { std::uint8_t number=0u,value=0u; };
inline constexpr unsigned kToneCount=10u;
inline constexpr unsigned kEffectFirst=10u;
// P165 product contract:
// - AMP/Filter 0% is neutral: keep the underlying original SoundFont
//   generator value. A sent NRPN value of zero is NOT neutral for these
//   parameters (e.g. cutoff zero = ~100 Hz and attack zero = fast).
// - Reverb/Chorus 0% is explicitly an original Creative send of zero (dry).
inline constexpr bool shouldTransmit(unsigned index,std::uint16_t value) noexcept {
    return index<count && (index>=kEffectFirst || value!=0u);
}
inline constexpr bool needsSoundFontReset(unsigned index,
                                std::uint16_t previous,
                                std::uint16_t next) noexcept {
    return index<kToneCount && previous!=0u && next==0u;
}

inline std::uint16_t raw(unsigned index,std::uint16_t basisPoints) noexcept {
    if(index>=count)return 0u;
    return static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(std::min<std::uint16_t>(10000u,basisPoints))
         * maximum[index]+5000u)/10000u);
}
inline std::array<Control,4> encode(unsigned index,std::uint16_t basisPoints) noexcept {
    if(index>=count)return {};
    // Documented Creative offset encoding: value + 8192 for signed 14-bit
    // NRPN: CC99=127, CC98=generator, CC6=high, CC38=low.
    const auto data=static_cast<std::uint16_t>(8192u+raw(index,basisPoints));
    return {{{99u,127u},{98u,lsb[index]},
             {6u,static_cast<std::uint8_t>(data>>7u)},
             {38u,static_cast<std::uint8_t>(data&127u)}}};
}
template<class Sender>
bool submit(unsigned index,std::uint16_t basisPoints,Sender&& sender) noexcept {
    if(index>=count)return false;
    const auto commands=encode(index,basisPoints);
    // Stop at the first rejected packet and retry all four in the next
    // audio callback. NEVER mark a partial CC sequence as delivered.
    for(const auto& c:commands)
        if(!sender(c.number,c.value))return false;
    return true;
}
} // namespace p007::creative_nrpn
