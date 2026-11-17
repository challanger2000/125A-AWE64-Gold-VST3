// Synthetic source/state lifecycle tests for the real VST3 input adapter.
#include "P005OriginalCreativeHostAdapter.hpp"
#include "OriginalSf2RuntimeFixture.hpp"
#include <cstdio>
using namespace awe64::original_vxd;
using namespace awe64::original_vxd::test;
using awe64::vst3::P005OriginalCreativeHostAdapter;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"RUNTIME_HOST_FAIL %d %s\n",__LINE__,#x);return 1;}}while(false)
int main() {
    RuntimeFixture fixture;OriginalCreativeSf2RuntimeBank bank,otherBank;
    CHECK(bank.prepareNonRealtime(fixture.slots,fixture.files,fixture.bindings,
        fixture.resources,128u*1024u,true)==Sf2RuntimeBankStatus::Ok);
    const auto initial=runtimeState();RuntimeChip chip,other;
    P005OriginalCreativeHostAdapter<RuntimeChip> adapter,second;
    CHECK(!adapter.activateOwnedOriginalRuntimeBankNonRealtime(chip,bank,initial,false));
    auto unknown=initial;unknown.channel[0].original_fine_tune_verified=false;
    CHECK(!adapter.activateOwnedOriginalRuntimeBankNonRealtime(chip,bank,unknown,true));
    CHECK(!bank.runtimeLeased() && !adapter.sourceBound());
    CHECK(adapter.activateOwnedOriginalRuntimeBankNonRealtime(chip,bank,initial,true));
    CHECK(bank.runtimeLeased() && adapter.active());
    CHECK(!bank.resetNonRealtime());
    CHECK(!second.activateOwnedOriginalRuntimeBankNonRealtime(other,bank,initial,true));
    CHECK(adapter.sharedRuntime()->scratch.device==&adapter.sharedRuntime()->midi.device);
    CHECK(adapter.controller(0u,0u,1u)==MidiResult::Applied);
    CHECK(adapter.program(0u,7u)==MidiResult::Applied);
    CHECK(adapter.noteOn(0u,60u,100u)==MidiResult::Applied);
    CHECK(adapter.noteOff(0u,60u)==MidiResult::Applied);
    CHECK(adapter.controller(15u,0u,2u)==MidiResult::Applied);
    CHECK(adapter.program(15u,0u)==MidiResult::Applied);
    CHECK(adapter.noteOn(15u,62u,127u)==MidiResult::Applied);
    CHECK(adapter.noteOff(15u,62u)==MidiResult::Applied);
    // A second instance owns a separate source/cache and original device.
    CHECK(otherBank.prepareNonRealtime(fixture.slots,fixture.files,fixture.bindings,
        fixture.resources,128u*1024u,true)==Sf2RuntimeBankStatus::Ok);
    CHECK(second.activateOwnedOriginalRuntimeBankNonRealtime(other,otherBank,initial,true));
    CHECK(second.controller(0u,0u,2u)==MidiResult::Applied);
    CHECK(second.program(0u,0u)==MidiResult::Applied);
    CHECK(second.noteOn(0u,48u,100u)==MidiResult::Applied);
    CHECK(adapter.sharedRuntime()->midi.channel[0].original_selected_bank==1u);
    CHECK(second.sharedRuntime()->midi.channel[0].original_selected_bank==2u);
    CHECK(&adapter.sharedRuntime()->midi.device!=&second.sharedRuntime()->midi.device);
    CHECK(adapter.controller(0u,7u,127u)==MidiResult::Applied);
    CHECK(second.sharedRuntime()->midi.channel[0].volume==100u);
    CHECK(second.noteOff(0u,48u)==MidiResult::Applied);
    adapter.resetNonRealtime();
    CHECK(!bank.runtimeLeased() && !adapter.sourceBound());
    CHECK(second.active() && otherBank.runtimeLeased());
    second.resetNonRealtime();
    {
        P005OriginalCreativeHostAdapter<RuntimeChip> temporary;
        CHECK(temporary.activateOwnedOriginalRuntimeBankNonRealtime(chip,bank,initial,true));
    }
    CHECK(!bank.runtimeLeased());
    CHECK(adapter.activateOwnedOriginalRuntimeBankNonRealtime(chip,bank,initial,true));
    CHECK(adapter.controller(0u,0u,1u)==MidiResult::Applied);
    CHECK(adapter.program(0u,0u)==MidiResult::Applied);
    chip.fail_write=true;
    CHECK(adapter.noteOn(0u,60u,100u)==MidiResult::PortError);
    CHECK(adapter.sourceBound() && !adapter.active());
    const auto before=chip.writes;
    CHECK(adapter.noteOn(0u,60u,100u)==MidiResult::MissingSource);
    CHECK(chip.writes==before); // fault cannot route to an alternate engine
    adapter.resetNonRealtime();
    CHECK(bank.resetNonRealtime());
    // Original rows that lack an authenticated SF2 binding stay unavailable;
    // strict host activation must not advertise a complete source bank.
    CHECK(bank.prepareNonRealtime(fixture.slots,fixture.files,
        std::span(fixture.bindings).first(4u),fixture.resources,128u*1024u,true)==Sf2RuntimeBankStatus::Ok);
    CHECK(bank.unboundPresetCount()==1u);
    CHECK(!second.activateOwnedOriginalRuntimeBankNonRealtime(other,bank,initial,true));
    std::puts("SYNTHETIC_RUNTIME_BANK_HOST_LEASE_SHARED_STATE_NO_FALLBACK_PASS");
}
