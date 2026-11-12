#include "CreativeSf2FullNoteOnChip.h"
#include <cstddef>
#include <cstdint>
#include <span>

namespace awe64::creative {
bool applyOriginalCreativeSf2FullNoteOnPorts(
    awe64::Chip& chip,
    const std::span<const OriginalSf2FullNoteOnVoice> voices) noexcept {
    if(!chip.ready())return false;
    const auto prepared=candidateOriginalSf2FullNoteOnPorts(voices);
    if(!prepared ||
       prepared->count!=voices.size()*kOriginalSf2OneVoiceStartWrites)
        return false;
    const auto& ports=prepared->out;

    // First 5 source events for each allocated physical chip voice:
    // a genuine pointer-only E22, then two original E22/data pairs.
    // Preflight ALL the pairs and ALL 56-source-OUT voice-start parts
    // before issuing ANY I/O, including a malformed later voice.
    for(std::size_t i=0;i<voices.size();++i) {
        const auto pos=5u*i;
        const auto p=ports[pos];
        if(p.port!=0xe22u || p.access_width_bits!=8u ||
           p.value!=(0x60u|voices[i].physical_voice))
            return false;
        const std::span<const OriginalWin95VxdPortWrite> pairs(
            ports.data()+pos+1u,4u);
        if(!validateOriginalCreativeVxdVoicePorts(pairs))return false;
    }
    for(std::size_t i=0;i<voices.size();++i) {
        const std::size_t pos=voices.size()*5u+
            i*kOriginalSf2OneVoiceMainWrites;
        const std::span<const OriginalWin95VxdPortWrite> full(
            ports.data()+pos,kOriginalSf2OneVoiceMainWrites);
        if(!validateOriginalCreativeVxdVoicePorts(full))return false;
    }
    // Original multi-voice ordering: ALL five-write allocator prologues,
    // then 56 E22/data OUTs per voice, never a fake per-voice 61/61
    // contiguous replay when the original had two matched SF2 layers.
    for(std::size_t i=0;i<voices.size();++i) {
        const auto pos=5u*i;
        chip.writePort(0xe22u,
                       static_cast<std::uint16_t>(ports[pos].value));
        const std::span<const OriginalWin95VxdPortWrite> pairs(
            ports.data()+pos+1u,4u);
        if(!applyOriginalCreativeVxdVoicePorts(chip,pairs))return false;
    }
    for(std::size_t i=0;i<voices.size();++i) {
        const std::size_t pos=voices.size()*5u+
            i*kOriginalSf2OneVoiceMainWrites;
        const std::span<const OriginalWin95VxdPortWrite> full(
            ports.data()+pos,kOriginalSf2OneVoiceMainWrites);
        if(!applyOriginalCreativeVxdVoicePorts(chip,full))return false;
    }
    return true;
}
} // namespace awe64::creative
