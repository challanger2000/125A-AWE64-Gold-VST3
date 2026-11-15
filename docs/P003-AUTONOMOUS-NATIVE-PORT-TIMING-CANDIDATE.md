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

The 200 MHz virtual CPU / 44.1 kHz chip-clock candidate distinguishes index
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
