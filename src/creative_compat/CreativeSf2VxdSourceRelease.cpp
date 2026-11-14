#include "CreativeSf2VxdSourceRelease.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
namespace awe64::creative {
namespace {
std::uint16_t le16(std::span<const std::uint8_t> b,const std::size_t i) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<unsigned>(b[i])|(static_cast<unsigned>(b[i+1u])<<8u));
}
std::uint32_t le32(std::span<const std::uint8_t> b,const std::size_t i) noexcept {
    return static_cast<std::uint32_t>(b[i])|
           (static_cast<std::uint32_t>(b[i+1u])<<8u)|
           (static_cast<std::uint32_t>(b[i+2u])<<16u)|
           (static_cast<std::uint32_t>(b[i+3u])<<24u);
}
std::uint32_t readRealChipData0(
    awe64::Chip& chip,const std::uint8_t voice,
    const std::uint8_t reg) noexcept {
    chip.writePort(0xe22u,static_cast<std::uint16_t>((reg<<5u)|voice));
    const auto lo=chip.readPort(0x620u);
    const auto hi=chip.readPort(0x622u);
    return static_cast<std::uint32_t>(lo)|
           (static_cast<std::uint32_t>(hi)<<16u);
}
bool writePairs(awe64::Chip& chip,
    const OriginalVxdNoteReleaseCommands& commands,
    const std::size_t start,const std::size_t count) noexcept {
    return applyOriginalCreativeVxdVoicePorts(chip,
        std::span<const OriginalWin95VxdPortWrite>(
            commands.writes.data()+start,count));
}
// Shared original x86 LCOD+0x60E4 release body. Both ordinary NoteOff
// and pedal-up call this very same helper; no synthetic fast-release or
// independent ADSR sound engine is added.
std::optional<std::size_t> releaseOriginalVoice(
    awe64::Chip& chip,const std::span<std::uint8_t> state,
    const std::size_t base,const std::uint8_t physical,
    OriginalVxdNoteReleaseEvidence e) noexcept {
    e.original_sustain_held=false;
    e.current_chip_registers_verified=true;
    auto cmd=candidateOriginalVxdMidiNoteRelease(physical,e);
    if(!cmd || cmd->count<4u || !writePairs(chip,*cmd,0u,4u))
        return std::nullopt;
    std::size_t writes=4u;
    if(e.stored_end_word!=0u) {
        // Original source LCOD+0x613B reads CSL then PSST AFTER both
        // release-envelope OUTs, preserving the current upper bytes.
        e.observed_chip_csl=readRealChipData0(chip,physical,7u);
        e.observed_chip_psst=readRealChipData0(chip,physical,6u);
        cmd=candidateOriginalVxdMidiNoteRelease(physical,e);
        if(!cmd || cmd->count!=8u ||
           !writePairs(chip,*cmd,4u,4u))return std::nullopt;
        writes+=4u;
        for(unsigned b=0;b<4u;++b)
            state[base+0x0au+b]=static_cast<std::uint8_t>(
                e.stored_end_word>>(b*8u));
    }
    state[base]=0xfeu;
    state[base+1u]=0x0fu;
    return writes;
}
}
std::optional<OriginalSf2VerifiedReleaseRates>
candidateOriginalSf2VerifiedReleaseRates(
    const std::span<const std::uint8_t> original_32byte_voice_record,
    const OriginalSf2VxdSnapshotVoice& voice) noexcept {
    if(original_32byte_voice_record.size()!=kOriginalVxdVoiceRowStride ||
       !voice.assigned_voice.source_original_voice_verified)
        return std::nullopt;
    const auto& d=voice.assigned_voice.verified_after_start_record;
    // In the genuine format-2 VXD, the NoteOn source writes generator30
    // to voice+0x15 and generator38 to voice+0x16. Both values must be
    // low-byte encoded 0..127, with zero high descriptor byte.
    if(d[0x8cu]!=1u || d[0x8du]!=0u ||
       d[0x3du]!=0u || d[0x4du]!=0u ||
       d[0x3cu]>127u || d[0x4cu]>127u ||
       original_32byte_voice_record[0x15u]!=d[0x3cu] ||
       original_32byte_voice_record[0x16u]!=d[0x4cu])
        return std::nullopt;
    return OriginalSf2VerifiedReleaseRates{d[0x3cu],d[0x4cu]};
}
std::optional<OriginalSf2SourceNoteReleaseResult>
applyOriginalSf2MidiNoteOffFromVxdState(
    awe64::Chip& chip,const std::span<std::uint8_t> state,
    const std::span<const OriginalSf2VxdSnapshotVoice> voices,
    const std::uint8_t channel,const std::uint8_t key,
    const bool verified) noexcept {
    if(!verified || !chip.ready() ||
       state.size()<kOriginalVxdMinDeviceImage ||
       channel>=kOriginalCreativeMidiChannels || key>127u ||
       voices.empty() ||
       voices.size()>kOriginalCreativeMaxNoteStartDescriptors)
        return std::nullopt;
    const std::span<const std::uint8_t> read(state.data(),state.size());
    if(le32(read,kOriginalVxdRomIdOffset)!=0x4d474d31u ||
       le16(read,0x12u)!=0u)return std::nullopt;
    struct Item {
        std::size_t base=0;
        std::uint8_t physical=0;
        OriginalVxdNoteReleaseEvidence original{};
    };
    std::array<Item,kOriginalCreativeMaxNoteStartDescriptors> pending{};
    std::uint32_t seen=0u;
    const auto expected=static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(channel)<<8u)|key);
    std::size_t count=0;
    const auto channel_at=kOriginalVxdChannelRowStart+
        static_cast<std::size_t>(channel)*kOriginalVxdChannelRowStride;
    if(channel_at+0x24u>read.size())return std::nullopt;
    const bool channel_sustain=read[channel_at+4u]!=0u;
    // Validate EVERY physical voice and the complete 0x20-byte original
    // state BEFORE changing an original chip register or VXD state byte.
    for(const auto& layer:voices) {
        const auto physical=layer.assigned_voice.physical_voice;
        if(layer.original_midi_channel!=channel ||
           layer.assigned_voice.played_midi_key!=key ||
           !layer.assigned_voice.source_original_voice_verified ||
           physical>=kOriginalCreativeMusicVoices ||
           (seen&(1u<<physical))!=0u)return std::nullopt;
        const auto base=kOriginalVxdVoiceRowStart+
            static_cast<std::size_t>(physical)*kOriginalVxdVoiceRowStride;
        if(base+0x20u>read.size() || le16(read,base)!=expected)
            return std::nullopt;
        const auto rates=candidateOriginalSf2VerifiedReleaseRates(
            read.subspan(base,kOriginalVxdVoiceRowStride),layer);
        if(!rates)return std::nullopt;
        seen|=1u<<physical;
        auto& e=pending[count].original;
        e.original_key_channel_match=true;
        e.original_voice_record_verified=true;
        // Original LCOD+0x60B4 checks [EBX+4], where EBX is the
        // channel row at state+0x442+channel*0x24, not voice+4.
        e.original_sustain_held=channel_sustain;
        e.encoded_volume_release=rates->volume;
        e.encoded_mod_release=rates->modulation;
        e.stored_end_word=le32(read,base+6u);
        // Actual original helper accepts a max 24-bit cursor +4.
        if(e.stored_end_word>0xfffffbu)return std::nullopt;
        // Initial four hardware OUTs, if nondeferred, depend only on
        // authenticated source voice record; no HW reads required yet.
        e.current_chip_registers_verified=true;
        const auto proof=candidateOriginalVxdMidiNoteRelease(physical,e);
        if(!proof || (!e.original_sustain_held && proof->count<4u))
            return std::nullopt;
        pending[count].base=base;
        pending[count].physical=physical;
        ++count;
    }
    OriginalSf2SourceNoteReleaseResult result{};
    result.voice_count=count;
    for(std::size_t i=0;i<count;++i) {
        const auto& item=pending[i];
        auto e=item.original;
        if(e.original_sustain_held) {
            // Original +0x60B4 marks the KEY component FF, retains
            // channel marker and emits ZERO HW writes until pedal lift.
            state[item.base]=0xffu;
            result.deferred_by_sustain=true;
            continue;
        }
        const auto writes=releaseOriginalVoice(
            chip,state,item.base,item.physical,e);
        if(!writes)return std::nullopt;
        result.typed_port_writes+=*writes;
    }
    return result;
}
std::optional<OriginalSf2Cc64DeferredScan>
candidateOriginalSf2Cc64DeferredScan(
    const std::span<const std::uint8_t> state,
    const std::span<const OriginalSf2VxdSnapshotVoice> assigned,
    const std::uint8_t channel,const std::uint8_t cc_value,
    const bool verified) noexcept {
    if(!verified || state.size()<kOriginalVxdMinDeviceImage ||
       channel>=kOriginalCreativeMidiChannels || cc_value>127u ||
       assigned.size()>kOriginalCreativeMusicVoices ||
       le32(state,kOriginalVxdRomIdOffset)!=0x4d474d31u ||
       le16(state,0x12u)!=0u)
        return std::nullopt;
    const auto chan=kOriginalVxdChannelRowStart+
        static_cast<std::size_t>(channel)*kOriginalVxdChannelRowStride;
    if(chan+kOriginalVxdChannelRowStride>state.size())return std::nullopt;
    std::array<const OriginalSf2VxdSnapshotVoice*,
               kOriginalCreativeMusicVoices> by_voice{};
    // Authenticate physical mapping FIRST. The array belongs to the
    // caller's existing source-backed NoteOn lifetime, not a new
    // synthetic allocator; a six-layer NoteOn is not a 30-voice limit.
    for(const auto& layer:assigned) {
        const auto physical=layer.assigned_voice.physical_voice;
        if(physical>=kOriginalCreativeMusicVoices ||
           layer.original_midi_channel>=kOriginalCreativeMidiChannels ||
           layer.assigned_voice.played_midi_key>127u ||
           !layer.assigned_voice.source_original_voice_verified ||
           by_voice[physical]!=nullptr)return std::nullopt;
        by_voice[physical]=&layer;
    }
    OriginalSf2Cc64DeferredScan out{};
    // Pedal-down writes only the original per-CHANNEL sustain byte.
    // Both up and down legitimately occur with zero active voices.
    if(cc_value>=64u)return out;
    // Original LCOD+0x656D pedal-up scans physical voices; do NOT
    // inspect only the most recently played key or the latest six
    // note-start descriptors. Each deferred channel:FF marker must
    // correspond to a verified voice from this current lifetime.
    const auto deferred=static_cast<std::uint16_t>(
        (static_cast<unsigned>(channel)<<8u)|0xffu);
    for(std::size_t physical=0;physical<kOriginalCreativeMusicVoices;
        ++physical) {
        const auto base=kOriginalVxdVoiceRowStart+
            physical*kOriginalVxdVoiceRowStride;
        if(base+kOriginalVxdVoiceRowStride>state.size())
            return std::nullopt;
        if(le16(state,base)!=deferred)continue;
        const auto* source=by_voice[physical];
        if(!source || source->original_midi_channel!=channel)
            return std::nullopt;
        const auto rates=candidateOriginalSf2VerifiedReleaseRates(
            state.subspan(base,kOriginalVxdVoiceRowStride),*source);
        const auto end=le32(state,base+6u);
        if(!rates || end>0xfffffbu)return std::nullopt;
        OriginalVxdNoteReleaseEvidence e{};
        e.original_key_channel_match=true;
        e.original_voice_record_verified=true;
        e.current_chip_registers_verified=true;
        e.original_sustain_held=false;
        e.encoded_mod_release=rates->modulation;
        e.encoded_volume_release=rates->volume;
        e.stored_end_word=end;
        const auto proof=candidateOriginalVxdMidiNoteRelease(
            static_cast<std::uint8_t>(physical),e);
        if(!proof || proof->count<4u)return std::nullopt;
        out.voices[out.count++]={base,
            static_cast<std::uint8_t>(physical),e};
    }
    return out;
}
std::optional<OriginalSf2SourceCc64Result>
applyOriginalSf2MidiCc64FromVxdState(
    awe64::Chip& chip,const std::span<std::uint8_t> state,
    const std::span<const OriginalSf2VxdSnapshotVoice> assigned,
    const std::uint8_t channel,const std::uint8_t cc_value,
    const bool verified) noexcept {
    if(!chip.ready())return std::nullopt;
    const auto scan=candidateOriginalSf2Cc64DeferredScan(
        std::span<const std::uint8_t>(state.data(),state.size()),
        assigned,channel,cc_value,verified);
    if(!scan)return std::nullopt;
    const auto chan=kOriginalVxdChannelRowStart+
        static_cast<std::size_t>(channel)*kOriginalVxdChannelRowStride;
    OriginalSf2SourceCc64Result result{};
    result.sustain_enabled=cc_value>=64u;
    // All thirty physical marker rows, including any supplied older
    // NoteOn voices, were validated before this first state mutation.
    state[chan+4u]=result.sustain_enabled?1u:0u;
    for(std::size_t i=0;i<scan->count;++i) {
        const auto& pending=scan->voices[i];
        const auto writes=releaseOriginalVoice(
            chip,state,pending.state_row_offset,pending.physical_voice,
            pending.release);
        if(!writes)return std::nullopt;
        result.typed_port_writes+=*writes;
        ++result.released_deferred_voices;
    }
    return result;
}
} // namespace awe64::creative
