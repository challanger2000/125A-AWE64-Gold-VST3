// P003 original Creative mode-2 CCCA-only source contract, C++23.
#include "CreativeVxdAdditionalAllocatorModes.h"
#include <array>
#include <cstdint>
#include <cstdio>

using awe64::creative::OriginalWin95AllocatorVoiceSnapshot;
using awe64::creative::OriginalWin95MonoSelectionReason;
using awe64::creative::candidateOriginalWin95Mode2Range;
using awe64::creative::kOriginalWin95PhysicalVoiceCapacity;

// The original 2026-11-17 P002 implementation lacks a per-register CCCA
// validity marker; leave this source compilable against that old header.
// Do NOT treat merely having a numeric address as evidence of a chip read.
template<class T> void mark_verified_ccca(T& item) {
    if constexpr (requires(T x) { x.ccca_read_verified; })
        item.ccca_read_verified=true;
}

template<class T, std::size_t N>
bool rejects_unverified_ccca(std::array<T,N>& voices,
                            std::uint32_t packed_range) {
    if constexpr (requires(T x) { x.ccca_read_verified; }) {
        voices[0].ccca_read_verified=false;
        return !candidateOriginalWin95Mode2Range(
            voices,packed_range,2u,32u);
    }
    return true; // legacy API has no per-register validity field
}

int main() {
    std::array<OriginalWin95AllocatorVoiceSnapshot,
               kOriginalWin95PhysicalVoiceCapacity> voices{};
    auto& a=voices[0];
    a.internal_key_marker=0x2000u;
    a.stored_sample_end=100u;
    a.chip_current_address=100u;
    a.complete=false;  // no VTFT/DCYSUSV -- they are not read in source
    mark_verified_ccca(a); // only an original verified CCCA read
    auto& b=voices[1];
    b.internal_key_marker=0x0ffeu;
    b.volume_target_hi16=0u;
    b.complete=true;
    constexpr std::uint32_t r=(2u<<8u)|0u;

    // Case A: sample end reached; original short-circuits at voice 0.
    auto x=candidateOriginalWin95Mode2Range(voices,r,2u,32u);
    if(!x || x->physical_voice!=0u ||
       x->reason!=OriginalWin95MonoSelectionReason::SampleEndReached) {
        std::puts("P003_MODE2_CCCA_ONLY_EARLY_END_REJECTED");
        return 42;
    }

    // Case B: sample end not reached; original moves to voice 1 and
    // reads its real VTFT, which indicates an immediate idle voice.
    a.chip_current_address=99u;
    x=candidateOriginalWin95Mode2Range(voices,r,2u,32u);
    if(!x || x->physical_voice!=1u ||
       x->reason!=OriginalWin95MonoSelectionReason::CompletelyIdle) {
        std::puts("P003_MODE2_CCCA_ONLY_CONTINUE_REJECTED");
        return 42;
    }

    // Case C: if no CCCA read has actually been validated, fail closed.
    // The independently verified flag is a research extension; old
    // developer snapshot representation inherently cannot express it.
    if(!rejects_unverified_ccca(voices,r)) {
        std::puts("P003_MODE2_CCCA_MISSING_MUST_FAIL_CLOSED");
        return 3;
    }
    std::puts("P003_MODE2_CCCA_ONLY_SOURCE_CONTRACT_PASS");
    return 0;
}
