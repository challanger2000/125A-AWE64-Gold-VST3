#include "CreativeCandidateRamAllocator.h"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace awe64::creative;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<" "<<#x<<"\n";return 1;}}while(false)
int main(){
    P003CandidateRamAllocator a;
    CHECK(a.valid() && a.freeWords()==0xffffe0u-0x200010u);
    CHECK(!a.allocate(0u));
    CHECK(!a.allocate(0xffffffffu));
    const auto one=a.allocate(260921u);
    CHECK(one && one->base==0x200010u && one->words==260921u);
    CHECK(a.contains(*one));
    CHECK(a.pin(*one) && a.pin(*one) && a.activePins(*one)==2u);
    CHECK(!a.release(*one));
    CHECK(!a.unpin({one->base,one->words,one->generation+1u,one->id}));
    CHECK(a.unpin(*one) && a.activePins(*one)==1u);
    CHECK(!a.release(*one));
    CHECK(a.unpin(*one) && a.activePins(*one)==0u);
    CHECK(!a.unpin(*one));
    const auto two=a.allocate(345u);
    CHECK(two && two->base==one->base+one->words);
    const auto three=a.allocate(18u);
    CHECK(three && three->base==two->base+two->words);
    CHECK(a.usedBanks()==3u);
    CHECK(a.release(*two));
    CHECK(!a.release(*two)); // double free, no chip writes
    CHECK(a.contains(*one) && a.contains(*three));
    const auto reused=a.allocate(200u);
    CHECK(reused && reused->base==two->base && reused->id==two->id);
    CHECK(!a.release(*two)); // stale generation cannot free new bank
    CHECK(a.contains(*reused));
    const auto noFit=a.allocate(500u);
    CHECK(noFit && noFit->base==three->base+three->words);
    CHECK(a.release(*reused));
    const auto justFit=a.allocate(345u);
    CHECK(justFit && justFit->base==two->base);
    CHECK(a.freeWords()==(0xffffe0u-0x200010u)-
        (one->words+three->words+noFit->words+justFit->words));
    CHECK(a.release(*one));
    CHECK(a.release(*three));
    CHECK(a.release(*noFit));
    CHECK(a.release(*justFit));
    CHECK(a.usedBanks()==0u);
    const auto full=a.allocate(a.freeWords());
    CHECK(full && full->base==0x200010u);
    CHECK(!a.allocate(1u));
    CHECK(a.release(*full));
    P003CandidateRamAllocator tail(0xffffdcu,0xffffe0u);
    CHECK(tail.valid());
    const auto end=tail.allocate(4u);
    CHECK(end && end->base==0xffffdcu);
    CHECK(!tail.allocate(1u));
    CHECK(tail.release(*end));
    CHECK(!P003CandidateRamAllocator(0x200002u).valid());
    CHECK(!P003CandidateRamAllocator(0xffffe0u).valid());
    CHECK(!P003CandidateRamAllocator(0x200010u,0x200010u).valid());
    // Fixed descriptor capacity protects bounded worst-case behavior.
    // Two-bank ground truth: privately measured ORIGINAL Creative
    // Win95 86Box source, 2026-11-15. Test uses only physical address
    // and length metadata, never the user's ROM or SF2 sample bytes.
    // Original QUERY_DRAM_SIZE deltas: first 260937 (=260921+16);
    // second 32784 (=32768+16); real SMALW/SMLD sample bases:
    // 0x200010 and 0x23FB59. A naive raw allocator predicted
    // 0x23FB49 and MUST NOT silently pass as source parity.
    P003CandidateRamAllocator originalMeasured;
    const auto firstOriginal=
        originalMeasured.allocateCreativeSampleWords(260921u);
    CHECK(firstOriginal && firstOriginal->base==0x200010u &&
          firstOriginal->words==260921u);
    const auto secondOriginal=
        originalMeasured.allocateCreativeSampleWords(32768u);
    CHECK(secondOriginal && secondOriginal->base==0x23fb59u &&
          secondOriginal->words==32768u &&
          secondOriginal->base!=firstOriginal->base+firstOriginal->words);
    CHECK(originalMeasured.freeWords()==
          (0xffffe0u-0x200010u)-260937u-32784u);
    CHECK(originalMeasured.pin(*firstOriginal));
    CHECK(!originalMeasured.release(*firstOriginal));
    CHECK(originalMeasured.unpin(*firstOriginal));
    CHECK(originalMeasured.release(*secondOriginal));
    CHECK(originalMeasured.release(*firstOriginal));
    CHECK(originalMeasured.freeWords()==0xffffe0u-0x200010u);
    P003CandidateRamAllocator lastAllowed(0xffffcfu,0xffffe0u);
    const auto oneNearFm=lastAllowed.allocateCreativeSampleWords(1u);
    CHECK(oneNearFm && oneNearFm->base==0xffffcfu);
    CHECK(!lastAllowed.allocateCreativeSampleWords(1u));
    CHECK(lastAllowed.release(*oneNearFm));
    P003CandidateRamAllocator tooSmallForGuard(0xffffd0u,0xffffe0u);
    CHECK(!tooSmallForGuard.allocateCreativeSampleWords(1u));

    P003CandidateRamAllocator sixtyFour;
    std::vector<P003CandidateRamAllocation> leases;
    for(unsigned i=0;i<64u;++i){
        auto x=sixtyFour.allocate(1u);
        CHECK(x && x->base==0x200010u+i);
        leases.push_back(*x);
    }
    CHECK(!sixtyFour.allocate(1u));
    CHECK(sixtyFour.usedBanks()==64u);
    CHECK(sixtyFour.release(leases[13]));
    const auto refilled=sixtyFour.allocate(1u);
    CHECK(refilled && refilled->base==leases[13].base &&
          refilled->generation!=leases[13].generation);
    CHECK(!sixtyFour.release(leases[13]));
    CHECK(sixtyFour.contains(*refilled));
    std::cout<<"P003_CANDIDATE_SF2_MEMORY_ALLOCATOR_PASS"
             <<" multiple_banks=TRUE"
             <<" released_gap_reuse=TRUE"
             <<" live_voice_pins_block_bank_reuse=TRUE"
             <<" stale_handle_rejected=TRUE"
             <<" physical_ram_tail_guard=TRUE"
             <<" free_space=TRUE"
             <<" genuine_Win95_two_bank_sample_offsets=EXACT"
             <<" genuine_Win95_per_bank_16_words=EXACT"
             <<" generic_compaction_parity=NOT_PROVEN"
             <<" source_Creative_allocation_parity=TWO_BANK_LAYOUT_ONLY"
             <<" allocator_does_not_touch_chip=TRUE\n";
    return 0;
}
