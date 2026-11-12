#include "CreativeVxdSampleAddress.h"
namespace awe64::creative {
std::optional<std::array<OriginalWin95VxdPortWrite,4>>
candidateOriginalVxdSampleAddressUpdate(
    const SbkRomAddress& s, const std::uint8_t voice,
    const OriginalVxdSampleAddressCase mode, const std::uint32_t old_psst) noexcept {
    // The owner's real Creative 1MGM ROM is 1MiB or 0x80000 words.
    // No fake ROM pointer, non-looping flag, or P001 DSP side effect.
    if(voice>=kOriginalCreativeMusicVoices || s.start_word>=s.end_word ||
       s.end_word>0x80000u ||
       s.loop_start_word<s.start_word ||
       s.loop_start_word>=s.loop_end_word ||
       s.loop_end_word>s.end_word)return std::nullopt;
    std::uint32_t csl=0,psst_low24=0;
    if(mode==OriginalVxdSampleAddressCase::DriverCase5){
        // Real VXD LCOD +0x1B59..+0x1B94: sample +0x0C,
        // add 1; CS(L) index7. PSST retains old upper 8 bits and
        // takes sample +0x08 (loop-start).
        csl=(s.loop_end_word+1u)&0x00ffffffu;
        psst_low24=s.loop_start_word&0x00ffffffu;
    } else if(mode==OriginalVxdSampleAddressCase::DriverCase6) {
        // Real VXD LCOD +0x1C25..+0x1C6B: sample +0x04
        // (sample end), +8 for CSL and +4 for PSST.
        csl=(s.end_word+8u)&0x00ffffffu;
        psst_low24=(s.end_word+4u)&0x00ffffffu;
    } else return std::nullopt;
    return std::array<OriginalWin95VxdPortWrite,4>{{
        {0xE22u,static_cast<std::uint32_t>((7u<<5)|voice),8u},
        {0x620u,csl,32u},
        {0xE22u,static_cast<std::uint32_t>((6u<<5)|voice),8u},
        {0x620u,(old_psst&0xff000000u)|psst_low24,32u}
    }};
}
} // namespace awe64::creative
