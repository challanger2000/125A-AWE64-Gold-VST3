# P209 — Genuine owner SoundFont compatibility: linked stereo length

## Root cause verified on all 13 original private SF2 banks (offline only)

P199 introduced a blanket SF2 SoundFont parser check requiring two
reciprocally linked LEFT/RIGHT samples to have **exactly equal sample
frame counts**. Independent local offline validation of the owner's
13 previously supplied original SF2 banks found **four** banks where
otherwise-valid reciprocal, same-sample-rate stereo L/R pairs differ in
length, by 1 to 271 frames (with other disparities including 10,
17 and 18 frames).
P199 therefore rejected each ENTIRE bank before one preset could be
played — irrespective of how correct the original 86Box DSP core was.

This is not a damaged ROM or missing media. EMU8000's physical
left/right voices have distinct start, end, loop and CCCA registers.
Identical sample lengths are NOT required for them to run together.
Previous strict same-length parser gate was a native adapter decision,
not proven by original Creative hardware port registers. A bank
should not fail in its entirety for 1-frame differences.

## Repair and exact acceptance

- P209 removes **only** the false equal-length requirement; it
  retains type 2↔4, reciprocal sample IDs, same sample rate,
  individual nonnegative sample lengths and distinct physical
  sample end addresses.
- The original unchanged 86Box C EMU8000 engine, source-based
  physical stereo allocator, 28-MiB DRAM, Windows VST3 interface,
  SF1/SBK, preset/GUI/automation IDs and private assets are untouched.
- Independent public synthetic SF2 fixtures recreate the private
  failure conditions (linked L/R sample length +1, +18, and +271)
with an
  unrelated mono layer and reversed stereo zone order.
- Native Windows C++ test drives the **actual original 86Box chip**,
  requires two linked adjacent hardware voices with source-correct
  left/right main programming, one independent mono, all 3 distinct
  allocator stops BEFORE main, genuine note triggers and nonzero
  original-chip PCM.
- Standalone test-only **negative variant reintroduces the exact old
  length-equality error** and must be rejected specifically by
  `P209 valid unequal-length stereo SoundFont rejected:
  Mismatched SF2 stereo sample pair`. It is excluded from product VST3.
- Full prior P199-P208 tests, Steinberg VST3 Validator, published
  125A Tester, dev Tester, GUI/host/state/installer regressions are
  retained with no relaxation.

**Privacy:** only aggregate results of private original bank
inspection and synthetically generated samples enter GitHub.
No original owner ROM/SBK/SF2 byte is uploaded.

**Release remains independently gated:** live original guest/owner
SF2 sound parity, Studio Pro true ASIO playback, and external Codex
approval cannot be inferred from these strong compatibility checks.
