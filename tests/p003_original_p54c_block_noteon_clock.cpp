// P003 NoteOn CPU: original 86Box old-dynarec execution-cost ledger.
// Synthetic original-source cycle debits ONLY; neither original
// Creative VXD instruction trace nor product VST3 audio is replayed.
#include "P003OriginalP54cBlockClock.h"
#include "P003OriginalPitClock.h"
#include <cstdint>
#include <cstdio>
#include <limits>
using awe64::research::OriginalP54cBlockClock;
using awe64::research::original86BoxWavetableLatch32_32;
#define CK(c) do {if(!(c)){std::printf("P003_GUEST_CPU_BLOCK_CLOCK_FAIL line=%d\n",__LINE__);return 1;}}while(false)
int main(){
    constexpr std::uint64_t latch=
        original86BoxWavetableLatch32_32(133333333u);
    CK(latch==12985539821609ull);
    OriginalP54cBlockClock c;
    CK(!c.beginBlock());
    CK(!c.initialize(10000,0,13023,0,latch,0));
    CK(!c.initialize(10000,5000,9999,0,latch,0));
    CK(c.initialize(10000,6000,13023,
                    static_cast<std::uint32_t>(latch),latch,0));
    CK(!c.emuPort(0xe22u,0x60u));
    CK(c.beginBlock());
    CK(!c.beginBlock());
    CK(!c.debitTranslatorCycles(0u));
    CK(c.debitTranslatorCycles(50u));
    const auto first=c.emuPort(0xe22u,0x60u);
    CK(first && first->guest_tsc==10000u &&
       first->remaining_cycles==5950 && first->chip_frame==0u);
    CK(c.debitTranslatorCycles(19u));
    const auto second=c.emuPort(0xa20u,0xffu);
    CK(second && second->guest_tsc==first->guest_tsc &&
       second->remaining_cycles==5931 &&
       second->chip_frame==first->chip_frame);
    CK(!c.emuPort(0xeb,0u)); // delay port NOT an EMU8000 port
    CK(c.sourceUpdateTsc());
    CK(c.guestTsc()==10069u && c.chipFrames()==0u);
    const auto after_sync=c.emuPort(0xe22u,0xa0u);
    CK(after_sync && after_sync->guest_tsc==10069u);
    CK(c.debitTranslatorCycles(2990u));
    CK(c.finishBlock());
    // Total charge 50+19+2990=3059; interim TSC=69 was
    // ALREADY accounted at source update_tsc. NO DOUBLE CHARGE.
    CK(c.guestTsc()==13059u && c.chipFrames()==1u);
    CK(!c.finishBlock());
    CK(c.beginBlock());
    CK(c.debitTranslatorCycles(2000u));
    const auto later=c.emuPort(0x620u,0xffffu);
    CK(later && later->chip_frame==1u &&
       later->guest_tsc==13059u);
    CK(c.sourceUpdateTsc());
    CK(c.guestTsc()==15059u && c.chipFrames()==1u);
    CK(c.sourceUpdateTsc()); // repeated source sync must charge zero
    CK(c.guestTsc()==15059u);
    CK(c.debitTranslatorCycles(1024u));
    CK(c.finishBlock());
    CK(c.guestTsc()==16083u && c.chipFrames()==2u);
    CK(!c.emuPort(0x620u,0xffffu));
    // A full (synthetic) NoteOn source callback stream can contain
    // hundreds of OUTs within one guest block. All retain their
    // source CPU-TSC and CPU budget continuously decreases.
    CK(c.beginBlock());
    for(unsigned i=0;i<152u;++i) {
        CK(c.debitTranslatorCycles(i%3u==0u?12u:19u));
        auto stamp=c.emuPort(i%2u?0xa20u:0xe22u,
                             static_cast<std::uint16_t>(i));
        CK(stamp && stamp->guest_tsc==16083u &&
           stamp->chip_frame==2u);
    }
    CK(c.finishBlock());
    CK(c.guestTsc()>16083u);
    // Exact integer bound safety: a nonsensical translator debit
    // must be rejected, never clamped or wrapped.
    CK(c.beginBlock());
    const auto saved=c.cpuBudget();
    CK(!c.debitTranslatorCycles(std::numeric_limits<std::uint32_t>::max()));
    CK(c.cpuBudget()==saved);
    CK(c.debitTranslatorCycles(1u));
    CK(c.finishBlock());
    CK(c.cpuBudget()==saved-1);
    std::puts("P003_P54C_GUEST_BLOCK_TSC_NOTEON_SOURCE_ACCOUNTING_PASS"
              " P110_COMPILED_COSTS_REQUIRED=YES"
              " EXECUTED_BLOCK_ORDER_REQUIRED=YES"
              " LIVE_WIN95_GUEST_PACING_PROVEN=NO");
    return 0;
}
