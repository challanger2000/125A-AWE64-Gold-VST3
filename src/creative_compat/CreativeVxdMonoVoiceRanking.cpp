#include "CreativeVxdMonoVoiceRanking.h"
#include <cstdint>
#include <limits>
namespace awe64::creative {
std::optional<OriginalWin95MonoSelection> candidateOriginalWin95Mode0MonoVoice(
    const std::array<OriginalWin95MonoVoiceSnapshot,kOriginalCreativeMusicVoices>& slots,
    const std::size_t usable,const std::uint32_t original_mode_count) noexcept {
    if(usable==0 || usable>kOriginalCreativeMusicVoices)return std::nullopt;
    // The original routine's score depends on live EMU8000 register reads.
    // Never treat a missing read, even from a later lane, as a free voice.
    for(std::size_t voice=0;voice<usable;++voice)
        if(!slots[voice].complete)return std::nullopt;
    std::uint32_t bestScore=std::numeric_limits<std::uint32_t>::max();
    std::optional<OriginalWin95MonoSelection> best;
    // The original LCOD loops through physical voices in groups of three.
    for(std::size_t lane=0;lane<3;++lane) {
        for(std::size_t voice=lane;voice<usable;voice+=3) {
            const auto& v=slots[voice];
            if(original_mode_count<=2 && v.blocked_when_few_voices)continue;
            const std::uint32_t marker=v.internal_key_marker;
            if(marker<0x2000u) {
                std::uint32_t score=v.volume_target_hi16;
                if(marker==0x0ffe && score==0) {
                    return OriginalWin95MonoSelection{
                        static_cast<std::uint8_t>(voice),
                        OriginalWin95MonoSelectionReason::CompletelyIdle,0};
                }
                if(marker!=0x0ffe) {
                    score+=(marker&0x1000u)?0x1200u:0x200u;
                    if((marker&0xffu)==0xffu)score+=0x300u;
                    else if((v.envelope_status_reg&0x8000u)==0)score+=0x1300u;
                }
                // The original VXD selects later voices on a score tie.
                if(score<=bestScore) {
                    bestScore=score;
                    best=OriginalWin95MonoSelection{
                        static_cast<std::uint8_t>(voice),
                        OriginalWin95MonoSelectionReason::LowestEnvelopePriority,
                        score};
                }
            }
            // The original VXD checks sample completion even if the
            // internal key marker was ineligible for the priority branch.
            if(v.stored_sample_end!=0 &&
               v.stored_sample_end <= (v.chip_current_address&0x00ffffffu)) {
                return OriginalWin95MonoSelection{
                    static_cast<std::uint8_t>(voice),
                    OriginalWin95MonoSelectionReason::SampleEndReached,0};
            }
        }
    }
    return best;
}
std::optional<std::array<OriginalWin95VxdPortWrite,2>>
candidateOriginalWin95AssignedVoiceVtftWrites(
    const std::uint8_t voice) noexcept {
    if(voice>=kOriginalCreativeMusicVoices)return std::nullopt;
    // VXD +0x3DDB..+0x3E07: selector (3<<5)|voice, data DWORD 0xFFFF.
    // Its 0x3D1B writer uses OUT DX,AL for the E22 selector and
    // OUT DX,EAX for the DATA0 0x620 port. The pinned original 86Box
    // fork independently identifies DATA0 index 3 as EMU8000 VTFT.
    return std::array<OriginalWin95VxdPortWrite,2>{{
        {0xE22u,static_cast<std::uint32_t>((3u<<5)|voice),8},
        {0x620u,0x0000ffffu,32}
    }};
}
std::optional<std::array<OriginalWin95VxdPortWrite,2>>
candidateOriginalWin95SelectedVoiceDcysusvWrites(
    const OriginalWin95MonoSelection& selected) noexcept {
    if(selected.physical_voice>=kOriginalCreativeMusicVoices ||
       (selected.reason!=OriginalWin95MonoSelectionReason::CompletelyIdle &&
        selected.reason!=OriginalWin95MonoSelectionReason::SampleEndReached &&
        selected.reason!=OriginalWin95MonoSelectionReason::LowestEnvelopePriority))
       return std::nullopt;
    return std::array<OriginalWin95VxdPortWrite,2>{{
        {0xE22u,static_cast<std::uint16_t>((5u<<5)|selected.physical_voice),8},
        {0xA20u,0x00FFu,16}
    }};
}
} // namespace awe64::creative
