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
        if(base+0x20u>read.size() || le16(read,base)!=expected ||
           read[base+0x15u]>127u || read[base+0x16u]>127u)return std::nullopt;
        seen|=1u<<physical;
        auto& e=pending[count].original;
        e.original_key_channel_match=true;
        e.original_voice_record_verified=true;
        e.original_sustain_held=read[base+4u]!=0u;
        e.encoded_volume_release=read[base+0x16u];
        e.encoded_mod_release=read[base+0x15u];
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
        // Original source releases envelopes before reading current
        // CSL/PSST for a conditional end-loop rewrite. This preserves
        // the same order of IO as Win95, not a retrospective register
        // snapshot following all operations.
        auto cmd=candidateOriginalVxdMidiNoteRelease(item.physical,e);
        if(!cmd || !writePairs(chip,*cmd,0u,4u))return std::nullopt;
        result.typed_port_writes+=4u;
        if(e.stored_end_word!=0u) {
            // Original VXD LCOD+0x613B reads CSL first, PSST second.
            e.observed_chip_csl=readRealChipData0(chip,item.physical,7u);
            e.observed_chip_psst=readRealChipData0(chip,item.physical,6u);
            cmd=candidateOriginalVxdMidiNoteRelease(item.physical,e);
            if(!cmd || cmd->count!=8u ||
               !writePairs(chip,*cmd,4u,4u))return std::nullopt;
            result.typed_port_writes+=4u;
            const auto end=e.stored_end_word;
            for(unsigned b=0;b<4u;++b)
                state[item.base+0x0au+b]=
                    static_cast<std::uint8_t>(end>>(8u*b));
        }
        // Source original final +0x617C marks the freed voice 0x0FFE.
        state[item.base]=0xfeu;
        state[item.base+1u]=0x0fu;
    }
    return result;
}
} // namespace awe64::creative
