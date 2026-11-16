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

std::optional<std::array<OriginalWin95VxdPortWrite,2>>
candidateOriginalVxdRomCccaStart(
    const SbkRomAddress& s,const std::uint8_t voice,
    const std::uint8_t q,const bool verified,
    const OriginalVxdCccaStartPhase phase) noexcept {
    if(voice>=kOriginalCreativeMusicVoices || !verified || q>15 ||
       s.start_word<4 || s.start_word>=s.end_word ||
       s.end_word>0x80000u ||
       s.loop_start_word<s.start_word ||
       s.loop_start_word>=s.loop_end_word ||
       s.loop_end_word>s.end_word)return std::nullopt;
    const auto start=s.start_word-4u;
    std::uint32_t value=start;
    if(phase==OriginalVxdCccaStartPhase::StartWithEncodedFilterQ)
        value|=static_cast<std::uint32_t>(q)<<28;
    else if(phase!=OriginalVxdCccaStartPhase::StartWithoutFilterQ)
        return std::nullopt;
    return std::array<OriginalWin95VxdPortWrite,2>{{
        {0xE22u,static_cast<std::uint32_t>(voice),8u},
        {0xA20u,value,32u}
    }};
}

std::optional<OriginalVxdRomNoteStartGeometry>
candidateOriginalVxdInitialRomNoteGeometry(
    const SbkRomAddress& s, const std::uint8_t voice,
    const OriginalVxdRomNoteStartMode mode) noexcept {
    if(voice>=kOriginalCreativeMusicVoices ||
       !mode.original_driver_mode_verified ||
       (mode.encoded_mode_flags&0xfcu)!=0 ||
       s.start_word<4 || s.start_word>=s.end_word ||
       s.end_word>0x80000u ||
       s.loop_start_word<s.start_word ||
       s.loop_start_word>=s.loop_end_word ||
       s.loop_end_word>s.end_word)return std::nullopt;
    const bool looped=(mode.encoded_mode_flags&1u)!=0;
    const std::uint32_t psstLow=looped?s.loop_start_word-1u:s.end_word+4u;
    const std::uint32_t cslLow=looped?s.loop_end_word-1u:s.end_word+8u;
    // All actual owner ROM addresses are well under the 24-bit chip limit.
    if(psstLow>0xffffffu || cslLow>0xffffffu)return std::nullopt;
    OriginalVxdRomNoteStartGeometry result{};
    result.driver_retained_release_end=
        looped&&(mode.encoded_mode_flags&2u)?s.end_word+4u:0u;
    result.loop_address_ports={{
        {0xE22u,static_cast<std::uint32_t>((6u<<5)|voice),8},
        {0x620u,psstLow|(static_cast<std::uint32_t>(
                    mode.original_pan_byte)<<24),32},
        {0xE22u,static_cast<std::uint32_t>((7u<<5)|voice),8},
        {0x620u,cslLow|(static_cast<std::uint32_t>(
                    mode.original_chorus_send_byte)<<24),32}
    }};
    return result;
}

std::optional<OriginalVxdRamNoteStartGeometry>
candidateOriginalVxdInitialVerifiedRamNoteGeometry(
    const std::uint32_t start,
    const std::uint32_t end,
    const std::uint32_t loopStart,
    const std::uint32_t loopEnd,
    const std::uint8_t voice,
    const std::uint8_t encodedQ,
    const std::uint8_t pan,
    const std::uint8_t chorus,
    const bool originalRamPlacementVerified,
    const bool originalVxdModeVerified) noexcept {
    // 28 MiB EMU8000 RAM occupies physical 16-bit WORD addresses
    // [0x200000, 0x1000000). Never admit a ROM pointer, 30/31 reserved
    // voices, a wrapped -4/-1 offset, or an unverified source.
    constexpr std::uint32_t firstRam=0x200000u;
    constexpr std::uint32_t endRam=0x1000000u;
    if(!originalRamPlacementVerified || !originalVxdModeVerified ||
       voice>=kOriginalCreativeMusicVoices || encodedQ>15u ||
       start<firstRam+4u || start>=end || end>endRam ||
       loopStart<start || loopStart>=loopEnd || loopEnd>end ||
       loopStart<firstRam+1u)
        return std::nullopt;
    const auto ccca=start-4u;
    const auto psst=(loopStart-1u) |
        (static_cast<std::uint32_t>(pan)<<24u);
    const auto csl=(loopEnd-1u) |
        (static_cast<std::uint32_t>(chorus)<<24u);
    const auto filtered=ccca |
        (static_cast<std::uint32_t>(encodedQ)<<28u);
    // Only original VXD port selectors / values; execution order
    // between these groups and the other 61 source NoteOn writes is
    // NOT invented by this independently testable address primitive.
    OriginalVxdRamNoteStartGeometry result{};
    result.plain_ccca={{{0xE22u,voice,8u},
                        {0xA20u,ccca,32u}}};
    result.filtered_ccca={{{0xE22u,voice,8u},
                           {0xA20u,filtered,32u}}};
    result.loop_address_ports={{{0xE22u,
                                 static_cast<std::uint32_t>((6u<<5u)|voice),8u},
                                {0x620u,psst,32u},
                                {0xE22u,
                                 static_cast<std::uint32_t>((7u<<5u)|voice),8u},
                                {0x620u,csl,32u}}};
    return result;
}

} // namespace awe64::creative
