#include "CreativeVxdVoiceTermination.h"
namespace awe64::creative {
std::optional<OriginalVxdGroupChokeWrites>
candidateOriginalWin95VxdGroupChokeWrites(
    const std::uint8_t first, const OriginalVxdGroupChokeEvidence e) noexcept {
    if(first>=kOriginalCreativeMusicVoices || !e.matched_original_group)
        return std::nullopt;
    const bool paired=e.lead_voice_stereo_flag && e.following_voice_paired_flag;
    if(paired && first+1>=kOriginalCreativeMusicVoices)return std::nullopt;
    OriginalVxdGroupChokeWrites result{};
    const auto append=[&](std::uint8_t voice) noexcept {
        // Original VXD 16-bit writer +0x3CB1: byte pointer select at
        // base+2 (E22); word DATA3 at E20 and DATA1 at A20.
        result.writes[result.count++] = {
            0xE22u, static_cast<std::uint32_t>((1u<<5)|voice),8};
        result.writes[result.count++] = {0xE20u,0x00ffu,16};
        result.writes[result.count++] = {
            0xE22u,static_cast<std::uint32_t>((5u<<5)|voice),8};
        result.writes[result.count++] = {0xA20u,0x807fu,16};
    };
    append(first);
    if(paired)append(static_cast<std::uint8_t>(first+1u));
    return result;
}
} // namespace awe64::creative
