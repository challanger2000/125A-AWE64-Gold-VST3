# P008 — Isolated native AWE32Emu benchmark (no Windows 95, no VM)

This experiment is on **research/awe32emu-native-isolated-20261120** from main.
It never edits P007's branch, its driver, original EMU8000 or audio DSP.

**Candidate:** turican0/AWE32Emu at **17b0578fe59909dfaa784b5531d6d1a3a80729ce**. Clone exactly this commit.
The candidate runs its own Creative Win95 driver *reimplementation* and an
EMU8000 chip C module originating from 86Box, **not 86Box as a VM**.

Public Linux + Windows CI compiles the native executable and runs a synthetic
fallback smoke test. CI has NO owner ROM, Windows95, original Creative driver or
private SoundFonts, so its green status does NOT constitute original audio QA.

The Linux artifact supplies the pinned upstream C++ source and standalone
executable. For **PRIVATE** authentic ROM / SBK / 77 preset SF2 / second SF2:

python3 research/native-awe32emu/compare.py private \
  --binary ./AWE32Emu-linux \
  --rom ./awe32.raw \
  --sbk ./SYNTHGM.SBK \
  --sf2 "./Synth 512 Bank.sf2" \
  --other-sf2 "./Vintage Dreams Bank(1).sf2" \
  --out ./private-native-results --sweep

This runs original ROM melody, original GM Channel10 kick/snare, two owner
soundfonts, velocity layers, 77 program positions and all 16 MIDI channels,
checking genuine nonzero PCM. Its threshold is a **screening** threshold,
not Creative bit-exact or real-card evidence. It deliberately does not copy
the proprietary input bytes to GitHub or into the public CI artifacts.

## Gates before any migration
1. Private native PCM output with 77-preset owner SF2 and original GM
   percussion must be non-silent, no synthetic fallback.
2. Reproduce same MIDI, program, velocity, control change and chip ROM
   against P007 reference **when P007 works**; retain missing evidence as FAIL
   or NOT TESTED, never pass it.
3. Verify safe realtime VST3 callback architecture: all ROM/SF2 loading
   must be outside the audio callback, no allocations/IO/lock in render;
   test note timing, 16-channel state, save/restore, multiple isolated
   instances, Steinberg validator, real Windows Studio Pro last.
4. Only after verified parity may the Win95/86Box VM be removed.

The candidate's GPL-2.0-or-later EMU8000 core creates corresponding
distribution/source obligations for a distributed VST3. Creative ROM/SBK
and Windows95 binaries MUST NOT be published with the GPL source.
