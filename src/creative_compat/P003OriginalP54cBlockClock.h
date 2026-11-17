#pragma once
// Research-only, bounded copy of original 86Box old-dynarec CPU-block
// time reconciliation. SOURCE: turican0/86Box-AWE32 v6.0-AWE32-0.93
//   src/cpu/386_dynarec.c: update_tsc() and exec386_dynarec()
//   src/codegen/codegen_x86-64.c: codegen_accumulate(ACCREG_cycles,..)
//   src/io.c: registered EMU8000 ports do not call update_tsc()
// The costs passed into debitTranslatorCycles MUST eventually come from
// the original P110 translated Pentium U/V/AGI + P111 executed block join.
// This helper does not decode a Creative instruction, choose a branch,
// execute Windows 95, or prove autonomous NoteOn/PCM parity.
#include "P003OriginalWavetableTimer.h"
#include <cstdint>
#include <optional>
namespace awe64::research {
struct OriginalP54cIoStamp {
    std::uint64_t guest_tsc=0;
    std::int32_t remaining_cycles=0;
    std::uint64_t chip_frame=0;
    std::uint16_t port=0;
    std::uint16_t value=0;
};
class OriginalP54cBlockClock final {
public:
    // Source guest CPU budget and 86Box 32:32 wavetable timer state.
    // All quantities must come from independent source settings.
    [[nodiscard]] bool initialize(
        std::uint64_t guest_tsc,
        std::int32_t original_cpu_budget,
        std::uint64_t first_due_tsc,
        std::uint32_t first_due_fraction,
        std::uint64_t original_wavetable_latch_32_32,
        std::uint64_t initial_chip_frame) noexcept;
    // Start one source old-x64 86Box translated/JIT block.
    [[nodiscard]] bool beginBlock() noexcept;
    // Called in the ACTUAL sequence of compiled host-JIT cycle debits,
    // not per optimistic x86 instruction or calibrated MIDI command.
    [[nodiscard]] bool debitTranslatorCycles(std::uint32_t n) noexcept;
    // Observe a registered EMU8000 port callback; original source io.c
    // does not call update_tsc() for these six registered port families.
    [[nodiscard]] std::optional<OriginalP54cIoStamp> emuPort(
        std::uint16_t port,std::uint16_t word) const noexcept;
    // Original update_tsc() invoked by an explicit source CPU sync,
    // NOT automatically by each EMU8000 registered port callback.
    [[nodiscard]] bool sourceUpdateTsc() noexcept;
    // Source exec386_dynarec() reconciliation after executed block.
    [[nodiscard]] bool finishBlock() noexcept;
    [[nodiscard]] std::uint64_t guestTsc() const noexcept {return tsc_;}
    [[nodiscard]] std::uint64_t chipFrames() const noexcept {return timer_.frame();}
    [[nodiscard]] std::int32_t cpuBudget() const noexcept {return cycles_;}
    [[nodiscard]] bool blockActive() const noexcept {return active_;}
private:
    [[nodiscard]] bool reconcile(bool after_block) noexcept;
    OriginalWavetableTimer timer_{};
    std::uint64_t tsc_=0;
    std::uint64_t block_start_tsc_=0;
    std::int32_t cycles_=0;
    std::int32_t block_start_cycles_=0;
    bool initialized_=false;
    bool active_=false;
};
} // namespace awe64::research
