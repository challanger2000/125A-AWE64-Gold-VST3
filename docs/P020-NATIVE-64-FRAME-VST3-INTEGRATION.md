# P020 — 64-frame authentic EMU8000 VST3 integration (research branch)

Date: 20 November 2026. P007 VM/Windows 95 production architecture remains untouched; PR #33 continues in isolation.

## Input evidence: P019 owner ROM/SBK/SF2 passed

The owner executed the private Win64 P019 program using authentic `awe32.raw`, `SYNTHGM.SBK`, `Synth 512 Bank.sf2` and Vintage Dreams SF2. The supplied `P019-ERGEBNIS.txt` ends in:

- `P019_PRIVATE_ORIGINAL_ROM_SBK_SF2_LATENCY_COMPARE_SUMMARY pass=28 fail=0 expected=28`
- `P019_PRIVATE_ORIGINAL_ROM_SBK_SF2_CHIP_980_128_64_PCM_BIT_EXACT_PASS_28_OF_28`
- `P019_TEST_EXITCODE=0`

The 28 comparisons cover ROM program 7, user SF2 programs 7/33/34/54/77, vintage bank program 7, DRY and FX sends, and 980 vs 128/64 windows. Each uses 145,496 signed 16-bit stereo sample values after known latency compensation. All were bit-identical with 0 LSB error. Includes overlapping voices, multiple velocities, pitch bend and modulation. **These are native-chip timing-window parity tests, not direct Creative Win95 hardware parity.**

## P020 changes

- Pinned AWE32Emu/86Box core SHA remains unchanged.
- During the isolated native VST3 Windows build, the reviewed P019 header experiment is applied after the original RT/no-global patches.
- Only the `AWE64_Gold_Native_P009` Windows research VST3 target is compiled with `P019_EMU_BLOCK_FRAMES=64`. No voice filter, interpolation, reverb, chorus, Creative driver control logic or SF2 contents are changed.
- CMake refuses to compile the P020 low-latency target if the prerequisite reviewed header override is absent.
- Steinberg VST3 `getLatencySamples()` derives latency from the **actual chip wrapper** `Emu8000Box::kLatencyFrames`. The audio is **not** advanced by lying to the DAW.
- The real Win64 VST3 host QA requires correct delay at 8k, 44.1k, 48k, 96k, and 192k. For a 64-frame native chip, the corresponding advertised host delays are 12, 64, 70, 140 and 279 frames.
- The 64-frame native same-process two-instances/thread-migration probe and all-rates no-heap callback probe are compiled with the same 64-frame macro, not the old 980-frame setting.
- Separate 980/128/64 original-chip regression executables remain independently available to compare PCM after latency compensation, as previously validated P019.

## Acceptance

P020 64-frame VST3 **cannot be considered fully accepted** until:
1. Public Windows VST3 validator 47/47, real host latency checks, RT callback and multi-instance probes are green on the exact 64-frame commit.
2. A new owner Windows private `P010NativeHostQA.exe --private` run at 64 frames with actual ROM/SBK/SF2 (ROM, 77 SF2 programs, P014 timed bank/patch, P015 v1/v4 PCM project recall, A/B/A, fullscale and rate diagnostics) passes.
3. The final Studio Pro 8 live use is deferred until other core quality gates are ready.

There is no claim of sample-exact true Win95/hardware parity, comprehensive polyphony/velocity coverage, or guaranteed project-file SF2 identity recall. P019 original chip compared at 44.1k; the P020 host's 48–192k latency contract is independently tested, but same-input private PCM parity at all rates is still a future gate.

## Public Windows P020 result — PASSED on real 64-frame instrument

Windows Actions [run 44794648362](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44794648362), source commit `fe39a722bd65c1d3ed90683102244ef0fe915ec7`:

- Steinberg validator: **47 tests passed, zero failed**.
- The actual Windows x64 VST3 reports **12, 64, 70, 140, 279 host frames** at **8k, 44.1k, 48k, 96k, 192k Hz**, respectively. This verifies it is actually compiled with the shorter chip window, not merely claiming a lower latency.
- Windows no-heap audio/MIDI callback regressions across five sample rates, genuine two-layer SoundFont and two concurrent original-chip instances: PASSED.
- Same-process dual-synth and DAW thread migration: PASSED.
- Independent original-chip 980/128/64 DRY and FX PCM regression with synthetic SF2: 0 sample differences in all four latency-aligned cases, verified twice by Python and native Win64 comparator.
- No private ROM data or authentic sample banks are present in public CI; no silent fixture is interpreted as owner-ROM sound proof.

**P020 private Windows package assembled** with the exact publicly passed 64-frame VST3 binary and host QA, plus the owner's pre-existing private source assets. Private package holds original ROM, `SYNTHGM.SBK`, real 77-presets SF2 and vintage SF2, with checksum verification, one-click `START-P020-TEST.bat`, and gated `P020-ERGEBNISSE/P020-ERGEBNIS.txt`. This package must remain private and is **not published to GitHub**.

**Acceptance status:** P020 real VST3 public CI PASS. Original owner PCM inside the **new** 64-frame VST3 still awaits local private test results. P019 owner 28/28 chip-only results are not falsely substituted for that P020 test. P007 remains untouched.
