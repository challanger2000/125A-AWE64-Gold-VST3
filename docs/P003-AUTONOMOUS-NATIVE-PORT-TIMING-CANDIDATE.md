# P003 autonomous MIDI to EMU8000 port timing — RESEARCH ONLY

**No product release, no general driver PCM parity claim.**

## Reference retained

Owner-run 2026-11-15 original Creative Win95 + exported Windows VST3 produced
**160,000/160,000 identical dry stereo PCM16 channel samples** when independent
native-compiled port/value pairs were executed at the original source-clock
offsets. This reference mode remains unchanged. The original EMU8000 core,
original chip-state prelude and exact 260,921 source RAM words are unchanged.

The instant/native version writes all 152 or 156 NoteOn OUTs at sample zero
and does not match the real original multi-frame timing.

## New autonomous candidate

The environment flag AWE64_P003_AUTONOMOUS_OUT_PACING=1 is opt-in, private,
and OFF by default. It requires the authentic native RAM/VXD inputs and the
original MIDI-to-register compiler; original note-port *time offsets* are
neither loaded nor read in this mode. The speculative chip/voice state
transaction is restored before the bounded independent native OUT writes are
played by the existing per-sample scheduler. No new DSP or mutable original
EMU8000 core.

The earlier 200 MHz virtual CPU / 44.1 kHz chip-clock candidate distinguishes index
OUTs from data OUTs and includes a structural initial-voice allocation
workload. **Empirical coefficients 20, 67, 2020, and 291 cycles were fitted
to the 2026-11-15 capture itself.** They are NOT verified original Win95
Creative VXD instruction costs. The engine rejects unsupported note port
patterns and never runs in the default product path.

## Authentic driver cross-session variation

| Event | Original OUT count | 2026-11-14 frames | 2026-11-15 frames |
|---|---:|---|---|
| C4 NoteOn | 152 | 6 / 6 / 80 / 60 | 6 / 6 / 53 / 87 |
| C4 NoteOff | 8 | 8 | 8 |
| D4 NoteOn | 156 | 5 / 11 / 24 / 87 / 29 | 8 / 8 / 30 / 93 / 17 |
| D4 NoteOff | 8 | 8 | 8 |

An offline differential of the candidate's independently reconstructed
port timing against both authenticated originals gives:

- **Calibration 2026-11-15: 324/324 exact port frame positions.**
  Training-set fit only; NOT a validated universal driver clock.
- **Holdout 2026-11-14: 276/324 exact port frame positions.**
  This counterexample falsifies any claim that the fixed virtual phase
  generally produces bit-exact Creative Win95 register timing.

A fresh native VST3 WAV/trace must still be captured with the autonomous
mode to prove even the calibration-case PCM equality. Synthetic unit tests
and GitHub CI alone cannot establish private original media parity.

## Next independent proof

Reconstruct original VXD x86 MIDI handling instruction-level CPU work and
the 86Box fractional chip-sample phase between note events. Do NOT fit
separate presets, pitches, MIDI note IDs or source note timestamp tables.
Check multiple truly independent original run traces with distinct notes,
velocities, controllers and banks. Until that cross-session differential
passes, retain the hard-reference mode and private-only autonomous flag.

**Gate: autonomous register/PCM identity NOT YET VERIFIED.**

## Independent original owner V3 physical CPU + EMU8000 evidence, 2026-11-15 18:43 UTC

**Source**: owner-supplied `P003-20261115T184307Z-5fd6f4a2-ERGEBNIS.zip`,
ZIP SHA256 `ddaa0625cf0d984bad2f9cc2dd56f128a86734c22d638e1bf310526bd16afe7f`.
Full private files (original Creative driver, sample data, audio and CPU trace)
are NOT published to GitHub.

Independent read and SHA256/ZIP-CRC validation:
- **816,594/816,594 source 86Box EMU8000 callback events joined in strict
  original order to guest x86 CPU-port callbacks, with no event omissions**.
- **760,209 register writes, 56,385 register reads**. All 816,589
  non-byte-read values agree fully; the five byte-read CPU callbacks
  expose only the low byte (which independently matches the source),
  while the original chip trace carries their full 16-bit result.
  Never overstate those five as a 16-bit full-value comparison.
- **0 CPU-TSC regressions**, Win95 guest and original Creative API PASS,
  44.1kHz/stereo/PCM16 original WAV present. This proves original
  measurement coverage, **not independent native VST3 PCM parity**.
- The owner Win95 VM CPU clock is **133,333,333 Hz**, not 200MHz.
  The 200MHz candidate coefficient units were corrected in code
  (133333333 Hz, 13/45/1347/194 research cycles). The coefficients
  remain fitted approximations, not original VXD instruction counts.

The source NoteOn and NoteOff port **values and order** are exactly those
of the separately authenticated 2026-11-15 original P003 reference:
all **324/324** original note OUTs, independent session, no newly added
native melody/velocity reference yet.

| Note event | Writes | V3 44.1kHz frame histogram, starting at event | Measured guest TSC span |
|---|---:|---|---:|
| C4 NoteOn | 152 | 6 / 12 / 88 / 46 | 9,500 |
| C4 NoteOff | 8 | 8 | 331 |
| D4 NoteOn | 156 | 8 / 14 / 86 / 48 | 10,058 |
| D4 NoteOff | 8 | 6 / 2 | 325 |

At 133.333MHz one EMU8000 sample is ~3,023 guest CPU clock cycles.
C4 NoteOn's observed 9,500 TSC ticks span ~3.14 sample periods;
D4 NoteOn's 10,058 ticks ~3.33 periods. The note-off sequence can
**cross a sample boundary** depending on CPU/chip fractional phase;
therefore the existing candidate assumption that every NoteOff's
eight writes are all in frame zero is also independently falsified.

The current 133.333MHz-rescaled, sample-equivalent original 15 November
research candidate was checked offline against the NEW source trace
without using the source timing as generator:

| Event | Correct autonomous offsets / total |
|---|---:|
| C4 NoteOn | 105 / 152 |
| C4 NoteOff | 8 / 8 |
| D4 NoteOn | 71 / 156 |
| D4 NoteOff | 6 / 8 |
| **Independent V3 holdout total** | **190 / 324** |

This is a **FAIL of the autonomous research clock model**, not an issue
with original Creative PCM emulation. The earlier sample-exact
`source_clocked` VST3 comparison (160,000/160,000 PCM16) is preserved.

**Engineering direction**: use real guest CPU TSC plus captured CS:EIP and
remaining x86 instruction cycles to derive source-validated instruction
work and fractional sample-clock phase for generic note/voice/controller
operations. Do not replace a dynamic driver scheduler with hardcoded
C4/D4 frame tables or recorded source offsets, and do not promote the
model to normal VST3 playback without independent new MIDI/PCM trials.

**Gate unchanged: autonomous original-driver clock and PCM parity NO-GO.**
