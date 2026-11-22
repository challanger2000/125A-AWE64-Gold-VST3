# P197 · Original Creative SF2 scaleTuning: independent source and native Windows evidence

**Date:** 2026-11-22. **Scope:** targeted correction candidate, not a full auditory/driver/hardware audit or a release. Private PR #46 and owner-installed P192 remain untouched.

## Original Creative provenance, independently checked

Private user-provided `SBAWE.VXD` version 4.38.0.2 (SHA256 `d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`) was disassembled privately. No licensed original binary or bank is included in the repository or CI.

The original Win95 SF2 path in LCOD explicitly reads generator 56 (`scaleTuning`) at `+0x5709`, multiplies it by the root key at `+0x570F`, subtracts at `+0x5718`, multiplies it by the played note at `+0x572D`, then accumulates at `+0x5730`. Its pitched-note contribution is therefore `(playedKey - rootKey) × scaleTuning` cents per key.

By contrast, the prior pinned upstream `turican0/AWE32Emu` `SoundFont.cpp` always calculated `(playedKey-rootKey) × 100` for all values except 1. That exception was copied from a separately demonstrated SF1/SBK driver path, where generator 56 value 1 denotes a special half-pitch behavior. SF1 semantics remain unchanged.

The SoundFont 2 default is `scaleTuning = 100`, also verified in the original Creative SF2 default generator table.

## Scoped implementation

- `research/native-awe32emu/p009-vst3/p197_creative_sf2_scale_tuning.py`: narrowly overlays the pinned SoundFont.cpp when Windows CI assembles the VST3; no modification to original 86Box EMU8000 DSP, ROM/SBK, VST3 public IDs or GUI.
- `tests/P197CreativeScaleTuningPair.py`: five independently generated public SF2 banks with identical PCM sample and all other generator settings, differing only in gen56 (omitted, 100, 50, 0, 1).
- `tests/P010NativeHostQA.cpp`: exercises the **actual compiled Win64 VST3** at 44.1, 48 and 96 kHz. Source-derived positive equivalence checks: missing=100, MIDI72/scale50=MIDI66/scale100, MIDI72/scale0=MIDI60/scale100. Negative controls: 50 != 100, 0 != 100, 1 != 0 at MIDI72. At root MIDI60, all scales must match.
- SF1/SBK branch of the original code was not altered.

## Native Win64 evidence

[GitHub Actions run 45031888603](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45031888603), exact audio code commit `78eaa1115f3bb866c4d857edae95d12bcfbdfdf8`: **SUCCESS**.

- `P197_ORIGINAL_CREATIVE_SF2_SCALE_TUNING_THREE_RATES_SIX_ORACLES_PASS`: PASS with nonzero original-EMU8000 audio, all positive/negative comparisons at three sample rates.
- Mandatory P192 Sustain, P194 both Attacks, P195 generator sums + negatives, P196 sample-end offsets + negatives: PASS on same artifact.
- Steinberg Validator **47/47 PASS**.
- 125A Tester 0.2.10 and 0.3.3 **41 PASS, 0 warnings, 0 failures** each.
- N08 **1,398,799** sounding saves; stale/mixed/rejected all 0.
- P179 **500/500** coherent valid saves, 700 audible blocks, zero torn/failed.

## Owner data risk inventory

13 owner-provided SF2 corpus: **316 reachable layers** with explicitly nondefault gen56 in two banks: `Callanish.SF2` (313 reachable layer occurrences across 11 presets) and `Synth 512 Bank.sf2` (3 layers across three guitar presets, value=50). Other user SF2s using the default=100 are unaffected by P197. The three named guitar presets are `punk guitar`, `lead guitar` and `Guitar`. These are static counts, not a successful original hardware playback test.

## Honest remaining bounds

Native Win64 synthetic audio equality establishes the fix's specific A/B behavior **in the product**, not end-to-end parity with a real AWE64 Gold capture or live original Win95 driver traces for every note/velocity/SF2 bank. Review of all 60 generators, nonlinearly combined preset/instrument values, arbitrary SF2 modulator records, sample stereo-link semantics, dynamic original FX, and actual original-owner SF2 audition and Fender Studio Pro/ASIO remain **NOT VERIFIED**.

Real CPU deadlines and cause of reported ~80% DAW audio-engine load remain **NOT VERIFIED**; no DSP/CPU optimizations were included. **No release, no full-audit PASS, no change to PR #46.**
