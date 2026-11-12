#include "CreativeSf2VxdStateSnapshot.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace awe64::creative {
namespace {
std::uint16_t le16(std::span<const std::uint8_t> b,
                   const std::size_t at) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(b[at])|
        (static_cast<unsigned>(b[at+1u])<<8u));
}
std::uint32_t le32(std::span<const std::uint8_t> b,
                   const std::size_t at) noexcept {
    return static_cast<std::uint32_t>(b[at])|
        (static_cast<std::uint32_t>(b[at+1u])<<8u)|
        (static_cast<std::uint32_t>(b[at+2u])<<16u)|
        (static_cast<std::uint32_t>(b[at+3u])<<24u);
}
}
std::optional<OriginalSf2VxdFinalTargetInputs>
candidateOriginalSf2TargetStateFromVxdImage(
    const OriginalSf2VerifiedVxdDeviceSnapshot& original,
    const std::uint8_t channel,
    const std::uint8_t physical_voice) noexcept {
    const auto b=original.original_state_bytes;
    if(!original.independently_verified_original_vxd_snapshot ||
       !original.original_lcod_31word_table_verified ||
       b.size()<kOriginalVxdMinDeviceImage ||
       channel>=kOriginalCreativeMidiChannels ||
       physical_voice>=kOriginalCreativeMusicVoices ||
       le32(b,kOriginalVxdRomIdOffset)!=0x4d474d31u ||
       le16(b,0x12u)!=0u) // only genuine observed DRY device mode0
        return std::nullopt;
    const std::size_t c=kOriginalVxdChannelRowStart+
        static_cast<std::size_t>(channel)*kOriginalVxdChannelRowStride;
    const std::size_t v=kOriginalVxdVoiceRowStart+
        static_cast<std::size_t>(physical_voice)*kOriginalVxdVoiceRowStride;
    if(c+0x18u>b.size() || v+0x10u>b.size())return std::nullopt;
    const std::uint16_t signed_channel=le16(b,c+0x0eu);
    OriginalSf2VxdFinalTargetInputs out{};
    out.source_original_device_voice_state_verified=true;
    out.original_lcod_2fa0_16word_table_verified=true;
    out.original_lcod_2f82_negative_table_verified=true;
    out.channel_word_plus_0e=
        static_cast<std::int16_t>(signed_channel<=0x7fffu
            ?static_cast<std::int32_t>(signed_channel)
            :static_cast<std::int32_t>(signed_channel)-65536);
    out.allocated_voice_word_plus_0e=le16(b,v+0x0eu);
    out.channel_dword_plus_14=le32(b,c+0x14u);
    out.source_lcod_2fa0_table=original.source_positive_16word_table;
    out.source_lcod_2f82_negative_table=original.source_negative_15word_table;
    return out;
}
std::optional<OriginalSf2ResolvedSnapshotNote>
candidateOriginalSf2ResolveVxdSnapshotVoices(
    const std::span<const OriginalSf2VxdSnapshotVoice> assigned,
    const OriginalSf2VerifiedVxdDeviceSnapshot& snapshot) noexcept {
    if(assigned.empty() ||
       assigned.size()>kOriginalCreativeMaxNoteStartDescriptors)
        return std::nullopt;
    const auto channel=assigned[0].original_midi_channel;
    const auto key=assigned[0].assigned_voice.played_midi_key;
    OriginalSf2ResolvedSnapshotNote resolved{};
    for(const auto& source:assigned) {
        const auto& voice=source.assigned_voice;
        if(source.original_midi_channel!=channel ||
           voice.played_midi_key!=key ||
           !voice.source_original_voice_verified ||
           !voice.lfo.source_original_state_verified)
            return std::nullopt; // one MIDI NoteOn event, same channel/key
        auto inputs=candidateOriginalSf2TargetStateFromVxdImage(
            snapshot,channel,voice.physical_voice);
        if(!inputs)return std::nullopt;
        const auto targets=candidateOriginalSf2ComputeFinalTargetsFromVxdState(
            voice.verified_after_start_record,*inputs);
        if(!targets)return std::nullopt;
        resolved.voices[resolved.count]=voice;
        // Ignores arbitrary caller target words; derives actual originals
        // from this SAME verified VXD state snapshot for all voices.
        resolved.voices[resolved.count].target=*targets;
        ++resolved.count;
    }
    return resolved;
}
std::optional<OriginalSf2FullNoteOnPlan>
candidateOriginalSf2FullNoteOnFromVxdSnapshot(
    const std::span<const OriginalSf2VxdSnapshotVoice> voices,
    const OriginalSf2VerifiedVxdDeviceSnapshot& snapshot) noexcept {
    const auto resolved=candidateOriginalSf2ResolveVxdSnapshotVoices(
        voices,snapshot);
    if(!resolved)return std::nullopt;
    return candidateOriginalSf2FullNoteOnPorts(
        std::span<const OriginalSf2FullNoteOnVoice>(
            resolved->voices.data(),resolved->count));
}
bool applyOriginalCreativeSf2FullNoteOnFromVxdSnapshot(
    awe64::Chip& chip,
    const std::span<const OriginalSf2VxdSnapshotVoice> voices,
    const OriginalSf2VerifiedVxdDeviceSnapshot& snapshot) noexcept {
    const auto resolved=candidateOriginalSf2ResolveVxdSnapshotVoices(
        voices,snapshot);
    if(!resolved)return false;
    return applyOriginalCreativeSf2FullNoteOnPorts(
        chip,std::span<const OriginalSf2FullNoteOnVoice>(
            resolved->voices.data(),resolved->count));
}
} // namespace awe64::creative
