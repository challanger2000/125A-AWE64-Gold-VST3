# P008 — Measured native AWE32Emu comparison, 20 November 2026

This is an isolated research branch. P007 and original Windows 95 files
were not changed, migrated or deleted. Exact upstream SHA:
17b0578fe59909dfaa784b5531d6d1a3a80729ce.

**Both GitHub Actions baseline native builds PASSED**: Windows x64 and
Linux with original 86Box-derived EMU8000 C chip but NO virtual 86Box machine.

Actual private owner ROM/SBK/SF2 tests were run ONLY locally:
- 1MiB owner ROM SHA256 4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e
- Original SYNTHGM.SBK 34,832 B SHA256 fd2e35a260fef22fe809918dda3c921c6d48d53a29c3515af8a41019ce72d586
- Owner Synth 512 Bank.sf2 555,754 B: 77 real preset entries
- Vintage Dreams Bank(1).sf2 315,982 B

**9/9 initial real private cases PASS**, including GM-ROM kick CH10 RMS
0.004385582 and GM-ROM snare CH10 RMS 0.004045817. P007BV did NOT play
either of these ROM drums (genuine Win95 guest). User SF2 and velocities
30 and 127 also produced real native nonzero PCM.

**102/102 total private native cases PASS**: all 77 preset program positions
produce audible PCM; all 77 have distinct PCM SHA256 fingerprints; all
16 channels pass separately and selected sounds and velocities pass.
One separate actual simultaneous 16-channel MIDI take produced 16 distinct
hardware voice allocations with 28 MiB configured DRAM.
This took 0.45 seconds wall CPU for 3.02 seconds of offline WAV, on one
container CPU. It is NOT a formal DAW/plugin benchmark.

**Authentic multi-instance FAILURE in unmodified source**: concurrent
same-process instances intermittently disagree with their own sequential
original-ROM+SBK PCM hashes. The upstream uses process-global
wavetable_pos_global, music_pos_global, and g_romPath. In isolated local
Linux test, making these thread-local fixed the observed issue on 8/8
parallel runs, without changing EMU8000 mathematics. This is only an
experimental patch, not a Windows or Studio Pro verification.

**Latency:** the native chip wrapper buffers 980 frames, which is 22.22 ms
at 44.1 kHz. CLI WAV export compensates, live VST3 will need redesign.

**Realtime safety:** current code contains heap allocations in original
MIDI/render callback paths (Emu8000Box pending.push_back, Synth region
selection vectors). These need bounded/preallocated replacements before
a final DAW VST3 implementation.

**NOT PROVEN:** P007 Win95 vs native AWE32Emu *same-input* register/PCM
bit parity; multi-instance Windows VST3; realtime automation/project
recall; Steinberg validator for native port; Studio Pro audition.
Unchanged P007 has silent ROM drums and cannot serve as a correct
reference for that failure.

**Disposition:** promising native architecture; DO NOT remove or replace
P007 before true signal parity and realtime/plugin acceptance.
