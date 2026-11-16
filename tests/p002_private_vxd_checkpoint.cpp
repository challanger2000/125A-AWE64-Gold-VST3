// P002 native original-Creative note speculation must restore VXD state
// as well as the EMU8000 chip before rejecting ANY failed original parity
// check. No actual Creative ROM, bank, driver or reference trace here.
#include "P002PrivateRomBank.h"
#include <cstdint>
#include <iostream>

namespace awe64::vst3 {
struct P002PrivateRomBankCheckpointProbe {
    static void seed(P002PrivateRomBank& bank) {
        bank.ready_=true;
        bank.state_[0x42u]=0xfeu;
        bank.state_[0x43u]=0x0fu;
        bank.state_[0x90u]=0x29u;
        bank.held_keys_[60]=false;
        bank.held_keys_[62]=true;
    }
    static void mutate(P002PrivateRomBank& bank) {
        bank.ready_=false; // failed original voice ownership check
        bank.state_[0x42u]=0x3cu;
        bank.state_[0x43u]=0x00u;
        bank.state_[0x90u]=0xffu;
        bank.held_keys_[60]=true;
        bank.held_keys_[62]=false;
    }
    static void seedSyntheticSparsePresetState(P002PrivateRomBank& bank) {
        bank.clear();
        bank.ready_=true;
        bank.bank_number_=0u;
        bank.program_=0u;
        awe64::creative::Sf2PresetIdentity first{};
        first.bank=0u;first.program=0u;first.original_phdr_row=0u;
        awe64::creative::Sf2PresetIdentity second{};
        second.bank=0u;second.program=77u;second.original_phdr_row=76u;
        awe64::creative::Sf2PresetIdentity drum{};
        drum.bank=128u;drum.program=2u;drum.original_phdr_row=77u;
        bank.presets_={first,second,drum};
    }
    static void seedMidiCheckpoint(P002PrivateRomBank& bank) {
        bank.clear();
        bank.ready_=true;
        bank.bank_number_=0u;
        bank.channel_=0u;
        bank.program_=77u;
        awe64::creative::MidiMessage program{};
        program.kind=awe64::creative::MidiKind::Program;
        program.channel=0u;program.data1=77u;
        bank.midi_.apply(program);
        awe64::creative::MidiMessage volume{};
        volume.kind=awe64::creative::MidiKind::Controller;
        volume.channel=0u;volume.data1=7u;volume.data2=93u;
        bank.midi_.apply(volume);
    }
    static void mutateMidiCheckpoint(P002PrivateRomBank& bank) {
        bank.program_=9u;
        bank.bank_number_=128u;
        bank.channel_=1u;
        awe64::creative::MidiMessage program{};
        program.kind=awe64::creative::MidiKind::Program;
        program.channel=0u;program.data1=9u;
        bank.midi_.apply(program);
        awe64::creative::MidiMessage volume{};
        volume.kind=awe64::creative::MidiKind::Controller;
        volume.channel=0u;volume.data1=7u;volume.data2=12u;
        bank.midi_.apply(volume);
    }
    static bool originalMidiCheckpoint(const P002PrivateRomBank& bank) {
        const auto& ch=bank.midi_.channel(0u);
        return bank.ready_ && bank.program_==77u &&
               bank.bank_number_==0u && bank.channel_==0u &&
               ch.program==77u && ch.cc7_volume==93u;
    }
    static bool mutatedMidiCheckpoint(const P002PrivateRomBank& bank) {
        const auto& ch=bank.midi_.channel(0u);
        return bank.ready_ && bank.program_==9u &&
               bank.bank_number_==128u && bank.channel_==1u &&
               ch.program==9u && ch.cc7_volume==12u;
    }
    static bool original(const P002PrivateRomBank& bank) {
        return bank.ready_ &&
               bank.state_[0x42u]==0xfeu &&
               bank.state_[0x43u]==0x0fu &&
               bank.state_[0x90u]==0x29u &&
               !bank.held_keys_[60] && bank.held_keys_[62] &&
               bank.ownership_.trackedPhysicalVoices()==0u;
    }
    static bool seedSourceVerifiedSyntheticOwner(P002PrivateRomBank& bank) {
        using namespace awe64::creative;
        bank.clear();
        bank.ready_=true;
        constexpr auto base=kOriginalVxdVoiceRowStart;
        bank.state_[base]=60u;bank.state_[base+1u]=0u;
        bank.state_[base+0x15u]=45u; // real VXD row layout only
        bank.state_[base+0x16u]=92u;
        OriginalSf2CompiledRomBankNote compiled{};
        compiled.voice_count=1u;
        auto& voice=compiled.assignments[0];
        voice.original_midi_channel=0u;
        voice.assigned_voice.physical_voice=0u;
        voice.assigned_voice.played_midi_key=60u;
        voice.assigned_voice.source_original_voice_verified=true;
        auto& descriptor=voice.assigned_voice.verified_after_start_record;
        descriptor[0x8cu]=1u;
        descriptor[0x8du]=0u;
        descriptor[0x3cu]=45u;
        descriptor[0x3du]=0u;
        descriptor[0x4cu]=92u;
        descriptor[0x4du]=0u;
        bank.held_keys_[60u]=true;
        return bank.ownership_.recordCompiledNoteOn(compiled,bank.state_);
    }
    static bool owned(const P002PrivateRomBank& bank) {
        constexpr auto base=awe64::creative::kOriginalVxdVoiceRowStart;
        return bank.ready_ && bank.held_keys_[60u] &&
               bank.state_[base]==60u &&
               bank.state_[base+0x15u]==45u &&
               bank.state_[base+0x16u]==92u &&
               bank.ownership_.trackedPhysicalVoices()==1u;
    }
    static void loseSourceOwner(P002PrivateRomBank& bank) {
        constexpr auto base=awe64::creative::kOriginalVxdVoiceRowStart;
        bank.state_[base]=0xfeu;
        bank.state_[base+1u]=0x0fu;
        bank.held_keys_[60u]=false;
        bank.ownership_={};
        bank.ready_=false;
    }
    static bool mutated(const P002PrivateRomBank& bank) {
        return !bank.ready_ &&
               bank.state_[0x42u]==0x3cu &&
               bank.state_[0x43u]==0x00u &&
               bank.state_[0x90u]==0xffu &&
               bank.held_keys_[60] && !bank.held_keys_[62];
    }
};
}
#define CHECK(x) do { if(!(x)) { \
    std::cerr<<"P002_NATIVE_VXD_CHECKPOINT_FAIL line="<<__LINE__ \
    <<" "<<#x<<"\\n"; return 1; }}while(false)
int main() {
    using awe64::vst3::P002PrivateRomBank;
    using Probe=awe64::vst3::P002PrivateRomBankCheckpointProbe;
    P002PrivateRomBank bank;
    CHECK(!bank.beginNativeNoteCheckpoint());
    CHECK(!bank.nativeNoteCheckpointActive());
    Probe::seed(bank);
    CHECK(Probe::original(bank));
    CHECK(bank.beginNativeNoteCheckpoint());
    CHECK(bank.nativeNoteCheckpointActive());
    CHECK(!bank.beginNativeNoteCheckpoint()); // cannot overwrite checkpoint
    Probe::mutate(bank);
    CHECK(Probe::mutated(bank));
    bank.rollbackNativeNoteCheckpoint();
    CHECK(!bank.nativeNoteCheckpointActive());
    CHECK(Probe::original(bank)); // failed event MUST leave no held ghost key
    bank.rollbackNativeNoteCheckpoint(); // idempotent; no state clobber
    CHECK(Probe::original(bank));

    CHECK(bank.beginNativeNoteCheckpoint());
    Probe::mutate(bank);
    bank.commitNativeNoteCheckpoint();
    CHECK(!bank.nativeNoteCheckpointActive());
    CHECK(Probe::mutated(bank)); // successful event must persist changes
    bank.rollbackNativeNoteCheckpoint(); // after commit must be NO-OP
    CHECK(Probe::mutated(bank));

    // Failed source-clocked MIDI transaction rolls back the FULL
    // logical channel state, not only the VXD voice bytes. A stale CC7
    // or ProgramChange would alter the very next note after an error.
    P002PrivateRomBank midiRollback;
    Probe::seedMidiCheckpoint(midiRollback);
    CHECK(Probe::originalMidiCheckpoint(midiRollback));
    CHECK(midiRollback.beginNativeNoteCheckpoint());
    Probe::mutateMidiCheckpoint(midiRollback);
    CHECK(Probe::mutatedMidiCheckpoint(midiRollback));
    midiRollback.rollbackNativeNoteCheckpoint();
    CHECK(Probe::originalMidiCheckpoint(midiRollback));
    CHECK(!midiRollback.nativeNoteCheckpointActive());
    CHECK(midiRollback.beginNativeNoteCheckpoint());
    Probe::mutateMidiCheckpoint(midiRollback);
    midiRollback.commitNativeNoteCheckpoint();
    midiRollback.rollbackNativeNoteCheckpoint();
    CHECK(Probe::mutatedMidiCheckpoint(midiRollback));
    midiRollback.clear();
    CHECK(!midiRollback.ready());

    // Replay the original source ownership preflight with a SYNTHETIC
    // descriptor whose original 0x20 VXD row is internally consistent.
    // A failed original-vs-native OUT comparison must NOT lose an
    // already tracked hardware voice or incorrectly clear its held key.
    P002PrivateRomBank ledger;
    CHECK(Probe::seedSourceVerifiedSyntheticOwner(ledger));
    CHECK(Probe::owned(ledger));
    CHECK(ledger.beginNativeNoteCheckpoint());
    Probe::loseSourceOwner(ledger);
    CHECK(ledger.trackedVoices()==0u);
    ledger.rollbackNativeNoteCheckpoint();
    CHECK(Probe::owned(ledger));
    CHECK(!ledger.nativeNoteCheckpointActive());
    CHECK(ledger.beginNativeNoteCheckpoint());
    Probe::loseSourceOwner(ledger);
    ledger.commitNativeNoteCheckpoint();
    CHECK(ledger.trackedVoices()==0u && !ledger.ready());
    ledger.rollbackNativeNoteCheckpoint();
    CHECK(ledger.trackedVoices()==0u); // cannot resurrect committed voices

    // Actual production P002PrivateRomBank::programChange must be
    // transactional: the original 77-preset owner's bank does NOT have
    // MIDI program 63. An absent patch must never silently select it,
    // nor may the separate drum bank alias ordinary bank zero.
    P002PrivateRomBank sparseBank;
    Probe::seedSyntheticSparsePresetState(sparseBank);
    CHECK(sparseBank.presetCount()==3u);
    CHECK(sparseBank.presetAt(0)!=nullptr);
    CHECK(sparseBank.presetAt(1)->program==77u);
    CHECK(sparseBank.presetAt(1)->original_phdr_row==76u);
    CHECK(sparseBank.presetAt(2)->bank==128u);
    CHECK(sparseBank.presetAt(3)==nullptr);
    CHECK(sparseBank.programChange(77u));
    CHECK(sparseBank.currentProgram()==77u);
    CHECK(!sparseBank.programChange(63u));
    CHECK(sparseBank.currentProgram()==77u);
    CHECK(!sparseBank.programChange(2u));
    CHECK(sparseBank.currentProgram()==77u);
    CHECK(!sparseBank.programChange(128u));
    CHECK(sparseBank.currentProgram()==77u);
    CHECK(sparseBank.programChange(0u));
    CHECK(sparseBank.currentProgram()==0u);
    // Actual PHDR bank switch, without guessed Win95 CC bank formulas.
    // Missing bank is a strict no-op; bank128 retains its own program.
    CHECK(sparseBank.currentBank()==0u);
    CHECK(!sparseBank.selectExactSf2Bank(1u));
    CHECK(sparseBank.currentBank()==0u &&
          sparseBank.currentProgram()==0u);
    CHECK(sparseBank.selectExactSf2Bank(128u));
    CHECK(sparseBank.currentBank()==128u &&
          sparseBank.currentProgram()==2u);
    CHECK(!sparseBank.programChange(77u));
    CHECK(sparseBank.currentProgram()==2u);
    CHECK(sparseBank.selectExactSf2Bank(0u));
    CHECK(sparseBank.currentBank()==0u &&
          sparseBank.currentProgram()==0u);
    CHECK(sparseBank.programChange(77u));
    CHECK(sparseBank.selectExactSf2Bank(0u));
    CHECK(sparseBank.currentProgram()==77u);
    CHECK(!sparseBank.selectExactSf2Bank(129u));
    CHECK(sparseBank.currentBank()==0u &&
          sparseBank.currentProgram()==77u);
    CHECK(sparseBank.beginNativeNoteCheckpoint());
    CHECK(sparseBank.selectExactSf2Bank(128u));
    CHECK(sparseBank.currentBank()==128u);
    sparseBank.rollbackNativeNoteCheckpoint();
    CHECK(sparseBank.currentBank()==0u &&
          sparseBank.currentProgram()==77u);
    // Real VST3 getState/setState recall: only existing PHDR identities
    // may commit. Invalid bank/program/range must leave every MIDI
    // controller and program exactly unchanged.
    CHECK(!sparseBank.restoreSelection(128u,77u,50u,20u,30u));
    CHECK(!sparseBank.restoreSelection(9u,0u,50u,20u,30u));
    CHECK(!sparseBank.restoreSelection(0u,77u,128u,20u,30u));
    CHECK(sparseBank.currentBank()==0u &&
          sparseBank.currentProgram()==77u &&
          sparseBank.currentCc7()==100u &&
          sparseBank.currentCc10()==64u &&
          sparseBank.currentCc11()==127u);
    CHECK(sparseBank.restoreSelection(128u,2u,77u,18u,120u));
    CHECK(sparseBank.currentBank()==128u &&
          sparseBank.currentProgram()==2u &&
          sparseBank.currentCc7()==77u &&
          sparseBank.currentCc10()==18u &&
          sparseBank.currentCc11()==120u);
    CHECK(!sparseBank.restoreSelection(128u,63u,1u,1u,1u));
    CHECK(sparseBank.currentBank()==128u &&
          sparseBank.currentProgram()==2u &&
          sparseBank.currentCc7()==77u);
    CHECK(sparseBank.restoreSelection(0u,77u,100u,64u,127u));
    CHECK(sparseBank.currentBank()==0u &&
          sparseBank.currentProgram()==77u &&
          sparseBank.currentCc7()==100u &&
          sparseBank.currentCc10()==64u &&
          sparseBank.currentCc11()==127u);
    sparseBank.clear();
    CHECK(sparseBank.presetCount()==0u);
    CHECK(!sparseBank.programChange(77u));
    CHECK(sparseBank.currentProgram()==0u);

    bank.clear();
    CHECK(!bank.ready());
    CHECK(!bank.nativeNoteCheckpointActive());
    CHECK(!bank.beginNativeNoteCheckpoint());
    std::cout<<"P002_REAL_VST3_CREATIVE_VXD_CHECKPOINT_ROLLBACK_PASS "
                "rollback_state=TRUE keys=TRUE ready=TRUE "
                "commit_persist=TRUE no_ownership_ghost=TRUE\\n";
    return 0;
}
