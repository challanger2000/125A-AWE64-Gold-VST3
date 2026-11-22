# P199 — Reciprocal SF2 stereo samples, original EMU8000 VST3

**Candidate:** `fix/p199-original-sf2-stereo-linked-pair-20261122` from exact P198 `c636c5feba38a7200befcafc0ba2c3ddaf596596`. This is **not** a new release and does not modify PR #46, the installed P192, the original 86Box EMU8000 DSP, the SF1/SBK path or VST3 state IDs.

**Source defect:** Pinned `turican0/AWE32Emu` SF2 parser dropped both 16-bit `wSampleLink` at shdr byte 42 and the non-ROM `sfSampleType` at byte 44. A legal stereo pair referenced by only one instrument zone consequently loaded one mono sample. A second linked sample's PCM could not reach the original chip. The published SoundFont 2.01 technical specification requires simultaneous linked left/right playback. This is **format-specification-derived**, not yet directly validated against the original Creative Win95 SBAWE.VXD execution.

**Correction:** Retain left/right sample link metadata, check reciprocal partner IDs, direction, equal lengths and rates, and select two original-chip voices if there is only one active instrument zone. If both sides have explicit matching zones, do not double them. Force left/right sample pan in the existing hardware pan representation. Keep all mono and SF1 logic unchanged.

**Adversarial native Windows host test:** Independent synthetic SF2 generator creates four banks with exactly the same LEFT PCM: linked with distinct RIGHT, two explicit matching stereo zones, linked with a zeroed RIGHT, and unlinked mono LEFT. The real built Windows VST3 must emit nonzero EMU8000 PCM and bit-identical single-zone vs explicit-zone stereo, but different PCM when the RIGHT content is zero or linking is disabled. Run 44.1, 48, 96 kHz and notes 60/72. The old loader will fail because it cannot play the right sample. The previous P192–P198, VST3 Validator, 125A Testers and N08 host-state gates stay enforced.

## Completed native Win64 result — exact production/test commit

[Windows Actions #45040486421](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45040486421) finished **SUCCESS** at code/test commit `68103a28bd4f2a877b34d4078ca56e0a3af14d6f` (2026-11-22). This document-only follow-up does not change the tested product code.

- **6/6** actual compiled Win64 VST3 stereo-pair equivalence cases: 44.1/48/96 kHz × MIDI notes 60 and 72. Every one-zone linked-left case matched the explicitly mapped left-plus-right case **bit-identically**.
- **12/12** contrasting-source negative comparisons: in each of six settings, muting the right sample or unlinking the pair produced **different audible real-EMU8000 VST3 PCM**. This tests audio source causality, not merely a successful parser call.
- P199 native test marker: `P199_LINKED_SF2_STEREO_NATIVE_3RATES_2KEYS_CAUSAL_PCM_PASS`.
- Existing P192–P198 native SF2 host checks remained green, including `P198_SF2_CREATIVE_NONLINEAR_FC_Q_3RATES_2KEYS_AND_DEFAULTS_PASS`.
- Steinberg VST3 Validator stage PASS. Both actual Windows 125A Plugin Testers (v0.2.10, v0.3.3) report **41 PASS, 0 WARNING, 0 FAIL** each; real EditorHost lifecycle and P189 installer fail-closed checks PASS.
- Sounding N08: **1,471,924 accepted host saves**, stale=0, mixed=0, rejected=0. P179 paced audible state saves **500/500 PASS**.
- Original pre-P199 branch's right-hand sample was inaccessible by static source inspection; this is not a separately executed **pre-fix Windows red** test.
- The current suite uses synthetic public SoundFonts, not the owner's Creative ROM/SBK/SF2 nor a running real Win95 driver.

## Explicit fidelity caution

The SoundFont 2.01 specification defines left/right sample links and coherent stereo playback. It does **not** independently establish how the specific original Creative SBAWE.VXD v4.38.0.2 handles a stereo partner without its own explicit instrument zone, or its exact link-validation behavior and pan/register write order. Those assumptions in this candidate require independent original-driver note-on/held/release register traces and realistic original stereo banks. Until then, P199 is **scoped native QA PASS, original AWE64 sonic-fidelity NOT VERIFIED**. Do not merge it into the integration PR merely because automated synthetic audio matches these internal equivalence pairs.

**Still NOT VERIFIED / release NO-GO:** Original Creative Win95 stereo register/voice ordering and controller interaction, hardware PCM, arbitrary malformed SF2 stereo/ROM references, owner real SF2 bank, Fender Studio Pro/ASIO, performance p95/p99/max/deadlines, full modulation audit and independent Codex review. No private/licensed assets in this branch.
