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

## Completed original-chip Windows x64 full regression, 22 November 2026

**Identical tested code+QA SHA:** `7f771d9defbb0fc0af6570e75d6b7f64463166fe`.
**Windows Actions run #45069346043:** **COMPLETED SUCCESS**,
job `137352581760` (the exact job ID is recoverable from the Actions
run; use the run and source SHA as primary immutable evidence).

- P209 genuine 86Box C EMU8000 **four** independent public synthetic
  original-SF2 worklists PASS: 3 mono, mixed mono+linked stereo with
  +1-frame difference, right-first pair with +18 and +271-frame
  differences. Verified distinct three hardware allocator stops,
  stereo pair first, left/right adjacent original voice programming,
  reverse hardware pair trigger, independent mono start and
  **nonzero** original chip PCM.
- Deliberately bad **P209 test-only old equal-length rule** MUST and
  DID fail at its direct source defect:
  `P204_ORIGINAL_CHIP_RESERVATION_NO_GO P209 valid unequal-length stereo
  SoundFont rejected: Mismatched SF2 stereo sample pair`.
- P199-P208 source + original-chip port-trace tests and negative
  mutants stayed **PASS**, including P204 reservation chronology,
  P205 physical VTFT reset, P206 pedal/CC120/CC123 and P207 pitch
  saturation.
- Windows x64 Steinberg Validator **47 PASS / 0 FAIL**;
  published 125A Plugin Tester v0.2.10 **41 PASS / 0 WARN / 0 FAIL**;
  development tester v0.3.3 **41 / 0 / 0**. Real VST3 GUI/EditorHost,
  dry/wet true chip PCM multirate, ASIO-sized synthetic CPU
  regressions, live sounding project-snapshot stress, and Win64
  interrupt-safe installer rollback integrity **PASS**.

## Independent private owner-media compatibility — no uploaded owner data

All **13 original owner SF2 files** were actually read from the
previously provided local archive, ZIP CRC checked and RIFF/pdta/sdta
hydra fields validated separately. The P199 exact old gate failed
**4 entire banks / 46 reciprocal stereo pairs** due to length
disparities (by as much as **271 frames**), even though all link
types, mutual indices and sample rates were valid. Offline replay
of the P209 parser acceptance conditions now accepts **13/13**.
This is **structural import-compatibility verification**, NOT an
audible native Windows render of all 13 private banks.

The sparse primary bank has **77 actual presets**, and is not
padded to 128. The legacy Vintage Dreams file contains one
`0x8001` ROM sample header whose word indices refer to the real
512K-word EMU8000 ROM; it must not be falsely flagged as a
DRAM-overrun. The largest owner-bank sample-data field is roughly
**88.2% of the 28-MiB RAM**, leaving headroom when banked
individually. One Callanish preset can nominally select
**37 overlapping sample zones** on one MIDI key (the real EMU8000
supports 32 hardware voices; excess layers must never alias
reserved physical voices). This remains a separately tracked
hardware-voice-limit stress scenario, not an assertion of 37
simultaneously audible voices.

## Internal privacy-preserving package staged (NOT a released VST3)

The immutable run's genuine Windows VST3 binary was combined locally
with the original owner `awe32.raw`, `SYNTHGM.SBK`, exact 77-preset
SF2 and previously tested default Vintage Dreams assets, under the
canonical `125A-AWE64-Gold.vst3` bundle, with the prior 125A
crash-recovery installer and a new 18-file complete SHA256
manifest. No ROM/SBK/SF2 user audio was committed to or uploaded to
GitHub. ZIP test, per-member hashes, expected FUID, manufacturer,
vendor URL privacy and internal filename checks **PASS**.

**Private staging ZIP SHA256:**
`5279fa0dad2b82f79af8474a7efeb70832820bdd9b24ea7f4d614745ae4aae5f`.
**Actual Windows PE module SHA256:**
`699dbdb237cec32a3175dd9f0c3bbb3291770cbb73491f9b311afe7668ddab8a`.

**Release decision STILL NO-GO:** source+synthetic C chip testing
and private ZIP integrity do not substitute for original owner's
Win95 driver PCM/register side-by-side across real private banks,
actual in-DAW Studio Pro ASIO sustained 44.1k CPU/latency/playback,
and an independent full Codex inspection. Do not send user an
unverified installer or claim a finished plugin.
