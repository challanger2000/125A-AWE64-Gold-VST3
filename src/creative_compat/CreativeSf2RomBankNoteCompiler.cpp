#include "CreativeSf2RomBankNoteCompiler.h"
#include "CreativeMidiRomPlan.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace awe64::creative {
std::optional<OriginalSf2CompiledRomBankNote>
candidateOriginalSf2CompileRomBankNote(
    const std::span<const std::uint8_t> bank,
    const MidiMessage& midi,
    const MidiState& midi_state,
    const std::uint16_t bank_number,
    const std::size_t rom_word_count,
    const OriginalCreativeSf2DriverResources& source,
    const OriginalSf2VoiceStartConditions& start,
    const OriginalSf2VerifiedVxdDeviceSnapshot& original_snapshot,
    const std::span<const std::uint8_t> voice_assignment,
    const bool source_verified) noexcept {
    const auto state=original_snapshot.original_state_bytes;
    if(!source_verified ||
       !original_snapshot.independently_verified_original_vxd_snapshot ||
       !original_snapshot.original_lcod_31word_table_verified ||
       state.size()<kOriginalVxdMinDeviceImage ||
       midi.kind!=MidiKind::NoteOn || midi.channel>=16u ||
       midi.data1>127u || midi.data2==0u || midi.data2>127u ||
       start.creative_bank!=bank_number ||
       start.midi_key!=midi.data1 ||
       start.midi_velocity!=midi.data2 ||
       start.device_effects_flag!=0u ||
       start.channel_chorus!=0u ||
       start.channel_reverb!=0u ||
       rom_word_count==0u ||
       voice_assignment.empty() ||
       voice_assignment.size()>kOriginalCreativeMaxNoteStartDescriptors)
        return std::nullopt;
    const auto& ch=midi_state.channel(midi.channel);
    if(start.volume.cc7!=ch.cc7_volume ||
       start.volume.cc11!=ch.cc11_expression ||
       start.channel_pan!=ch.cc10_pan ||
       // Original device+0x10 is WinMM master volume attenuation, not
       // the signed SF2 instrument generator48 stored at descriptor+0x60.
       (static_cast<std::uint16_t>(state[0x10u])|
        (static_cast<std::uint16_t>(state[0x11u])<<8u))!=
           start.volume.device_state_additive_attenuation)
        return std::nullopt;
    const auto selected=prepareOriginalCreativeRomNote(
        midi,midi_state,bank,bank_number,rom_word_count);
    if(!selected || selected->count==0u ||
       voice_assignment.size()!=selected->count ||
       selected->channel!=midi.channel)
        return std::nullopt;
    OriginalSf2CompiledRomBankNote result{};
    result.voice_count=selected->count;
    std::copy_n(state.begin(),result.vxd_state_after_noteon.size(),
                result.vxd_state_after_noteon.begin());
    std::uint32_t used=0u;
    for(std::size_t i=0;i<selected->count;++i) {
        const auto physical=voice_assignment[i];
        if(physical>=kOriginalCreativeMusicVoices ||
           (used&(1u<<physical))!=0u)
            return std::nullopt;
        used|=1u<<physical;
        const auto& layer=selected->layers[i];
        const auto descriptor=candidateOriginalSf2NoteFromVerifiedVxdResources(
            layer.metadata,layer.address,source,
            layer.metadata.original_preset_row_index,start);
        if(!descriptor)return std::nullopt;
        auto& dst=result.assignments[i];
        dst.original_midi_channel=midi.channel;
        dst.assigned_voice.physical_voice=physical;
        dst.assigned_voice.played_midi_key=midi.data1;
        dst.assigned_voice.source_original_voice_verified=true;
        dst.assigned_voice.lfo={true,0u,0u};
        dst.assigned_voice.verified_after_start_record=
            descriptor->after_note_start;
        result.before_note_start[i]=descriptor->before_note_start;
        const auto rateEnd=static_cast<std::size_t>(
            kOriginalVxdVoiceRowStart+physical*kOriginalVxdVoiceRowStride+9u);
        const auto& snapshot=result.vxd_state_after_noteon;
        const auto storedEnd=
            static_cast<std::uint32_t>(snapshot[rateEnd-3u])|
            (static_cast<std::uint32_t>(snapshot[rateEnd-2u])<<8u)|
            (static_cast<std::uint32_t>(snapshot[rateEnd-1u])<<16u)|
            (static_cast<std::uint32_t>(snapshot[rateEnd])<<24u);
        if(storedEnd>0xfffffbu)return std::nullopt;
    }
    const auto voices=result.selectedVoices();
    // Recreate the two actual original source state mutations:
    // LCOD+0x3DF8 & LCOD+0x5A9D/+0x5AA6 for the note marker/rates;
    // LCOD+0x574F for the initial per-voice pitch word. Both helpers
    // preflight ALL layers; no HW writes can occur on failure.
    if(!candidateOriginalSf2StoreNoteOnSourceReleaseState(
            result.vxd_state_after_noteon,true,voices) ||
       !candidateOriginalSf2WriteInitialIpToVxdState(
            result.vxd_state_after_noteon,true,voices))
        return std::nullopt;
    auto localized=original_snapshot;
    localized.original_state_bytes=result.vxd_state_after_noteon;
    const auto ports=candidateOriginalSf2FullNoteOnFromVxdSnapshot(
        voices,localized);
    if(!ports ||
       ports->voices!=result.voice_count ||
       ports->count!=static_cast<std::size_t>(result.voice_count)*
                    kOriginalSf2OneVoiceStartWrites)
        return std::nullopt;
    result.typed_original_outs=*ports;
    return result;
}
std::optional<OriginalSf2ExecutedLiveIdleMonoNote>
candidateOriginalSf2StartSingleRomBankLayerOnLiveChip(
    awe64::Chip& chip,
    const std::span<const std::uint8_t> bank,
    const MidiMessage& midi,
    const MidiState& midi_state,
    const std::uint16_t bank_number,
    const std::size_t rom_word_count,
    const OriginalCreativeSf2DriverResources& resources,
    const OriginalSf2VoiceStartConditions& start,
    const OriginalSf2VerifiedVxdDeviceSnapshot& state,
    const bool verified) noexcept {
    if(!chip.ready() || !verified)return std::nullopt;
    const auto inventory=prepareOriginalCreativeRomNote(
        midi,midi_state,bank,bank_number,rom_word_count);
    if(!inventory || inventory->count!=1u ||
       inventory->layers[0].metadata.source_format!=
           OriginalCreativeBankFormat::Format2Sf2)
        return std::nullopt;
    const auto selected=candidateOriginalSf2LiveVxdMonoVoice(
        chip,state.original_state_bytes,
        state.independently_verified_original_vxd_snapshot);
    if(!selected || selected->reason!=
            OriginalWin95MonoSelectionReason::CompletelyIdle)
        return std::nullopt; // no original busy-voice-steal without proof
    const std::array<std::uint8_t,1> assigned{{selected->physical_voice}};
    auto compiled=candidateOriginalSf2CompileRomBankNote(
        bank,midi,midi_state,bank_number,rom_word_count,
        resources,start,state,assigned,true);
    if(!compiled || compiled->voice_count!=1u)
        return std::nullopt;
    auto derived=state;
    derived.original_state_bytes=compiled->vxd_state_after_noteon;
    // The existing production port bridge FULLY preflights all 61
    // source-ordered typed OUTs BEFORE executing on the real chip.
    if(!applyOriginalCreativeSf2FullNoteOnFromVxdSnapshot(
           chip,compiled->selectedVoices(),derived))
        return std::nullopt;
    return OriginalSf2ExecutedLiveIdleMonoNote{*selected,*compiled};
}
std::optional<OriginalSf2ExecutedLiveIdleMonoNote>
candidateOriginalSf2StartNextIdleRomBankNoteOnLiveChip(
    awe64::Chip& chip,
    const std::span<std::uint8_t> mutable_state,
    const std::span<const std::uint8_t> bank,
    const MidiMessage& midi,
    const MidiState& midi_state,
    const std::uint16_t bank_number,
    const std::size_t rom_word_count,
    const OriginalCreativeSf2DriverResources& resources,
    const OriginalSf2VoiceStartConditions& start,
    const OriginalSf2VerifiedVxdDeviceSnapshot& source_context,
    const bool source_verified) noexcept {
    if(!source_verified || !chip.ready() ||
       !source_context.independently_verified_original_vxd_snapshot ||
       !source_context.original_lcod_31word_table_verified ||
       mutable_state.size()<kOriginalVxdMinDeviceImage ||
       source_context.original_state_bytes.size()<
           kOriginalVxdMinDeviceImage ||
       !std::equal(
           mutable_state.begin(),
           mutable_state.begin()+kOriginalVxdMinDeviceImage,
           source_context.original_state_bytes.begin()))
        return std::nullopt;
    // Retain the independently original-source 15+16 lookup tables.
    // They are not part of the 59-WORD SF2 generator templates, and
    // inventing zero attenuation tables would falsify attack128.
    auto original=source_context;
    original.original_state_bytes=
        std::span<const std::uint8_t>(mutable_state.data(),mutable_state.size());
    const auto played=candidateOriginalSf2StartSingleRomBankLayerOnLiveChip(
        chip,bank,midi,midi_state,bank_number,rom_word_count,
        resources,start,original,true);
    if(!played)return std::nullopt;
    // Original source LCOD+0x3DF8/0x5A9D/0x574F state mutations
    // from the exact just-started physical voice become the only
    // caller-visible state transition. The next live allocator can
    // no longer treat that already-keyed source voice as idle.
    std::copy_n(played->original_compilation.vxd_state_after_noteon.begin(),
                kOriginalVxdMinDeviceImage,mutable_state.begin());
    return played;
}
} // namespace awe64::creative
