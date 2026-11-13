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
std::optional<OriginalSf2ExecutedLiveLayeredBankNote>
candidateOriginalSf2StartNextIdleLayeredRomBankNoteOnLiveChip(
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
       !std::equal(mutable_state.begin(),
                   mutable_state.begin()+kOriginalVxdMinDeviceImage,
                   source_context.original_state_bytes.begin()))
        return std::nullopt;
    const auto inventory=prepareOriginalCreativeRomNote(
        midi,midi_state,bank,bank_number,rom_word_count);
    if(!inventory || inventory->count<2u ||
       inventory->count>kOriginalCreativeMaxNoteStartDescriptors)
        return std::nullopt;
    for(std::size_t i=0;i<inventory->count;++i) {
        if(inventory->layers[i].metadata.source_format!=
           OriginalCreativeBankFormat::Format2Sf2)
            return std::nullopt;
    }
    // Plan the original THREE-LANE idle-voice lookup on a source VXD
    // image copy. A previously selected physical voice gets precisely
    // the original channel/key marker in this scratch image, so it is
    // no longer eligible as completely idle for the NEXT layer.
    // The chip isn't triggered until the WHOLE multi-voice original
    // output plan has passed the existing descriptor/register oracle.
    std::array<std::uint8_t,kOriginalVxdMinDeviceImage> scratch{};
    std::copy_n(mutable_state.begin(),scratch.size(),scratch.begin());
    OriginalSf2ExecutedLiveLayeredBankNote result{};
    std::array<std::uint8_t,kOriginalCreativeMaxNoteStartDescriptors> selected{};
    for(std::size_t layer=0;layer<inventory->count;++layer) {
        const auto chosen=candidateOriginalSf2LiveVxdMonoVoice(
            chip,scratch,true);
        if(!chosen || chosen->reason!=
             OriginalWin95MonoSelectionReason::CompletelyIdle ||
           chosen->physical_voice>=kOriginalCreativeMusicVoices)
            return std::nullopt;
        const auto physical=chosen->physical_voice;
        for(std::size_t prior=0;prior<layer;++prior)
            if(selected[prior]==physical)return std::nullopt;
        selected[layer]=physical;
        result.selections[layer]=*chosen;
        const auto row=kOriginalVxdVoiceRowStart+
            static_cast<std::size_t>(physical)*kOriginalVxdVoiceRowStride;
        scratch[row]=midi.data1;
        scratch[row+1u]=midi.channel;
    }
    const auto compiled=candidateOriginalSf2CompileRomBankNote(
        bank,midi,midi_state,bank_number,rom_word_count,
        resources,start,source_context,
        std::span<const std::uint8_t>(selected.data(),inventory->count),true);
    if(!compiled || compiled->voice_count!=inventory->count ||
       compiled->typed_original_outs.count!=
           static_cast<std::size_t>(inventory->count)*
               kOriginalSf2OneVoiceStartWrites)
        return std::nullopt;
    auto verified_after=source_context;
    verified_after.original_state_bytes=compiled->vxd_state_after_noteon;
    if(!applyOriginalCreativeSf2FullNoteOnFromVxdSnapshot(
           chip,compiled->selectedVoices(),verified_after))
        return std::nullopt;
    std::copy_n(compiled->vxd_state_after_noteon.begin(),
                kOriginalVxdMinDeviceImage,mutable_state.begin());
    result.original_compilation=*compiled;
    return result;
}
namespace {
std::uint16_t readSourceMarker(
    const std::span<const std::uint8_t> state,
    const std::size_t physical) noexcept {
    const auto offset=kOriginalVxdVoiceRowStart+
        physical*kOriginalVxdVoiceRowStride;
    return static_cast<std::uint16_t>(state[offset] |
        (static_cast<unsigned>(state[offset+1u])<<8u));
}
}
bool OriginalSf2RomBankVoiceOwnership::recordExecutedNoteOn(
    const OriginalSf2ExecutedLiveIdleMonoNote& executed,
    const std::span<const std::uint8_t> state) noexcept {
    return recordCompiledNoteOn(executed.original_compilation,state);
}
bool OriginalSf2RomBankVoiceOwnership::recordExecutedNoteOn(
    const OriginalSf2ExecutedLiveLayeredBankNote& executed,
    const std::span<const std::uint8_t> state) noexcept {
    return recordCompiledNoteOn(executed.original_compilation,state);
}
bool OriginalSf2RomBankVoiceOwnership::recordCompiledNoteOn(
    const OriginalSf2CompiledRomBankNote& compiled,
    const std::span<const std::uint8_t> state) noexcept {
    const auto voices=compiled.selectedVoices();
    if(state.size()<kOriginalVxdMinDeviceImage ||
       voices.empty() || voices.size()>kOriginalCreativeMaxNoteStartDescriptors ||
       compiled.voice_count!=voices.size())
        return false;
    std::uint32_t physicals=0u;
    for(const auto& src:voices) {
        const auto physical=src.assigned_voice.physical_voice;
        const auto channel=src.original_midi_channel;
        const auto key=src.assigned_voice.played_midi_key;
        if(physical>=kOriginalCreativeMusicVoices || occupied_[physical] ||
           !src.assigned_voice.source_original_voice_verified ||
           channel>=kOriginalCreativeMidiChannels || key>127u ||
           (physicals&(1u<<physical))!=0u ||
           readSourceMarker(state,physical)!=
               static_cast<std::uint16_t>((channel<<8u)|key))
            return false;
        const auto offset=kOriginalVxdVoiceRowStart+
            static_cast<std::size_t>(physical)*kOriginalVxdVoiceRowStride;
        if(!candidateOriginalSf2VerifiedReleaseRates(
               state.subspan(offset,kOriginalVxdVoiceRowStride),src))
            return false;
        physicals|=1u<<physical;
    }
    // All physical source bindings were verified first. A failed later
    // layer never leaves a partly owned note in the bank ledger.
    for(const auto& src:voices) {
        const auto physical=src.assigned_voice.physical_voice;
        source_voices_[physical]=src;
        occupied_[physical]=true;
    }
    return true;
}
std::size_t OriginalSf2RomBankVoiceOwnership::trackedPhysicalVoices() const noexcept {
    std::size_t n=0u;
    for(const auto b:occupied_)if(b)++n;
    return n;
}
std::optional<OriginalSf2SourceNoteReleaseResult>
OriginalSf2RomBankVoiceOwnership::noteOff(
    awe64::Chip& chip,const std::span<std::uint8_t> state,
    const std::uint8_t channel,const std::uint8_t key) noexcept {
    if(!chip.ready() || state.size()<kOriginalVxdMinDeviceImage ||
       channel>=kOriginalCreativeMidiChannels || key>127u)
        return std::nullopt;
    std::array<OriginalSf2VxdSnapshotVoice,
               kOriginalCreativeMaxNoteStartDescriptors> target{};
    std::size_t count=0;
    for(std::size_t i=0;i<occupied_.size();++i) {
        if(!occupied_[i])continue;
        const auto& src=source_voices_[i];
        if(src.original_midi_channel!=channel ||
           src.assigned_voice.played_midi_key!=key)continue;
        if(count>=target.size() ||
           readSourceMarker(state,i)!=
               static_cast<std::uint16_t>((channel<<8u)|key))
            return std::nullopt;
        target[count++]=src;
    }
    if(count==0u)return std::nullopt;
    const auto result=applyOriginalSf2MidiNoteOffFromVxdState(
        chip,state,
        std::span<const OriginalSf2VxdSnapshotVoice>(target.data(),count),
        channel,key,true);
    if(!result)return std::nullopt;
    // Deferred NoteOff keeps source ownership until original CC64 up.
    // An immediate NoteOff changes the original marker to 0x0FFE.
    if(!result->deferred_by_sustain) {
        for(std::size_t i=0;i<occupied_.size();++i) {
            if(!occupied_[i])continue;
            const auto& src=source_voices_[i];
            if(src.original_midi_channel==channel &&
               src.assigned_voice.played_midi_key==key) {
                if(readSourceMarker(state,i)!=0x0ffeu)return std::nullopt;
                occupied_[i]=false;
            }
        }
    }
    return result;
}
std::optional<OriginalSf2SourceCc64Result>
OriginalSf2RomBankVoiceOwnership::controller64(
    awe64::Chip& chip,const std::span<std::uint8_t> state,
    const std::uint8_t channel,const std::uint8_t value) noexcept {
    if(!chip.ready() || state.size()<kOriginalVxdMinDeviceImage ||
       channel>=kOriginalCreativeMidiChannels || value>127u)
        return std::nullopt;
    std::array<OriginalSf2VxdSnapshotVoice,
               kOriginalCreativeMusicVoices> active{};
    std::size_t count=0u;
    for(std::size_t i=0;i<occupied_.size();++i)
        if(occupied_[i])active[count++]=source_voices_[i];
    const auto selected=std::span<const OriginalSf2VxdSnapshotVoice>(
        active.data(),count);
    // Preflight the ORIGINAL 30-physical-slot channel:FF scan;
    // do not discard any source provenance before actual chip release.
    const auto scan=candidateOriginalSf2Cc64DeferredScan(
        std::span<const std::uint8_t>(state.data(),state.size()),
        selected,channel,value,true);
    if(!scan)return std::nullopt;
    const auto released=applyOriginalSf2MidiCc64FromVxdState(
        chip,state,selected,channel,value,true);
    if(!released || released->released_deferred_voices!=scan->count)
        return std::nullopt;
    if(value<64u)
        for(std::size_t i=0;i<scan->count;++i) {
            const auto physical=scan->voices[i].physical_voice;
            if(readSourceMarker(state,physical)!=0x0ffeu)
                return std::nullopt;
            occupied_[physical]=false;
        }
    return released;
}
} // namespace awe64::creative
