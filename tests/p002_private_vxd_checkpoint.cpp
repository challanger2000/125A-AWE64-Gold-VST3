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
