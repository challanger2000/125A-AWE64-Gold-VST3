#include "CreativeSf2VxdSourceRelease.h"
#include "Chip.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
using namespace awe64::creative;
#define CHECK(x) do {if(!(x)){std::cerr<<"P002_SOURCE_NOTEOFF_FAIL line="<<__LINE__<<" "<<#x<<"\n";return 1;}}while(0)
namespace {
void set16(std::array<std::uint8_t,kOriginalVxdMinDeviceImage>& s,
           std::size_t p,std::uint16_t w){
    s[p]=static_cast<std::uint8_t>(w);
    s[p+1u]=static_cast<std::uint8_t>(w>>8u);
}
void set32(std::array<std::uint8_t,kOriginalVxdMinDeviceImage>& s,
           std::size_t p,std::uint32_t w){
    for(unsigned i=0;i<4u;++i)s[p+i]=static_cast<std::uint8_t>(w>>(i*8u));
}
OriginalSf2VxdSnapshotVoice layer(std::uint8_t id){
    OriginalSf2VxdSnapshotVoice result{};
    result.original_midi_channel=0;
    result.assigned_voice.physical_voice=id;
    result.assigned_voice.played_midi_key=60;
    result.assigned_voice.source_original_voice_verified=true;
    return result;
}
}
int main(){
    std::array<std::uint8_t,kOriginalVxdMinDeviceImage> state{};
    set32(state,kOriginalVxdRomIdOffset,0x4d474d31u);
    constexpr std::size_t v0=kOriginalVxdVoiceRowStart;
    constexpr std::size_t v3=kOriginalVxdVoiceRowStart+
                             3u*kOriginalVxdVoiceRowStride;
    for(const auto p:{v0,v3}){
        set16(state,p,60u);
        state[p+0x16u]=92u; // source-record volume release
        state[p+0x15u]=45u; // source-record modulation release
    }
    auto notes=std::array<OriginalSf2VxdSnapshotVoice,2>{{layer(0),layer(3)}};
    for(auto& n:notes) {
        n.assigned_voice.verified_after_start_record[0x8cu]=1u;
        n.assigned_voice.verified_after_start_record[0x3cu]=127u;
        n.assigned_voice.verified_after_start_record[0x4cu]=41u;
    }
    CHECK(candidateOriginalSf2StoreNoteOnSourceReleaseState(state,true,notes));
    CHECK(state[v0]==60u && state[v3]==60u &&
          state[v0+0x15u]==127u && state[v3+0x15u]==127u &&
          state[v0+0x16u]==41u && state[v3+0x16u]==41u);
    // Reject entire batch before mutating any note marker when a
    // later layer has a corrupt encoded rate or duplicate voice.
    auto bad=notes;
    bad[1].assigned_voice.verified_after_start_record[0x4cu]=128u;
    const auto unchanged=state;
    CHECK(!candidateOriginalSf2StoreNoteOnSourceReleaseState(state,true,bad));
    CHECK(state==unchanged);
    // Source original sustain = CHANNEL+4. Voice+4 stores a separate
    // instrument amplitude term and MUST NOT gate NoteOff.
    const auto ch=kOriginalVxdChannelRowStart;
    state[v0+4u]=1u;
    state[v3+4u]=1u;
    state[ch+4u]=0u;
    const auto wrongKey=state;
    awe64::Chip unopened{};
    CHECK(!applyOriginalSf2MidiNoteOffFromVxdState(
        unopened,state,notes,0u,60u,true));
    CHECK(state==wrongKey); // no mutation when no actual source chip
    // The genuine pinned chip ROM must be opened for I/O: additionally
    // covered with a real ROM in exported Windows VST3 diagnostic host.
    CHECK(!applyOriginalSf2MidiNoteOffFromVxdState(
        unopened,state,notes,0u,61u,true));
    CHECK(!applyOriginalSf2MidiNoteOffFromVxdState(
        unopened,state,notes,16u,60u,true));
    CHECK(!applyOriginalSf2MidiNoteOffFromVxdState(
        unopened,state,notes,0u,60u,false));
    CHECK(!applyOriginalSf2MidiCc64FromVxdState(
        unopened,state,notes,0u,127u,true));
    CHECK(!applyOriginalSf2MidiCc64FromVxdState(
        unopened,state,notes,0u,0u,true));
    CHECK(!applyOriginalSf2MidiCc64FromVxdState(
        unopened,state,notes,16u,0u,true));
    // Existing (already source proven) port-plan helper handles exact
    // rate byte values, sustain deferral, and optional CSL/PSST updates.
    OriginalVxdNoteReleaseEvidence e{};
    e.original_key_channel_match=true;
    e.original_voice_record_verified=true;
    e.current_chip_registers_verified=true;
    e.encoded_volume_release=state[v0+0x16u];
    e.encoded_mod_release=state[v0+0x15u];
    const auto cmd=candidateOriginalVxdMidiNoteRelease(0,e);
    CHECK(cmd && cmd->count==4u);
    CHECK(cmd->writes[1].port==0xa20u &&
          cmd->writes[1].value==0x8029u); // GM0 original gen38=41
    CHECK(cmd->writes[3].port==0xa20u &&
          cmd->writes[3].value==0x807fu); // GM0 original gen30=127
    e.original_sustain_held=true;
    const auto held=candidateOriginalVxdMidiNoteRelease(0,e);
    CHECK(held && held->deferred_by_sustain &&
          held->count==0u);
    e.original_sustain_held=false;
    e.stored_end_word=0x152feu;
    e.observed_chip_csl=0x72001234u;
    e.observed_chip_psst=0x63005678u;
    const auto rewrite=candidateOriginalVxdMidiNoteRelease(0,e);
    CHECK(rewrite && rewrite->count==8u);
    CHECK(rewrite->writes[5].value==0x72015302u);
    CHECK(rewrite->writes[7].value==0x630152feu);
    std::cout<<"P002_SOURCE_ORIGINAL_VXD_NOTEOFF_EVIDENCE_PASS"
             <<" two_original_voice_record_byte_rates=TRUE"
             <<" original_GM0_gen38_8029_gen30_807f=TRUE"
             <<" original_release_from_SF2_noteon_descriptors=TRUE"
             <<" sustain_at_MIDI_channel_plus4_NOT_voice_plus4=TRUE"
             <<" original_CC64_656D_pedal_off_release_helper_PRESENT=TRUE"
             <<" original_sustain_defer=TRUE"
             <<" original_CSL_PSST_preserved_high_byte=TRUE"
             <<" actual_exported_VST3_PCM=TEST_SEPARATELY"
             <<" original_Win95_runtime_PCMPARITY=NOT_PROVEN\n";
    return 0;
}
