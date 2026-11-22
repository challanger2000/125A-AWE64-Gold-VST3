# P198 — Original Creative SF2 filter-register quantization audit

**Candidate:** `fix/p198-original-creative-sf2-filter-register-conversion-20261122`. Private ROM, Creative binaries and user soundfonts are not published.

## Original driver evidence and scope

The owner-provided original Win95 SBAWE.VXD 4.38.0.2 SHA256 `d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a` and independent owner-private original driver descriptor reconstruction support **per-zone conversion before 16-bit saturating instrument/preset addition**.

SF2 generator 8 (filter cutoff) is converted with signed integer truncation `(x+15)/29 -153`, instrument default 313, preset converted offset `convert(x)-convert(0)`; final hardware cutoff clamps to 0..255. Generator 9 (filter Q) is converted `x/12` (both instrument and preset scope), summed and clamped to 0..15.

The old upstream source used `FillFrom` for both scopes and used its own `FilterFcFromAbsCents` and `round(rawQ/16)` conversion. This ignored original quantization and original SF2 preset offsets. The **SF1 / SBK / ROM paths remain unchanged**.

## Source-derived adversarial cases

- **Fc 100 register:** instrument x=11921, preset y=-4604 -> 258 + (-158) = 100. Equivalent all-instrument x=7322 -> 100. Counterexample all-instrument x=11921 -> 255, must differ.
- **Q 9 register:** instrument x=80, preset y=40 -> trunc(80/12)+trunc(40/12)=6+3=9. Equivalent all-instrument x=108 ->9. Counterexample x=0 ->0, must differ.
- **Default cutoff:** omitted generator8 = template313 -> hardware255. Explicit x=13500 also hardware255.
- **Default Q:** omitted=0, explicit x=0 -> register0.

Every public test bank has identical independent generated sample PCM, fixed NoteOn/NoteOff, program and velocity; only tested filter generators differ. The actual Win64 VST3 host checks each pair at three sample rates 44.1/48/96k and notes 60/72, including audible different-value negatives. Source VXD comparison is an **independent static driver-code reference**, not an actual replay from running Creative Win95 or physical AWE64 hardware.

A complete audit of remaining nonlinear generators, modulation routing, dynamic filter sweeps, and owner's real SF2 / ASIO performance remains **NOT VERIFIED**. PR #46 and P192 private install are not changed.

## Completed native Windows x64 result (exact product code commit)

[GitHub Actions 45036030923](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45036030923), exact implementation/QA source `c636c5feba38a7200befcafc0ba2c3ddaf596596`: **SUCCESS**.

- Verified `P198_SF2_CREATIVE_NONLINEAR_FC_Q_3RATES_2KEYS_AND_DEFAULTS_PASS` on the compiled real 86Box-EMU8000 Windows VST3, not an abstract simulated filter. At each 44.1/48/96k sample rate and notes 60/72, the split versus independently equivalent generator forms match **bit-identical PCM** for both Fc and Q (12 positive trials). The deliberately different true registers each produce different PCM (12 adversarial negative trials). Omitted vs explicit default Fc and Q match at three rates (6 positive trials).
- The prior upstream SF2 `FillFrom` and `round(rawQ/16)` must fail those split/equivalent cases by construction: for Q, upstream converted instrument Q80 to 5 and equivalent Q108 to 7 (where original VXD requires both at Q9); for cutoff, upstream saw instrument Fc11921 as effectively max255 while equivalent Fc7322 mapped about 101 (where original VXD requires both at Fc100). This is a **source-level old-bug counterexample**, not an independently executed pre-fix Windows VST3 red run.
- P192 sustain, P194 attack, P195 linear preset/instrument merge + negative controls, P196 sample-end offsets, P197 scale tuning: all existing mandatory host gates pass.
- Steinberg Validator **47/47 PASS**, plugin testers v0.2.10 and v0.3.3 **41 PASS / 0 WARNING / 0 FAIL** each.
- N08 **1,361,303** valid sounding saves with 0 stale, 0 mixed and 0 rejected. P179 **500/500** valid saves, 0 torn/failed, 700 blocks.
- This test set is **not** real owner Win95 SBAWE.VXD execution, physical CT4390 capture or Fender Studio Pro/ASIO; those remain **NOT VERIFIED**.

## Independently computed 13-user-bank static impact (not an audio PASS)

Read-only private SF2 corpus: 13 banks, 656 presets, 12,998 reachable preset/sample layers. Using the independently reconstructed original VXD cutoff/Q integer conversion compared to the exact previous upstream effective register formulas:
- **2,941 layer occurrences** differ in cutoff;
- **3,377 layer occurrences** differ in Q;
- at least one of the registers differs in **350 distinct presets** across the 13 banks.
- Specifically `Digital Bliss.sf2`: 1,736 cutoff occurrences, 944 Q occurrences; **67 presets** with one or more potential register differences.
- The affected-presets count is per-bank distinct-preset, not sum of altered layer occurrences. Counts are **static predicted register differences**, not a count of audibly wrong presets, physical hardware failure or proven original Win95 emitted PCM.

The private ROM and all user SoundFont bytes remained local, never embedded in the public source/repository. The static inventory input hash and scripts remain private.

## Final boundary

**TARGETED P198 NATIVE AUTOMATED QA: PASS. FULL SONIC AUDIT / OWNER MEDIA / ORIGINAL CREATIVE LIVE DRIVER / DAW ASIO: NOT VERIFIED. RELEASE: NO-GO.**

The remaining independent steps include physical/original Win95 multi-parameter note-on/held/release register captures, valid SF2 custom modulator routing (or documented hardware incompatibility), stereo sample-link semantics, dynamic filter/FX, original-PCM comparisons for owner-supplied banks, and only then real DAW CPU p95/p99/deadline profiling and optimization. No speculative CPU/DSP changes in P198; no change to PR #46 or owner-installed P192.
