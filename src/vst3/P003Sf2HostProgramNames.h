#pragma once
// Real SF2 PHDR names for the standard VST3 host program list.
// The original 20-byte name, MIDI program and bank are NEVER invented.
// The display prefix makes identical names in different banks unambiguous.
// No heap, DSP, file I/O, clock, or Creative-driver dependencies.
#include "CreativeSf2PresetInventory.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace awe64::vst3::sf2host {
inline constexpr std::size_t kLabelCharacters=128u;
inline constexpr std::uint32_t kProgramListId=0x125A0033u;
using Label=std::array<char16_t,kLabelCharacters>;

[[nodiscard]] constexpr Label labelFor(
    const awe64::creative::Sf2PresetIdentity& preset) noexcept {
    Label out{};
    std::size_t at=0u;
    const auto write=[&](char16_t c) {
        if(at+1u<out.size())out[at++]=c;
    };
    const auto decimal=[&](unsigned value,unsigned digits) {
        char16_t digitsBuf[5]{};
        for(unsigned i=0u;i<digits;++i) {
            digitsBuf[digits-i-1u]=static_cast<char16_t>(
                u'0'+static_cast<char16_t>(value%10u));
            value/=10u;
        }
        for(unsigned i=0u;i<digits;++i)write(digitsBuf[i]);
    };
    write(u'B');decimal(preset.bank,5u);
    write(u' ');write(u'P');decimal(preset.program,3u);
    write(u':');write(u' ');
    // SoundFont v2 PHDR uses at most 20 bytes, not 128 MIDI slots.
    // Widen Latin-1 literally; never silently overwrite the source.
    for(std::size_t i=0u;i<20u;++i) {
        const auto byte=static_cast<unsigned char>(preset.name[i]);
        if(byte==0u)break;
        // Prevent control-character injection into a host popup.
        write(byte<32u || byte==127u?u'_':
              static_cast<char16_t>(byte));
    }
    return out;
}
} // namespace awe64::vst3::sf2host
