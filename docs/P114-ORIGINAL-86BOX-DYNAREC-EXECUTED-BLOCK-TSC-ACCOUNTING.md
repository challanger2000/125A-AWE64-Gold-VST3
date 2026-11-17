# P114 – Native C++23 block-time accounting from the original 86Box update_tsc()

**17 November 2026; research-only. NO-GO for autonomous Creative NoteOn.**

## Proven source structure

Pinned reference: `turican0/86Box-AWE32`, tag `v6.0-AWE32-0.93`:

- `src/cpu/386_dynarec.c`, `update_tsc()` (original lines 293–319): `cycdiff = cycles_old - cycles`, optionally add `acycs` when `USE_ACYCS` and `inrecomp`; subtract any already-booked interim `tsc - tsc_old`; add only a remaining positive interval.
- `src/cpu/386_dynarec.c`, `exec386_dynarec()` (original lines 859–862 and 935–951): snapshot `cycles_old` and `tsc_old` at block start; the outer block-end time update does **not** add the optional `acycs` offset.
- `src/codegen/codegen_x86-64.c` (original lines 984–1005): P54C translator costs are accumulated into emitted runtime `ACCREG_cycles` changes, including an additional special branch flush. Compiler-time `codegen_block_cycles` alone does not prove when each cost was actually charged.
- `src/cpu/386_dynarec.c` (original lines 595–718): `code()` denotes actually executing previously translated JIT code; the first execution while translating uses a distinct interpreter path.

`src/creative_compat/P114OriginalDynarecBlockClock.hpp` is a header-only, no-allocation, integer-only clock accountant. It accepts explicit **already verified executed** CPU charges, a block CPU/TSC snapshot, optional original auxiliary-cycle changes, and explicit interim timer time. Port callbacks request a timestamp without inventing an OUT-to-OUT delay. Several EMU8000 callbacks within the same timing charge receive the same guest TSC. It guards int32 cycle counter underflow and uint64 TSC overflow. The `autonomous_noteon_ready()` compile-time gate remains **false**.

It deliberately does **not** calculate which original x86 instruction executed, its P54C U/V/AGI costs, Windows-95 dispatch phase, timer callback behavior, original hardware chip phase, or whether the P110/P113 compiler costs were incurred at any given callback. It does not consume original Creative x86 binaries, ROM, SF2, original MIDI traces, or product WAVs.

**Important clock distinction:** the original PIT source timebase `400000000/3 Hz` and the configured Pentium integer cycle budget `133333333 cycles/s` are separate quantities. P114 emits guest TSC counts only; it does **not** convert cycles to EMU8000 frames or round a fractional sample phase.

## Tests

`tests/p114_original_dynarec_block_clock.cpp` executes 20,000 reproducible synthetic comparisons against a separate arithmetic oracle for the original `update_tsc()` order, covering JIT/interpreter flag, optional auxiliary cycles, already elapsed TSC, repeated port callbacks, successive blocks, explicit timer advancement, invalid state and overflow rejection. Both local Linux GCC 14.2 C++23 and Clang C++23 builds/tests completed with `-Wall -Wextra -Werror -pedantic` on 17 November.

These tests **are not** a private genuine Win95/86Box run, a recompiled original emulator callback, actual x86 cost derivation, or PCM identity. `USE_ACYCS` differs by build; call sites must pass this adjustment only if the source build actually uses it. The original `timer_process()` side effects remain outside this helper.

## Next verifiable integration milestone

1. Feed P114 from a **complete executed-path clock producer**: P110/P113 compiler AGI and cost state only for the matching P111 executed JIT epoch; separately include original interpreter first-pass and branch outcomes from P107/P108.
2. Assert the actual costs are charged at the correct guest x86 execution points, not at the mere translation points. Verify against a *fresh, same-run* original Win95 sequence of CPU/TSC/EMU8000 callbacks; multiple port callbacks with the same TSC must remain grouped.
3. Solve fractional CPU-to-EMU8000 sample phase independently and test new MIDI note/voice/bank conditions before any autonomous dry PCM claim.

No developer P005 branch or PR #6, product VST3, EMU8000 DSP, Creative ROM, SF2, or original Win95 guest has been changed. Never merge P114 into product execution based on the synthetic tests alone.
