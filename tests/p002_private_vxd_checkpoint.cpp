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
    static bool original(const P002PrivateRomBank& bank) {
        return bank.ready_ &&
               bank.state_[0x42u]==0xfeu &&
               bank.state_[0x43u]==0x0fu &&
               bank.state_[0x90u]==0x29u &&
               !bank.held_keys_[60] && bank.held_keys_[62] &&
               bank.ownership_.trackedPhysicalVoices()==0u;
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

    bank.clear();
    CHECK(!bank.ready());
    CHECK(!bank.nativeNoteCheckpointActive());
    CHECK(!bank.beginNativeNoteCheckpoint());
    std::cout<<"P002_REAL_VST3_CREATIVE_VXD_CHECKPOINT_ROLLBACK_PASS "
                "rollback_state=TRUE keys=TRUE ready=TRUE "
                "commit_persist=TRUE no_ownership_ghost=TRUE\\n";
    return 0;
}
