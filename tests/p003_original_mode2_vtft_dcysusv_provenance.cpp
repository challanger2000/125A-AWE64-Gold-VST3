// Independent original SBAWE.VXD mode-2 per-register read provenance audit.
// Native C++23 regression, NOT a second Creative driver port.
//
// Original LCOD+41D9, +41E0..+423B, +424F..+4269:
//   marker<0x2000             : VTFT is required
//   marker!=0x0FFE & low!=0xFF: DCYSUSV is also required
//   marker==0x0FFE & VTFT hi=0: immediate idle return before CCCA
//   stored_sample_end==0      : no CCCA read is needed
//   stored_sample_end!=0      : CCCA required unless already idle
//
// Only ACTUALLY validated chip reads may set source flags. A missing
// required register must yield nullopt, never an invented chip value.
#include "CreativeVxdAdditionalAllocatorModes.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

using awe64::creative::OriginalWin95AllocatorVoiceSnapshot;
using awe64::creative::OriginalWin95MonoSelectionReason;
using awe64::creative::candidateOriginalWin95Mode2Range;
using awe64::creative::kOriginalWin95PhysicalVoiceCapacity;

template<class V>
void mark_vtft(V& v, bool x) {
    if constexpr (requires(V a){ a.vtft_read_verified; })
        v.vtft_read_verified=x;
}
template<class V>
void mark_dcysusv(V& v, bool x) {
    if constexpr (requires(V a){ a.dcysusv_read_verified; })
        v.dcysusv_read_verified=x;
}
template<class V>
void mark_ccca(V& v, bool x) {
    if constexpr (requires(V a){ a.ccca_read_verified; })
        v.ccca_read_verified=x;
}

int main() {
    std::array<OriginalWin95AllocatorVoiceSnapshot,
               kOriginalWin95PhysicalVoiceCapacity> rows{};
    auto &v=rows[0];
    v.complete=false;      // no extra EMU8000 reads may be invented
    v.internal_key_marker=0x0ffeu;
    v.volume_target_hi16=0u;
    v.stored_sample_end=100u;
    v.chip_current_address=0u;
    mark_vtft(v,true);     // Original only read VTFT
    constexpr std::uint32_t one=(1u<<8u)|0u;
    auto selection=candidateOriginalWin95Mode2Range(rows,one,1u,32u);
    if(!selection || selection->physical_voice!=0u ||
       selection->reason!=OriginalWin95MonoSelectionReason::CompletelyIdle) {
        std::puts("P003_MODE2_IDLE_VTFT_ONLY_REJECTED");
        return 42;
    }

    // Nonzero VTFT value and no cached end: still no DCYSUSV/CCCA.
    v.volume_target_hi16=5u;v.stored_sample_end=0u;
    selection=candidateOriginalWin95Mode2Range(rows,one,1u,32u);
    if(!selection || selection->physical_voice!=0u ||
       selection->reason!=OriginalWin95MonoSelectionReason::LowestEnvelopePriority ||
       selection->priority!=5u) {
        std::puts("P003_MODE2_NONIDLE_VTFT_ONLY_REJECTED");
        return 42;
    }

    // Non-0FFE marker with low byte == FF takes +4216 and skips
    // DCYSUSV at original +4228. Score: 5+0x200+0x300=773.
    v.internal_key_marker=0x01ffu;
    selection=candidateOriginalWin95Mode2Range(rows,one,1u,32u);
    if(!selection || selection->priority!=773u) {
        std::puts("P003_MODE2_FF_MARKER_VTFT_ONLY_REJECTED");
        return 42;
    }

    // Low byte != FF needs the DCYSUSV status. No validated read
    // must fail; validated DCYSUSV permits a genuine score.
    v.internal_key_marker=0x0100u;
    v.envelope_status_reg=0x8000u;
    mark_dcysusv(v,false);
    selection=candidateOriginalWin95Mode2Range(rows,one,1u,32u);
    if(selection) {
        std::puts("P003_MODE2_MISSING_REQUIRED_DCYSUSV_NOT_REJECTED");
        return 3;
    }
    mark_dcysusv(v,true);
    selection=candidateOriginalWin95Mode2Range(rows,one,1u,32u);
    if(!selection || selection->priority!=517u) {
        std::puts("P003_MODE2_VALID_DCYSUSV_NOT_ACCEPTED");
        return 42;
    }

    // CCCA is required only for non-early-return with cached end.
    v.internal_key_marker=0x0ffeu;
    v.stored_sample_end=100u;
    v.chip_current_address=100u;
    mark_ccca(v,false);
    selection=candidateOriginalWin95Mode2Range(rows,one,1u,32u);
    if(selection) {
        std::puts("P003_MODE2_MISSING_REQUIRED_CCCA_NOT_REJECTED");
        return 3;
    }
    mark_ccca(v,true);
    selection=candidateOriginalWin95Mode2Range(rows,one,1u,32u);
    if(!selection || selection->physical_voice!=0u ||
       selection->reason!=OriginalWin95MonoSelectionReason::SampleEndReached) {
        std::puts("P003_MODE2_VERIFIED_CCCA_NOT_ACCEPTED");
        return 42;
    }
    mark_vtft(v,false);
    selection=candidateOriginalWin95Mode2Range(rows,one,1u,32u);
    if(selection) {
        std::puts("P003_MODE2_MISSING_REQUIRED_VTFT_NOT_REJECTED");
        return 3;
    }
    std::puts("P003_MODE2_VTFT_DCYSUSV_CCCA_SOURCE_PROVENANCE_PASS");
    return 0;
}
