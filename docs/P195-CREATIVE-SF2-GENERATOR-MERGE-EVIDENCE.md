# P195 · Original Creative SF2 preset/instrument semantics and open validation

Scope: isolated source branch. **NOT** evidence of full AWE64 parity.
No private ROM, driver bytes, user SF2 contents or actual original audio in this document.

## Independent original Creative evidence

Owner-provided original Windows 95 `SBAWE.VXD` v4.38.0.2, SHA-256
`d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`,
examined privately, not published.

- LE object `LCOD`, `+0x48EE`: relocation to the copied generator-default
  table at `+0x2D88`, copied as 37 DWORDs.
- `LCOD+0x499B..0x49D8`: **converted** instrument and preset generator
  words are added, with `ADD ECX, EDI` at `+0x49AA` and signed-16
  saturation before storing. This is not equivalent to the upstream
  unconditionally using `GenSet::FillFrom`.
- Earlier 22,000-note upstream tests motivating `FillFrom` came from
  **SF1/SBK** presets, not an exhaustive SF2 original-driver test.

## Correctly restricted product change

The original driver adds generator quantities **after** their respective
instrument/preset transformations. For *linear* parameter families,
merging raw SF2 values before later conversion is equivalent (subject
to valid domain bounds). P195 therefore modifies **only**:
- generator 48 `initialAttenuation` (centibels);
- generator 51 `coarseTune` (semitones);
- generator 52 `fineTune` (cents).

For these three classes P195 adds overlapping values with signed-16
saturation. If a value is present at only one level, it is used
unchanged. The verified SF1/SBK behavior is not modified.

**NO blanket `r.gen.AddFrom(presetGen)` for SF2.** That shortcut was tried
on the initial research branch but withdrawn before any integration into
PR #46; it is not justified for nonlinear generator conversions.

## Independent 13-bank structural audit

Inputs: 13 owner-supplied SF2 files, 656 presets, 12,998 reachable layers
(only numerical metadata counted; actual sample audio not redistributed).

Overlapping instrument and preset parameter occurrences:
- `coarseTune`: 1,129 layers.
- `fineTune`: 953 layers.
- `initialAttenuation`: 136 layers.
- `initialFilterQ`: 1,356 layers.
- `initialFilterFc`: 317 layers.
- `modLfoToVolume`: 4 layers.

The private original-code-derived intermediate-conversion model gives
a *different internal generator value* for **921/1,356 filter-Q overlaps**,
**292/317 cutoff overlaps**, and **2/4 tremolo-depth overlaps** when
instrument and preset are separately converted before addition, rather
than raw-SF2-summed before conversion. Examples:
- `Synth 512 Bank.sf2`, “Wonderland Xylo”, Q instrument 100 + preset
  -110: raw-sum/convert = 0 versus convert/separate/add = -1.
- `Synth 512 Bank.sf2`, “Gated Screamer”, cutoff 11921 + (-4604):
  raw-sum/convert = 99 versus convert/separate/add = 100.

After applying only the expected original chip register bounds to the
same static model (cutoff 0..255; resonance 0..15), potential differences
remain in **178 cutoff-layer overlaps and 12 resonance-layer overlaps**.
Many of the larger intermediate counts disappear after hardware clamping.
The **12** resonance candidates include *Vintage Drums 2001.sf2*, preset
“Analog Kit II_”, instrument Q=80 and preset Q=40: raw-sum maps to Q=10,
separate conversions/addition map to Q=9 in the analyzed reference model.

These are **static intermediate-level counterexamples**, even when the
clamped register value differs; they do NOT yet establish a matching
original Windows 95 note-on register trace or audible PCM difference.
A direct Creative original-driver oracle is needed before fixing nonlinear
families.

## Tests and release discipline

P195 synthetic positive/negative tests use pairs of otherwise identical
public SF2 banks, exercising separate preset+instrument generator values
against an equivalent all-instrument combined value, using real Windows
VST3 PCM at 44.1/48/96 kHz. Intended gates are 3 generator families x3
rates = 9 comparisons, plus P194 Volume/Mod Attack and P192 Sustain.

The final tightened code-level gate on commit
`a450f128a4c7832a6e432cea83498e51a981266c` was verified in
[Windows Actions run 45029110478](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45029110478):
- P195 3 generator classes x3 sample rates: **9/9 identical PCM comparisons**
  for semantically equivalent SoundFonts, plus **9/9 discriminating
  negative controls** (the third SF2 has a deliberately changed parameter).
- P194 SF2 Volume/Mod Attack, P192 Sustain, legacy native chip/host and
  installer gates: PASS.
- Steinberg Validator **47/47**, 125A Plugin Testers v0.2.10/v0.3.3 each
  **41 PASS / 0 WARNINGS / 0 FAILURES**.
- N08 **1,485,066 sounding saves**, zero stale/mixed/rejected; P179
  **500/500** valid saves.

This verifies the restricted P195 **synthetic test set**, not all SF2
generator combinations.

The 13-bank static corpus cannot substitute for Win95 Creative driver
register/PCM parity. Steinberg Validator and 125A Plugin Tester scores do
not confer musical fidelity. **No merge / release of P195 without native
Windows evidence and final private original-driver/DAW acceptance.**

CPU cleanup remains separate and postponed until all relevant sound
invariants are pinned. Unused duplicate native core work at 44.1 kHz
is an established source-code finding but has not been profiled in DAW.
