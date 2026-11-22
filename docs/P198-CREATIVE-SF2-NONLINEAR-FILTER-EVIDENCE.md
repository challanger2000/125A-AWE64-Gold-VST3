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
