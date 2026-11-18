# P007AJ — genuine Win95 Creative → EMU8000 filter-envelope generator proof: 4/4 register writes and resets

**Owner original test:** 18 November 2026, `P007AJ-EMU8000-REGISTER-ERGEBNIS.txt` and `P007AJ-EMU8000-REGISTER-EVENTS.csv`. **Verdict: all four remaining P165 controls reach and change their intended original EMU8000 per-voice generator registers.**

This is **hardware register-control PASS, not yet 4/4 acoustic-effect PASS**. Total **12/12 hardware-control paths confirmed**, **8/12 individually acoustically confirmed** across P007AE/P007AG/P007AH/P007AI/P007AJ; no claim that the final four are audibly distinct with Pad8 ROM sound and the present A/B/A windows.

## Original data integrity and provenance

- TXT SHA256: `b051728414cb5119b759c3957b0190bdaaa96dd44d703bc2146eaa495fba69a7`
- CSV SHA256: `6ad13a73eb0c7051ebee175815061910bd729bee927c52dbfb76ae3c86a184e4`
- **14 authentic original Windows-95 + Creative WinMM + original EMU8000 test notes**, each exactly **80 original chip port writes** => **1120 writes**, 0 overflows, 0 missing notes. `P007AJ_REAL_ORIGINAL_86BOX_REGISTER_SHIM_PRESENT`, `P007AI_ORIGINAL_WIN95_WINMM_READY`, and `P007W_ACTUAL_WINDOWS_VST3_HOST_QA_PASS`.
- True original 86Box-AWE32 compiled core; register observer only in original `snd_emu8k_trace.c` wrapper, actual `snd_emu8k.c` and x86 CPU unchanged. No synthetic sound source.
- Data are private and not committed, only values and SHA-256 summary here. Only voice #0 was used; precise mapping decoded from the unchanged original `snd_emu8k.c:2082-2085` (pointer `E22`, reg/voice selection), `:1626-1729` (`ENVVAL/DCYSUS`), `:1954-1985` (`ATKHLD`), and `:2025-2035` (filter depth `PEFE`).

## Exact original hardware register comparison (decoded from all CSV rows)

All hexadecimal. For the four Filter ENV generators the A reference and A return are at **identical depth PEFE=0040**, meaning the comparison is not confounded by changing the filter route.

| P165 knob | Original Creative generator | Source and note-off port | A (baseline) | B (changed) | A (restored) | Control/return |
|---|---|---|---|---|---|---|
| Filter Env Attack 1104 | Mod-ENV Attack/Hold `ATKHLD` | `A22`, pointer reg 6, voice 0, note start | `7F7D` | `7F03` | `7F7D` | **PASS** |
| Filter Env Decay 1105 | Mod-ENV Decay `DCYSUS` | `A20`, pointer reg 7, voice 0, note start | `007F` | `0002` | `007F` | **PASS** |
| Filter Env Sustain 1106 | Mod-ENV Sustain `DCYSUS` | `A20`, pointer reg 7, voice 0, note start | `007F` | `7F7F` | `007F` | **PASS** |
| Filter Env Release 1107 | Mod-ENV Release `DCYSUS` | `A20`, pointer reg 7, voice 0, **note-off** | `807F` | `8002` | `807F` | **PASS** |

The full note streams for source takes 0/1 are **identical port/value sequences** (80 writes each). Every one of the four target takes has *exactly one distinct physical register word* versus the same-depth reference baseline; there are **no spurious changes** to adjacent original hardware registers. Each of the immediate four A-return takes is again **bit-for-bit identical** in the port/value sequence to the same-depth reference. This is substantially stronger than A/B/A PCM RMS proximity.

### Original Creative NRPN24 signed filter-envelope amount also confirmed

| Test | Original `PEFE` (port `E20`, pointer reg 2, voice 0) |
|---|---|
| Original SoundFont/ROM Pad8 A1/A2 | `0000` |
| Sent Creative NRPN24 signed -96 | `0040` |
| Sent Creative NRPN24 signed +96 | `00C0` |
| Selected depth -96 for all 4 filtered envelope B/A pairs | `0040` on every take |
| Final CC121 SoundFont reset | `0000` |

The sign→hardware 8-bit mapping is via the **original Creative driver**; the test's -96/+96 signed NRPN24 commands have clearly different and reproducible true original chip register effects. This resolves P007AI's ambiguity about whether NRPN24 and the four existing controls reached original Creative/EMU8000. They did.

## Why P007AI audio still returned 0/4 under its acoustic thresholds

The chip-side generator programming demonstrably works and the original filter-depth route demonstrably changes, but the chosen GM Pad 8 (Sweep) ROM voice at this filter cutoff, the envelope's original phase/timing and the old aggregate RMS/roughness classifier may produce small or nonuniform changes. In P007AJ private original PCM all four still showed `INCONCLUSIVE_WITH_THIS_ROM_PATCH`; the original sound had take-to-take baseline drift, and the test's thresholds are too coarse to prove subtle audible changes. That is **not evidence that the filter generator or NRPN path is broken**.

The original emulator code in `snd_emu8k.c:1954-1985` and `:2612-2717` processes modulation envelope attack, decay, sustain and releases and feeds the modulation value through `pefe` into the filter cutoff `filtercut`; therefore the source implements a real runtime effect in addition to the verified register storage. This does not by itself prove the audibility or accurate emulation of the original Creative hardware in all presets; a suitable sound/measurement remains necessary to close the acoustic criterion.

## Release engineering decision

**Close all four original MIDI→Creative driver→EMU8000 register routing failures as DISPROVEN. Do not keep re-running basic control-to-register diagnosis and do not patch or spoof the original Creative driver.** Preserve existing 12-knob approved P165 GUI, raw original 86Box/EMU8000 CPU/chip behavior, 0% SoundFont-neutral reset and 0% FX dry send.

**Product qualification explicitly distinguished:**
- **12/12 original hardware register control paths demonstrably work (PASS).**
- **8/12 acoustic differential proofs established under chosen original patches; 4/12 remain acoustic qualification open.**

A future acoustic probe, if pursued, should use a reproducible sustained spectrally rich original loaded bank/preset with suitable filter modulation depth, meaningful spectral comparisons or original chip filter trajectory rather than chasing absent NRPN delivery. Creative SysEx FX type selection, actual SF2/SBK loading and final Studio Pro/ASIO QA remain independent outstanding product features.

**No user rerun required for the routing conclusion.**
