#include "CreativeVxdAdditionalAllocatorModes.h"
#include <algorithm>
#include <cstdint>
#include <limits>

namespace awe64::creative {
namespace {
constexpr std::uint32_t kOriginalMaxPairPriority=0x000fffffu;
constexpr std::uint32_t kPairStatePenalty=0x00000c00u;

bool capacityValid(const std::uint16_t capacity) noexcept {
    return capacity>=1u && capacity<=kOriginalWin95PhysicalVoiceCapacity;
}
bool forbiddenByDeviceMode(
    const OriginalWin95AllocatorVoiceSnapshot& v,
    const std::uint32_t mode_count) noexcept {
    // The original x86 tests (device+0x18)>2 before it skips
    // the blocked condition stored in each 0x20-byte voice row.
    return mode_count<=2u && v.blocked_when_few_voices;
}
bool finishedSample(
    const OriginalWin95AllocatorVoiceSnapshot& v) noexcept {
    return v.stored_sample_end!=0u &&
           v.stored_sample_end <=
               (v.chip_current_address & 0x00ffffffu);
}
// Original Win95 mode-0/1/2 evaluation uses register3 VTFT[31:16],
// the driver key marker and live original DCYSUSV/CCCA values.
// A missing chip state MUST fail closed, not pretend an idle voice.
std::uint32_t scoreOfEligible(
    const OriginalWin95AllocatorVoiceSnapshot& v) noexcept {
    std::uint32_t score=v.volume_target_hi16;
    const std::uint32_t marker=v.internal_key_marker;
    if(marker!=0x0ffeu) {
        score+=(marker&0x1000u)?0x1200u:0x0200u;
        if((marker&0x00ffu)==0x00ffu)
            score+=0x0300u;
        else if((v.envelope_status_reg&0x8000u)==0u)
            score+=0x1300u;
    }
    return score;
}
} // namespace

std::optional<OriginalWin95PairSelection>
candidateOriginalWin95Mode1Pair(
    const std::array<OriginalWin95AllocatorVoiceSnapshot,
                     kOriginalWin95PhysicalVoiceCapacity>& voices,
    const std::uint16_t capacity,
    const std::uint32_t driver_mode_count) noexcept {
    if(!capacityValid(capacity)||capacity<2u)
        return std::nullopt;

    // Reverse physical voice order (capacity-1, ..., 0). Original
    // +0x3FE8 stores the most recently visited individual score
    // as PREVIOUS, including the invalid 0xFFFFF sentinel.
    std::uint32_t previous=kOriginalMaxPairPriority;
    std::uint32_t best=kOriginalMaxPairPriority;
    std::optional<OriginalWin95PairSelection> selected;
    for(std::size_t pos=capacity;pos>0u;--pos) {
        const std::size_t i=pos-1u;
        const auto& v=voices[i];
        // Source LCOD tests cheap blocked/marker predicates BEFORE any
        // live chip read. A skipped voice does not need a complete chip
        // snapshot and must not turn a valid selection into nullopt.
        if(forbiddenByDeviceMode(v,driver_mode_count) ||
           (i+1u<capacity &&
            forbiddenByDeviceMode(voices[i+1u],driver_mode_count)) ||
           v.internal_key_marker>=0x2000u) {
            previous=kOriginalMaxPairPriority;
            continue;
        }
        if(!v.complete)return std::nullopt;

        std::uint32_t current=scoreOfEligible(v);
        // Original +0x40B3 resets the individual score to zero
        // if the cached sample end was reached in the actual chip.
        if(finishedSample(v))current=0u;

        // Last capacity voice has no adjacent partner; its score
        // seeds the comparison for (capacity-2,capacity-1).
        if(i+1u<capacity) {
            const std::uint32_t sum=previous+current;
            if(sum==0u) {
                // +0x40DA -> +0x4167 early zero-pair allocation.
                return OriginalWin95PairSelection{
                    static_cast<std::uint8_t>(i),
                    static_cast<std::uint8_t>(i+1u),
                    0u,true};
            }
            const std::uint32_t priority=
                sum+2u*std::max(previous,current)+
                (v.pair_extra_penalty?kPairStatePenalty:0u);
            // Original unsigned JB skip: equality REPLACES
            // previous candidate. Reverse scan => lower ID tie.
            if(priority<=best) {
                best=priority;
                selected=OriginalWin95PairSelection{
                    static_cast<std::uint8_t>(i),
                    static_cast<std::uint8_t>(i+1u),
                    priority,false};
            }
        }
        previous=current;
    }
    return selected;
}

std::optional<OriginalWin95MonoSelection>
candidateOriginalWin95Mode2Range(
    const std::array<OriginalWin95AllocatorVoiceSnapshot,
                     kOriginalWin95PhysicalVoiceCapacity>& voices,
    const std::uint32_t packed_range,
    const std::uint16_t capacity,
    const std::uint32_t driver_mode_count) noexcept {
    if(!capacityValid(capacity))return std::nullopt;
    const std::uint32_t begin=packed_range&0x000000ffu;
    const std::uint32_t end=std::min(
        packed_range>>8u,static_cast<std::uint32_t>(capacity));
    if(begin>=end || begin>=capacity)return std::nullopt;

    std::optional<OriginalWin95MonoSelection> selected;
    std::uint32_t best=std::numeric_limits<std::uint32_t>::max();
    // Original +0x416C ascending scan, inclusive tie handling:
    // a later voice replaces the previous lowest-scored voice.
    for(std::uint32_t voice=begin;voice<end;++voice) {
        const auto& v=voices[voice];
        // Blocked voice is skipped in the original BEFORE reading
        // VTFT/DCYSUSV/CCCA; don't demand snapshot fields never read.
        if(forbiddenByDeviceMode(v,driver_mode_count))continue;
        // The sample-end test below still needs the actual CCCA even
        // for a key marker >=0x2000, so only blocked rows are skipped.
        if(!v.complete)return std::nullopt;
        if(v.internal_key_marker<0x2000u) {
            std::uint32_t score=scoreOfEligible(v);
            // Original +0x423B: marker 0x0FFE + VTFT upper16 zero
            // terminates immediately, with NO extra scoring.
            if(v.internal_key_marker==0x0ffeu && score==0u)
                return OriginalWin95MonoSelection{
                    static_cast<std::uint8_t>(voice),
                    OriginalWin95MonoSelectionReason::CompletelyIdle,0u};
            if(score<=best) {
                selected=OriginalWin95MonoSelection{
                    static_cast<std::uint8_t>(voice),
                    OriginalWin95MonoSelectionReason::LowestEnvelopePriority,
                    score};
                best=score;
            }
        }
        // The sample-end short circuit occurs even for a marker
        // that was INELIGIBLE for ranking (source +0x424F onwards).
        if(finishedSample(v))
            return OriginalWin95MonoSelection{
                static_cast<std::uint8_t>(voice),
                OriginalWin95MonoSelectionReason::SampleEndReached,0u};
    }
    return selected;
}

std::optional<std::array<OriginalWin95VxdPortWrite,4>>
candidateOriginalWin95Mode1PairReleaseWrites(
    const OriginalWin95PairSelection& pair) noexcept {
    if(pair.first_voice>=kOriginalWin95PhysicalVoiceCapacity-1u ||
       static_cast<unsigned>(pair.first_voice)+1u!=pair.second_voice)
        return std::nullopt;
    // The original +0x414A and +0x4158 both call writer +0x3CB1.
    // 0x2A0|physical voice indexes DCYSUSV through E22 byte port;
    // value 0x00FF is emitted as the original 16-bit A20 word.
    const auto a=static_cast<std::uint16_t>(
        0x00a0u|pair.first_voice);
    const auto b=static_cast<std::uint16_t>(
        0x00a0u|pair.second_voice);
    return std::array<OriginalWin95VxdPortWrite,4>{{
        {0xe22u,a,8u},{0xa20u,0x00ffu,16u},
        {0xe22u,b,8u},{0xa20u,0x00ffu,16u}}};
}
std::optional<std::array<OriginalWin95VxdPortWrite,2>>
candidateOriginalWin95Mode2ReleaseWrites(
    const OriginalWin95MonoSelection& selected) noexcept {
    if(selected.physical_voice>=kOriginalWin95PhysicalVoiceCapacity)
        return std::nullopt;
    return std::array<OriginalWin95VxdPortWrite,2>{{
        {0xe22u,static_cast<std::uint16_t>(0x00a0u|
            selected.physical_voice),8u},
        {0xa20u,0x00ffu,16u}
    }};
}
std::optional<OriginalWin95AllocatorDispatchResult>
candidateOriginalWin95VxdDispatchAllModes(
    const std::array<OriginalWin95AllocatorVoiceSnapshot,
                     kOriginalWin95PhysicalVoiceCapacity>& voices,
    const OriginalWin95AllocatorDispatchInput& input) noexcept {
    if (!input.independently_verified_source_state ||
        !capacityValid(input.original_device_physical_capacity) ||
        input.original_device_mode_count==0u)
        return std::nullopt;

    OriginalWin95AllocatorDispatchResult result{};
    result.original_x86_dispatch_mode=input.original_x86_dispatch_mode;
    const auto capacity=input.original_device_physical_capacity;
    const auto mode_count=input.original_device_mode_count;
    if (input.original_x86_dispatch_mode==0u) {
        // Existing mode-0 scorer has 30-voice original music-budget
        // bounds. It must NOT infer 30 from an actual device+0x06 of
        // 32 without a separately verified source-mode scan ceiling.
        const auto limit=input.independently_verified_mode0_scan_ceiling;
        if (limit==0u || limit>kOriginalCreativeMusicVoices ||
            limit>capacity)
            return std::nullopt;
        std::array<OriginalWin95MonoVoiceSnapshot,
                   kOriginalCreativeMusicVoices> original{};
        for (std::size_t i=0;i<limit;++i) {
            const auto& src=voices[i];
            if (!src.complete) return std::nullopt;
            auto& dst=original[i];
            dst.complete=true;
            dst.blocked_when_few_voices=src.blocked_when_few_voices;
            dst.internal_key_marker=src.internal_key_marker;
            dst.volume_target_hi16=src.volume_target_hi16;
            dst.envelope_status_reg=src.envelope_status_reg;
            dst.stored_sample_end=src.stored_sample_end;
            dst.chip_current_address=src.chip_current_address;
        }
        const auto chosen=candidateOriginalWin95Mode0MonoVoice(
            original,limit,mode_count);
        if (chosen) {
            result.physical_voices[0]=chosen->physical_voice;
            result.priority=chosen->priority;
            result.voice_count=1u;
        }
        return result;
    }
    if (input.original_x86_dispatch_mode==1u) {
        // Pair selection consults both voice and adjacent voice
        // original VXD state plus live hardware register snapshots.
        // A truncated row is NOT a genuine available physical voice.
        if (capacity<2u) return result; // original no candidate: 0xFF
        // Do not preflight every voice: the source short-circuits on
        // blocked/forbidden/idle voices before requiring all live reads.
        const auto chosen=candidateOriginalWin95Mode1Pair(
            voices,capacity,mode_count);
        if (chosen) {
            result.physical_voices={chosen->first_voice,
                                    chosen->second_voice};
            result.priority=chosen->priority;
            result.voice_count=2u;
            result.immediate_idle_pair=chosen->immediate_zero_pair;
        }
        return result;
    }
    if (input.original_x86_dispatch_mode==2u) {
        const auto begin=input.original_mode2_packed_range&0xffu;
        const auto end=std::min(input.original_mode2_packed_range>>8u,
                                static_cast<std::uint32_t>(capacity));
        if (begin>=end) return result; // original no candidate: 0xFF
        // The candidate performs source-ordered lazy read validation;
        // a blocked row must not force a speculative chip snapshot.
        const auto chosen=candidateOriginalWin95Mode2Range(
            voices,input.original_mode2_packed_range,capacity,mode_count);
        if (chosen) {
            result.physical_voices[0]=chosen->physical_voice;
            result.priority=chosen->priority;
            result.voice_count=1u;
        }
        return result;
    }
    // Original x86 dispatcher's invalid-mode result is 0xFF. This is
    // *not* an instruction to allocate voice 255 or silently pick mode0.
    return result;
}

} // namespace awe64::creative
