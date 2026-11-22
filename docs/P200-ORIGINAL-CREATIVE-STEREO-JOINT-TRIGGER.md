# P200 — Authentic Win95 stereo pair trigger ordering

**Baseline:** P199 isolated development branch, exact head \`ec5251726d1327ed78ac15c4bbcb163816c90031\`. P200 branch \`fix/p200-original-creative-stereo-joint-trigger-20261122\`. Stable main, active PR #46 and private installed P192 are untouched. This is **not** a release.

## Previously missed original-driver divergence

Original owner Win95/86Box Creative SBAWE.VXD v4.38.0.2 test archive \`P003-20261116T014751Z-3594ea53-ERGEBNIS.zip\` was privately audited by \`audit_owner_25_stereo_sequence.py\` and \`audit_original_owner_stereo_pairs.py\`. The associated independent C++23 port's \`START-HIER.md\` and \`PORTIERUNGSSTAND.md\` report **25 real linked stereo hardware pair starts** (including physical voices 29/30). For linked pairs the real Creative guest writes **both voices' main register blocks first, then both DCYSUSV note-on triggers**. For a two-layer mono instrument, the first DCYSUSV trigger precedes the second voice's complete main block. This is independently reported original hardware control behavior, not a conclusion inferred from our VST3 tests. Original licensed traces/binaries stay private and are **not rerun in public CI**.

P199's `Synth::StartLayers` called `StartVoice` sequentially and `StartVoice` always emitted DCYSUSV immediately after its own main block. This made P199 wrong for stereo even though its one-zone linked-vs-two-zone stereo rendered exactly the same **wrong-ordered** PCM.

## Minimal repair and direct negative oracle

The P200 source overlay only changes the Win95/SF2 reciprocal stereo path. Select two matching SoundFont sample link halves without allocating on the audio thread, allocate hardware voices, finish both main voice programs with a deferred DCYSUSV, and only then issue the two triggers. Mono, SF1/SBK, ROM, original EMU8000 DSP and unrelated VST3 control/state paths are not modified.

The native Win64 compiled \`P200StereoTriggerTrace.exe\` links the **same pinned+patched original EMU8000/Synth.cpp** as the production VST3. It calls actual physical chip port write functions and parses \`Core().OpenTrace()\` output, identifying main completion via CPF's high word (Data0 register 0) and start via actual DCYSUSV Data1 register 5. Four independently distinct public fixtures test:
1. one left-linked SF2 stereo zone, two jointly triggered physical voices;
2. explicitly mapped left/right paired SF2 zones, same;
3. unlinked mono SF2 note, single immediate start;
4. independent two-layer mono SF2, strict first-trigger-before-second-main ordering.

This oracle is genuinely discriminatory: **before P200** the stereo pair produces first-trigger-before-second-main and must FAIL; after P200 the pair must pass while the mono negative control would fail if every two-layer patch were grouped. It checks real core port **order**, not merely a PCM hash or successful bank load. P199 PCM and all existing Windows VST3 tests are retained.

## Evidence and scope

**Before new Windows CI:** NOT RUN. Only the archived original Win95 evidence is preexisting, and source inspection establishes the P199 misordering. No claim of a live original-VXD-vs-new-compiled-VST3 register-by-register comparison.

**Still unverified:** exact original Win95 paired physical voice adjacency/allocator choice, exact per-register timestamp and numeric parity against owner original stereo SF2 bank, pan/controller behavior, owner ROM/SBK/SF2 audio, ASIO CPU deadlines, original stereo samples with more than 256 simultaneous regions, all SF2 modulators, independent Codex full audit. Release remains **NO-GO** until those gates pass.
