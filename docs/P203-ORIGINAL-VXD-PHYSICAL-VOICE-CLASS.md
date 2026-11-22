# P203 — Original Creative Win95 physical voice modes and adjacent stereo selection

**Source ground truth:** Original source-derived Win95 SBAWE.VXD allocator in this same repository, branch `codex/p005-vxd-full-port`, `OriginalCreativeVxdLive.cpp` and `OriginalCreativeMidiEngine.cpp`. Native candidate base is P202 branch `4dc09a06aadf50a5e5b4d2176d650417805f8d11` (P202 VST3 tested executable `82fdf4fe`). No original licensed VXD, ROM or owner's SF2 placed in source or public CI.

**Four proven source divergences in P201:** (a) P201 uses the number of CURRENT sounding ROM notes instead of original device+0x18 mutable `mode_count`, seeded with 32, tracking physical ROM/RAM class transitions; (b) P201 clears the ROM class on voice release instead of preserving original row+0x17 blocked flag; (c) P201 rejects a candidate when the *neighbor's key marker* is >=0x2000 although the real code checks only neighbor's protected class; (d) P201 omits row+0x1A bit0 penalty `0xC00` for MODE1 pair. A fifth safety issue: mono code wrongly took voice0 if all protected, instead of returning NO_CANDIDATE.

**Correction:** source-based mutable `m_vxdModeCount`; original persistent per-voice `vxdBlocked` and `vxdPairPenalty`; physical class based on sample address at hardware ROM/RAM boundary `0x200000`; pair rank corrected; mono failure returns -1 and MIDI caller skips it safely. The actual original 86Box EMU8000 DSP source remains untouched, as do original VST3 processor, parameter IDs, GUI and state serialization.

**6-case original-chip independent source oracle:** C0 physical count 32→33→32 with repeated same-class idempotence; C1 mode=32 flagged 30/31 allowed; C2 mode=1 flagged 30/31 skip, first immediate physical base=27; C3 all 32 blocked no candidate both allocators; C4 reserved upper KEY not neighbor veto; C5 0xC00 penalty changes tie from base0 to base1. Real chip registers read by both standalones using original 86Box C core. Negative executable compiles only the bad old-protection predicate and must fail C2/C3 for the correct reason. The same workflow also executes P202 exact PCM and CPU comparison, 125A QA and standard P199-201 tests.

## Native Windows verification — actual compiled VST3 / source

**Exact test run:** [P203 Windows Actions #45052572924](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45052572924), **SUCCESS** on workflow/QA-fix commit `6f3a0357b2c999776db5c6f6bc31209613d7c410`. **Exact product-code SHA:** `983c2830add0418d04b4d12986039c95b00613f5`; the later commit changed only workflow PowerShell test status and did not modify DSP, VST3 or C++ test sources.

- **Six actual source-derived physical allocation cases C0–C5 PASS** in an independent compiled Windows harness using the same reconstructed original Win95 Synth and original 86Box chip as the VST3: mutable ROM/RAM mode count; pair 30/31 in normal state; physical pair 27/28 with protected upper rows; no illegal stereo/mono voice when all 32 blocked; neighbor KEY does not veto; row+0x1A pair penalty correctly changes priority winner.
- **Failing TEST-ONLY mutant correctly rejected:** `P203_VXD_MODE1_FAIL C2 ...` from intentionally disabled physical protection. The negative executable is never compiled into the product VST3.
- **Retained P202 genuine 86Box 44.1/48k, 2/32-voice old/new PCM exact-match and paired CPU regression PASS**. Unmodified native Steinberg Validator **47/47**, 125A published Tester **41 PASS/0 WARN/0 FAIL**, 125A development v0.3.3 Tester **41/0/0**.
- Original-chip P016/P019/P021 DRY/FX PCM, P199 linked-stereo 3-rate PCM with negative controls, P200/P201 4-case physical stereo/mono port order, actual native 86Box VST3 SF2/host regression, P165 GUI EditorHost, and P189 interrupted installer integrity PASS.
- N08 sounding host snapshots: **958,105 accepted saves / 0 stale / 0 mixed / 0 rejected**, finite positive original-chip energy.
- A first run [#45052209746](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45052209746) FAILED at test-harness reporting: Windows PowerShell inherited the **expected nonzero exit code** from the correctly rejected mutant, despite printing the positive marker. CI fix `6f3a035` resets that code **only after validating the intended mutant failure**, preserving the genuine negative control. Its corrected full run is green.

### Assurance limitations and next-source issue

**P203 targeted Windows source/register-structure functional acceptance: PASS. Overall AWE64 product RELEASE NO-GO.**

The original Win95 guest's *live* 25-pair trace corpus was not recaptured. The real VXD may SET the row+0x1A pair-penalty flag through additional driver code not yet modeled here; P203 applies the documented bit when set, but its origin and changing high-load state are NOT VERIFIED. Original-ROM/SBK/SF2 PCM parity for diverse presets, all modulators, broader 32-voice stealing schedules, original guest register values/clock cycles, actual Studio Pro ASIO callback CPU, licensing, and independent Codex release audit remain **NOT RUN / NO-GO**.

**Newly exposed adjacent issue:** Source `third_party/creative-vxd-port/src/OriginalCreativeMidiEngine.cpp` allocates **all mono/stereo hardware voice prologues before any note-main register programming** and then triggers independent mono layers sequentially. Current reconstructed `Synth::StartLayers` interleaves mono allocation and main programming. Existing P200/P201 assertions prove main/trigger chronology but not allocation-vs-main chronology. This is a **SOURCE-FINDING, NOT A PASS**; it requires a separate source-derived original-port ordering oracle and correction, without undermining already verified mono/stereo timing.

No private Creative assets in public CI, no owner-installed VST3 replacement, no distribution, and no merge of draft PR #46.
