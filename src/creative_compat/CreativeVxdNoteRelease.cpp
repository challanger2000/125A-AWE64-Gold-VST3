#include "CreativeVxdNoteRelease.h"
#include <cstdint>
namespace awe64::creative {
std::optional<OriginalVxdNoteReleaseCommands>
candidateOriginalVxdMidiNoteRelease(
    const std::uint8_t voice,
    const OriginalVxdNoteReleaseEvidence& e) noexcept {
    if(voice>=kOriginalCreativeMusicVoices ||
       !e.original_key_channel_match || !e.original_voice_record_verified)
        return std::nullopt;
    OriginalVxdNoteReleaseCommands result{};
    if(e.original_sustain_held) {
        // Exact original LCOD +0x60B4..+0x60C8: saved note identity's
        // low byte becomes 0xFF, but no voice envelope HW writes yet.
        result.deferred_by_sustain=true;
        return result;
    }
    if(!e.current_chip_registers_verified ||
       e.stored_end_word>0xfffffbu) return std::nullopt;
    // LCOD 60E4: volume envelope release and modulation envelope release.
    // The real E22 selector is an 8-bit OUT; A20 is a 16-bit OUT.
    result.writes[result.count++]={0xE22u,
        static_cast<std::uint32_t>((5u<<5)|voice),8};
    result.writes[result.count++]={0xA20u,
        static_cast<std::uint32_t>(0x8000u|e.encoded_volume_release),16};
    result.writes[result.count++]={0xE22u,
        static_cast<std::uint32_t>((7u<<5)|voice),8};
    result.writes[result.count++]={0xA20u,
        static_cast<std::uint32_t>(0x8000u|e.encoded_mod_release),16};
    if(e.stored_end_word!=0) {
        // LCOD 613B..6171: read current hardware CSL then PSST,
        // keep their upper byte and rewrite their low 24-bit offsets
        // in EXACT order: CSL first and PSST second.
        const auto end=e.stored_end_word;
        const auto csl=(e.observed_chip_csl&0xff000000u)|
                       ((end+4u)&0x00ffffffu);
        const auto psst=(e.observed_chip_psst&0xff000000u)|
                        (end&0x00ffffffu);
        result.writes[result.count++]={0xE22u,
            static_cast<std::uint32_t>((7u<<5)|voice),8};
        result.writes[result.count++]={0x620u,csl,32};
        result.writes[result.count++]={0xE22u,
            static_cast<std::uint32_t>((6u<<5)|voice),8};
        result.writes[result.count++]={0x620u,psst,32};
    }
    result.original_voice_marker_idle=true; // VXD final +0x617C
    return result;
}
} // namespace awe64::creative
