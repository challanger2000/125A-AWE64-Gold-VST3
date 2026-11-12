#include "CreativeBoundVoiceController.h"
namespace awe64::creative {
namespace {
MidiMessage originalNote(const std::uint8_t channel,const std::uint8_t key,
                         const std::uint8_t velocity) noexcept {
    MidiMessage out{};
    out.kind=MidiKind::NoteOn;
    out.channel=channel;
    out.data1=key;out.data2=velocity;
    out.raw_short=static_cast<std::uint32_t>(0x90u|channel)
        |(static_cast<std::uint32_t>(key)<<8)
        |(static_cast<std::uint32_t>(velocity)<<16);
    return out;
}
bool validPlanAddress(const SbkRomAddress& a) noexcept {
    return a.start_word<a.end_word &&
           a.start_word<=a.loop_start_word &&
           a.loop_start_word<a.loop_end_word &&
           a.loop_end_word<=a.end_word;
}
bool append(BoundVoiceActions& out,const BoundVoiceAction& a) noexcept {
    if(out.count>=out.actions.size())return false;
    out.actions[out.count++]=a;return true;
}
}
std::optional<OriginalRomInitialRegisterCandidates>
CreativeBoundVoiceController::bind(
    const CreativeRomNotePlan& note,const std::size_t index,
    const std::uint8_t physical,const OriginalCreativeIfatnTerms& terms) noexcept {
    if(physical>=voices_.size() || voices_[physical].occupied ||
       index>=note.count || note.count>kMaxCreative1MgmLayers ||
       note.channel>=16 || note.key>127 || note.velocity==0 ||
       note.velocity>127 || note.program!=state_.channel(note.channel).program ||
       terms.bank!=note.original_sbk_bank ||
       !terms.volume.original_1mgm_rom_sample ||
       !validPlanAddress(note.layers[index].address))return std::nullopt;
    const auto& layer=note.layers[index].metadata;
    if(layer.bank!=note.original_sbk_bank ||
       layer.program!=note.program || layer.key!=note.key ||
       layer.velocity!=note.velocity ||
       note.layers[index].address.sample_id!=layer.rom_sample_selector)
       return std::nullopt;
    const auto pitch=candidateOriginalRomIpWrites(note,index,physical,state_);
    const auto filter=candidateOriginalCreativeIfatnWrites(
        originalNote(note.channel,note.key,note.velocity),physical,state_,terms);
    if(!pitch || !filter)return std::nullopt;
    auto& voice=voices_[physical];
    voice.occupied=true;voice.key_down=true;voice.release_requested=false;
    voice.channel=note.channel;voice.key=note.key;voice.velocity=note.velocity;
    voice.layer=layer;voice.terms=terms;
    return OriginalRomInitialRegisterCandidates{*pitch,*filter};
}
std::optional<OriginalRomInitialRegisterCandidates>
CreativeBoundVoiceController::bindWithOriginalSbkCutoff(
    const CreativeRomNotePlan& note,const std::size_t index,
    const std::uint8_t voice,
    const OriginalCreativeIfatnTerms& verified_terms) noexcept {
    if(index>=note.count || note.count>kMaxCreative1MgmLayers)
        return std::nullopt;
    const auto& layer=note.layers[index].metadata;
    const auto cutoff=layer.candidateOriginal1MgmInstrumentCutoff();
    if(!cutoff)return std::nullopt;
    auto terms=verified_terms;
    terms.initial_cutoff=*cutoff;
    // Use the EXISTING single native bind logic and controls. No parallel
    // voice allocator or second chip register writer is introduced.
    return bind(note,index,voice,terms);
}
std::optional<OriginalRomAllocatedRegisterCandidates>
CreativeBoundVoiceController::bindFirstOriginalIdleMono(
    const CreativeRomNotePlan& plan, const std::size_t layer,
    const OriginalCreativeIfatnTerms& terms,
    const std::array<OriginalVxdIdleVoiceProbe,30>& probes,
    const std::size_t available) noexcept {
    std::array<bool,30> busy{};
    for(std::size_t i=0;i<voices_.size();++i)busy[i]=voices_[i].occupied;
    const auto voice=candidateOriginalWin95IdleMonoVoice(probes,busy,available);
    if(!voice)return std::nullopt;
    const auto originalReset=candidateOriginalWin95SelectedVoiceDcysusvWrites(
        OriginalWin95MonoSelection{
            *voice,OriginalWin95MonoSelectionReason::CompletelyIdle,0});
    if(!originalReset)return std::nullopt;
    const auto originalVtft=candidateOriginalWin95AssignedVoiceVtftWrites(*voice);
    if(!originalVtft)return std::nullopt;
    const auto initial=bind(plan,layer,*voice,terms);
    if(!initial)return std::nullopt;
    return OriginalRomAllocatedRegisterCandidates{
        *voice,*originalReset,*originalVtft,*initial};
}
std::optional<BoundVoiceActions> CreativeBoundVoiceController::process(
    const MidiMessage& message) noexcept {
    if(message.channel>=16 || message.data1>127 || message.data2>127)
        return std::nullopt;
    MidiState updated=state_;
    updated.apply(message);
    BoundVoiceActions out{};
    // First evaluate ALL newly generated register words. On failure,
    // preserve the previous MIDI and voice state (transactional update).
    if(message.kind==MidiKind::PitchBend ||
       (message.kind==MidiKind::Controller &&
        (message.data1==7 || message.data1==11 || message.data1==121))) {
        for(std::size_t i=0;i<voices_.size();++i) {
            const auto& v=voices_[i];
            if(!v.occupied || v.channel!=message.channel || v.release_requested)
                continue;
            if(message.kind==MidiKind::PitchBend) {
                const auto p=candidateOriginalRomIp(v.layer,v.key,
                    updated.channel(v.channel).pitch_bend14);
                if(!p)return std::nullopt;
                if(!append(out,BoundVoiceAction{
                    BoundVoiceActionKind::UpdatePitch,static_cast<std::uint8_t>(i),
                    {{{0xE22u,static_cast<std::uint16_t>(i)},
                      {0xE20u,p->ip}}}}))return std::nullopt;
            } else {
                const auto writes=candidateOriginalCreativeIfatnWrites(
                    originalNote(v.channel,v.key,v.velocity),
                    static_cast<std::uint8_t>(i),updated,v.terms);
                if(!writes)return std::nullopt;
                if(!append(out,BoundVoiceAction{
                    BoundVoiceActionKind::UpdateIfatn,static_cast<std::uint8_t>(i),
                    *writes}))return std::nullopt;
            }
        }
    }
    // Release/kill are intentionally semantic: we have not fabricated the
    // original Creative DCYSUSV register value or exact chip clock.
    for(std::size_t i=0;i<voices_.size();++i) {
        auto& v=voices_[i];
        if(!v.occupied || v.channel!=message.channel)continue;
        const bool noteOff=message.kind==MidiKind::NoteOff &&
                           message.data1==v.key;
        const bool allNotesOff=message.kind==MidiKind::Controller &&
                               message.data1==123;
        const bool allSoundOff=message.kind==MidiKind::Controller &&
                               message.data1==120;
        const bool sustainReleased=message.kind==MidiKind::Controller &&
            (message.data1==64 || message.data1==121) &&
            state_.channel(message.channel).sustain_cc64>=64 &&
            updated.channel(message.channel).sustain_cc64<64;
        if(allSoundOff) {
            if(!append(out,BoundVoiceAction{
                BoundVoiceActionKind::RequestImmediateKill,
                static_cast<std::uint8_t>(i),{}}))return std::nullopt;
            v=Voice{}; // software slot becomes available after explicit kill
        } else if(!v.release_requested) {
            if(noteOff || allNotesOff)v.key_down=false;
            if(!v.key_down && updated.channel(v.channel).sustain_cc64<64 &&
               (noteOff || allNotesOff || sustainReleased)) {
                if(!append(out,BoundVoiceAction{
                    BoundVoiceActionKind::RequestEnvelopeRelease,
                    static_cast<std::uint8_t>(i),{}}))return std::nullopt;
                v.release_requested=true;
            }
        }
    }
    state_=updated;
    return out;
}
bool CreativeBoundVoiceController::clearPhysicalVoice(const std::uint8_t voice) noexcept {
    if(voice>=voices_.size() || !voices_[voice].occupied)return false;
    voices_[voice]=Voice{};return true;
}
std::size_t CreativeBoundVoiceController::occupiedVoiceCount() const noexcept {
    std::size_t total=0;
    for(const auto& v:voices_)if(v.occupied)++total;
    return total;
}
} // namespace awe64::creative
