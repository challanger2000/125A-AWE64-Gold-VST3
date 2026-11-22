# P019 – Original EMU8000 timing / low-latency feasibility

**Date:** 20 November 2026. Separate AWE32Emu research, P007 unchanged.

## Why P018 still has 980 samples of VST3 latency

The pinned AWE32Emu `Emu8000Box.h` explains the exact mechanism:
86Box renders the EMU8000 in `WTBUFLEN = 980` native 44.1 kHz frames.
The reconstructed Creative driver produces register writes at arbitrary
frames. The chip wrapper must enqueue those writes, apply them with their
sample-accurate offsets inside a complete window, call
`emu8k_update()`, and output *the previous completed window*. A
frame-by-frame emu8k_update is not known to preserve the original audio,
especially the original chorus/reverb behavior and active-voice gating.

Therefore a native 44.1 kHz VST3 has 980 output samples FIFO delay,
or 22.22 ms. It is NOT an extra Studio Pro host buffer. At other sample
rates it is converted to the host clock (e.g. 1067 at 48 kHz, 2134 at
96 kHz). Simply reporting zero to the DAW would misalign recorded MIDI,
automation and rendered audio; it would not remove physical delay.

## Exact experimental change, not released

P019 adds a strict, pinned-header test-only switch
`P019_EMU_BLOCK_FRAMES` (980 default) in
`research/native-awe32emu/p009-vst3/p019_latency_window_probe.py`.
Three separate, standalone `P019Probe980`, `P019Probe128`, and
`P019Probe64` executables link the same Win95 driver and original
86Box-derived chip. The shipping VST3 **always compiles with 980**
(default); none of the lower-latency variants is exposed in the VST3.

The probes exercise a synthetic 1MiB null ROM, a public two-layer SF2
with non-silent audio, notes, note-offs, pitch bend, modulation, and
both zero-send DRY and nonzero-send FX configurations. Exactly matching
MIDI events are injected into each test; 980/128/64-window raw int16
stereo PCM is captured.

The comparator `tests/p019_compare_pcm.py` aligns each trace by its
one-window FIFO delay and computes sample-exact identity, max and RMS
error (LSB), exact fraction and residual RMS in dB. The report is
machine-readable and archived in GitHub Actions.
**No audio equivalence should be declared until real CI metrics are
read and validated; a successful probe build alone proves nothing.**

| Window at 44.1 kHz | One-window latency | Status |
|---|---:|---|
| 980 frames | 22.22 ms | Original unchanged, owner PCM P018 PASS |
| 128 frames | 2.90 ms | Experimental only, PCM comparison required |
| 64 frames | 1.45 ms | Experimental only, PCM comparison required |

A separate real VST3 host regression explicitly checks that the
*shipping* plugin reports its true 980 native frames across supported
sample rates, with values derived from `Emu8000Box::kLatencyFrames`,
not an unrelated magic constant.

## Release guardrails

- No invisible zero-latency report, compensation hack, artificial tones,
  sample normalization, global limiter, resampler change or 86Box
  algorithm rewrite.
- If dry or FX PCM changes relative to the 980-frame fixture, short
  windows are **not production-eligible without finding and correcting
  the cause**. Even an exact synthetic test is insufficient to claim
  fidelity with original Creative Win95/real owner ROM.
- Next gates: original owner ROM/SBK/SF2 with matched input and
  compensated PCM; genuine Win95 Creative register/PCM parity; voice
  allocation, simultaneous polyphony, FX sends, sustain, short notes,
  16 channels and 8–192k host rates; audio callback heap and race tests;
  final Studio Pro acceptance at the very end.
