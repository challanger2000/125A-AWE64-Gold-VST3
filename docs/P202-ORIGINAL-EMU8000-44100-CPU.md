# P202 – Native 44.1k 86Box CPU duplicate renderer

**Baseline and provenance:** P201 development \`79e561924fd9a4e483eb03137bc4559ff52805d3\`; P202 isolated \`fix/p202-emu8000-44100-double-render-cpu-20261122\`; pinned upstream \`turican0/AWE32Emu@17b0578fe59909dfaa784b5531d6d1a3a80729ce\`. Stable main, owner-installed P192, and draft PR #46 untouched.

**Original source fact, not conjecture:** \`Emu8000Core::RenderBlock\` first performs \`RenderNative\` over **all 32 software voices and alternative C++ chorus/reverb/EQ**, then when \`Chip::Box86\` active computes **original** \`m_box.RenderFrame\` and only sends the latter PCM to the DAW. This doubles the synth traversal for 44.1 kHz and discards the first output. Existing P021 overlay bypasses the alternate renderer at **other** output rates. P202 removes only discarded DSP at 44.1k, preserving existing \`m_waveCounter\` and \`m_traceFrames\` increments; \`UpdateRegistersFromState\` of the discarded renderer manipulates only shadow C++ state with TraceOff, not original chip hardware. Direct \`ReadDriver\` reads the original 86Box chip; no normal \`Synth.cpp\` call to \`m_core.Read(\` was found in the pinned upstream.

**Independent source comparator:** Compile two Windows binaries from the identical pinned original native Synth/86Box sources, with \`P202_CPU_LEGACY_REFERENCE\` enabled **only on the pre-fix reference executable**. Both execute the original EMU8000 with synthetic ROM and independently generated sustained SF2, 1/16 keys (two physical voices per note), blocks 64/256, host sample rates 44.1/48k. Eight full-chip PCM cases and 320 measured callbacks per case with 45 warmup callbacks, in **two repeated before/after pairs on the same Windows CI runner**, require exactly equal stereo sample hashes and sample energy. A test-only PCM bitflip must be detected. Measure mean/p95/p99/max, deadline/overruns, timer overhead; require at least two dense 44.1k cases show >10% p95 reduction. Preserve existing 47/47 validator, 125A Testers, exact P199/P200/P201 Stereo, P021 multirate, RT callbacks and host save/recall regressions.

## Completed actual Windows P202 regression and CPU measurements

Exact **tested binary/source commit** `82fdf4fe0c825e9c7d04f47371557c5e80180792`; [Windows Actions #45047261012](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45047261012), **SUCCESS**. This report is a separate DOCS-only commit and does not alter that tested product SHA.

Positive actual 86Box EMU8000 stereo PCM: **8 configurations × 2 independent before/after repetitions, exact FNV sample hashes and nonzero stereo energy**, including 44.1/48 kHz, 64/256 frames, one-note vs 16 simultaneous keys (2 vs 32 hardware voices), 320 actual block renders each and 45 warm-up blocks. Explicit test-only first-block LSB mutation changed the PCM reference hash in every case and was correctly rejected. None of the comparisons uses the alternative software synth as its reference; both link the same genuine original 86Box `snd_emu8k.c` chip.

| Native chip test (16 notes / 32 actual voices) | Legacy p95 | P202 p95 | Relative p95 time reduction | Legacy p99 | P202 p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 44.1kHz, 64 frames, repetition 1 | 993.8 µs | 67.8 µs | **93.18%** | 1,046.7 µs | 72.1 µs |
| 44.1kHz, 64 frames, repetition 2 | 1,057.7 µs | 66.5 µs | **93.71%** | 1,238.6 µs | 76.9 µs |
| 44.1kHz, 256 frames, repetition 1 | 2,850.2 µs | 336.7 µs | **88.19%** | 3,640.0 µs | 348.4 µs |
| 44.1kHz, 256 frames, repetition 2 | 3,283.5 µs | 264.4 µs | **91.95%** | 4,067.8 µs | 274.3 µs |

P202 specifically changes **44.1 kHz**. At 48 kHz both programs already use the exact P021 genuine-chip path; baseline/candidate times are approximately equal with occasional substantial Windows shared-runner scheduling noise. All 16 observed before/after scenarios reported **zero nominal deadline overruns** as measured offline; this is **not** a paced host/ASIO overrun claim. At 44.1kHz/16 notes the PCM hashes include `bd718683ef2d1e0a` (64 frames) and `d059c3bcbf30dabf` (256 frames) for both implementations and both repeats.

**Prior regression gates survived the patched production VST3**:
- Genuine native Windows VST3 build and Steinberg Validator **47/47**, with unchanged class FUID.
- 125A Plugin Tester published 0.2.10 **41/0/0** and development 0.3.3 **41/0/0**.
- P009 audio/MIDI callback zero heap at 8, 44.1, 48, 96 and 192 kHz and simultaneous instance/thread migration.
- P016, P019 and P021 original-chip PCM, and P199 linked stereo SF2 3-rate causal PCM comparisons.
- P200 and P201 genuine EMU8000 port-order stereo-pair/mono negative tests.
- The actual Windows VST3 P192–P199 host and project/PCM regressions; N08 **1,098,531 sounding accepted state saves, 0 stale, 0 mixed, 0 rejected**. P165 EditorHost GUI and P189 interrupted installer target integrity PASS.

### Verification boundary

**P202 scoped authentic chip playback + Windows automated CPU correction: PASS.** The **93% p95 reduction is a native 44.1-kHz same-runner synthetic dense-bank benchmark**, not a generic CPU meter reduction in the owner's DAW, not a 48/96k speedup, and not a claim of all host callback deadlines. The original Creative Win95 driver was NOT executed anew in this test, and the synthetic ROM is not the owner's genuine ROM. Original 25-pair driver traces and original-bank sonic parity, all SF2 modulators, original ROM/SF2 hardware audio, actual Fender Studio Pro p95/p99/max ASIO and independent Codex full audit remain unverified. This particular benchmark proves exact PCM behavior only under its declared input/engine/timing state. Original hardware/DAW/asset/licensing gates and independent audit remain **RELEASE NO-GO**.
