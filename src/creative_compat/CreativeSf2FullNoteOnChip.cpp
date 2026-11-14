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
bool applyOriginalCreativeSf2StopSelectedIdleVoice(
    awe64::Chip& chip,const OriginalWin95MonoSelection& selected) noexcept {
    if(!chip.ready() ||
       selected.reason!=OriginalWin95MonoSelectionReason::CompletelyIdle ||
       selected.physical_voice>=kOriginalCreativeMusicVoices)return false;
    const auto dc=candidateOriginalWin95SelectedVoiceDcysusvWrites(selected);
    const auto vt=candidateOriginalWin95AssignedVoiceVtftWrites(selected.physical_voice);
    if(!dc || !vt || !validateOriginalCreativeVxdVoicePorts(*dc) ||
       !validateOriginalCreativeVxdVoicePorts(*vt))return false;
    // SOURCE x86: E22 A0|voice, A20 00FF, E22 60|voice,
    // 620 FFFF, 622 0000. The first E22 60|voice already came from
    // the live original VTFT selector, so never duplicate it here.
    return applyOriginalCreativeVxdVoicePorts(chip,*dc) &&
           applyOriginalCreativeVxdVoicePorts(chip,*vt);
}

bool applyOriginalCreativeSf2MainPortsAfterStop(
    awe64::Chip& chip,
    const std::span<const OriginalSf2FullNoteOnVoice> voices) noexcept {
    if(!chip.ready())return false;
    const auto prepared=candidateOriginalSf2FullNoteOnPorts(voices);
    if(!prepared ||
       prepared->count!=voices.size()*kOriginalSf2OneVoiceStartWrites)
        return false;
    const auto& outs=prepared->out;
    // Validate the COMPLETE original 61-command source plan, including
    // allocator prologues, before emitting only the source main writes.
    for(std::size_t i=0u;i<voices.size();++i) {
        const auto at=i*5u;
        const auto main=voices.size()*5u+i*kOriginalSf2OneVoiceMainWrites;
        if(outs[at].port!=0xe22u || outs[at].access_width_bits!=8u ||
           outs[at].value!=(0x60u|voices[i].physical_voice) ||
           !validateOriginalCreativeVxdVoicePorts(
               std::span<const OriginalWin95VxdPortWrite>(
                   outs.data()+at+1u,4u)) ||
           !validateOriginalCreativeVxdVoicePorts(
               std::span<const OriginalWin95VxdPortWrite>(
                   outs.data()+main,kOriginalSf2OneVoiceMainWrites)))
            return false;
    }
    for(std::size_t i=0u;i<voices.size();++i) {
        const auto at=voices.size()*5u+i*kOriginalSf2OneVoiceMainWrites;
        if(!applyOriginalCreativeVxdVoicePorts(
               chip,std::span<const OriginalWin95VxdPortWrite>(
                   outs.data()+at,kOriginalSf2OneVoiceMainWrites)))
            return false;
    }
    return true;
}
} // namespace awe64::creative
