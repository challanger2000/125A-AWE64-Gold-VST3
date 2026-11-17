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

## P003 experimental independent CPU-cycle scheduler after owner review, 2026-11-15

Owner's independently supplied `P003-Original-Creative-CPU-Zeitmessung-AUSWERTUNG(1).zip`
contains 324 full physical port+value events (as offline-private CSV),
a summary of the separate original 2026-11-15 18:53:12 Creative driver capture,
and an original 8/16/32-bit CPU-IO / 44.1kHz register sample-clock audit.

**No private CSV, source Win95 WAV, original ROM, or Creative VXD bytes
have been committed.**

Replaced the former 200MHz artificial OUT-group frame layout in
`CreativeAutonomousOutTiming.cpp` with a **bounded 133,333,333-Hz guest CPU
cycle estimator**, based on register protocol STRUCTURE rather than
pitch/program/source note ID:

* Identify two native voice-search completion groups from the native
  620/622-register transactions; model the observed ~2.3k cycle
  driver-work bridges between voice searches and main programming.
* Model subsequent independent index/data word transaction costs. Native
  register words remain byte-for-byte from the existing voice compiler.
* Support a **fractional CPU phase within a 44.1kHz sample** in
  `p003CandidateAutonomousNoteTimelineAtPhase`. An eight-register NoteOff
  now can legitimately split across two samples, rather than being
  incorrectly forced into the first sample.
* Full bounds checks, sample-clock monotonicity, no heap per callback,
  and fail-closed rejection of unsupported voice transaction layouts.
* The standard opt-in VST3 experimental wrapper uses a **deterministic
  half-sample placeholder phase**. This is **not** the actual Win95
  scheduler phase; a sample-accurate VST3 host MIDI event does not
  provide that CPU-side sub-sample information.

An independent private offline diagnostic compared the new cycle-cost
estimator to two *different, authentic 86Box/Win95 guest CPU/EMU8000
captures* (the earlier 18:43 V3 capture and the newly provided 18:53
reference). **ONLY FOR OFFLINE DIAGNOSTIC** the first event's ACTUAL
original TSC remainder was used to compute the phase. It is NEVER read
or injected into the VST3 autonomous mode.

| Authentic guest run | Mode | C4 ON | D4 ON | C4 OFF | D4 OFF | Total |
|---|---|---:|---:|---:|---:|---:|
| 2026-11-15 18:43 | Offline, source TSC phase known | 152/152 | 154/156 | 8/8 | 8/8 | **322/324** |
| 2026-11-15 18:53 | Offline, source TSC phase known | 152/152 | 154/156 | 8/8 | 8/8 | **322/324** |

Both remaining D4 ON errors are the two adjacent OUTs at indices
108/109, misassigned by one chip sample. These two independent source
runs share the same structural register program, hence this is
**NOT** an independent general MIDI/patch validation.

The offline source-phase test is only a **CPU-cost estimator diagnostic**,
not a valid claim of self-contained VST3 driver parity. With the
mid-sample fallback phase, the runtime candidate does NOT know the
original fractional Win95 phase; its bit-exact dry PCM output has not
been independently verified. The experimentally observed note-off
crossing and other timing edge cases are now part of the synthetic
automated C++ regressions.

Remaining blocker: derive or replace host-vs-original sub-sample CPU
dispatch phase with a reproducible, driver-independent rule or accept
physical-session variability. A universal bit-identical guarantee across
distinct original Win95 scheduling runs cannot be inferred solely from
MIDI sampleOffset. Next audit should use genuinely new notes, overlaps,
velocities, SF2 programs, and native exported VST3 WAVs.

**Gate remains NO-GO for autonomous Creative Win95 PCM parity.
Original 160,000/160,000 source-clocked reference remains unchanged.**

## P003 CPU-cost refinement on authentic original V3 guest traces (subsequent revision)

The private 2026-11-15 `18:43` and `18:53` original Win95
CPU-/EMU8000 traces were read and compared **event by event**.
For all the note-register writes of each of C4 On/Off and D4 On/Off,
the original **relative CPU-TSC intervals are exactly the same in
both runs**: C4 ON 9500, C4 OFF 331, D4 ON 10058, D4 OFF 325 cycles.
The timestamps' **phases inside the EMU8000 sample are different**,
so neither original 44.1kHz register trace is bit-for-bit identical
across original Win95 sessions. This is a strong *same-program*
observation, **not** a claim about other keys, patches or controllers.

The subsequent private research scheduler revision changes the CPU
work model to use native **EMU8000 selector register classes**, not
A20 sample/preset payloads, or any note-ID or copied clock table.
In particular, transitions from A20 data to E22 0x80-class or 0x60-class
selectors have empirically longer driver work than ordinary index
selects. It still uses an unverified half-sample phase by default.

Original x86 cycle durations vs independent native estimated durations:

| Event | Authentic x86 guest TSC span | New native estimated CPU cycle span |
|---|---:|---:|
| C4 ON | 9500 | 9555 |
| C4 OFF | 331 | 329 |
| D4 ON | 10058 | 9991 |
| D4 OFF | 325 | 329 |

Original-frame classification of 324 source register OUTs, with
**actual original phase used only in offline scoring**, never
as runtime input:

| Original Win95 run | New cycle model with source phase | Same model with unsupported fixed half-sample phase |
|---|---:|---:|
| 2026-11-15 18:43 | **322/324** | 267/324 |
| 2026-11-15 18:53 | **319/324** | 272/324 |

The five remaining original-phase errors over the two sessions are
from original driver CPU work estimation, whereas the much larger
mid-sample errors are from **unknown original sub-sample scheduling
phase**. Because the 18:43 and 18:53 CPU instruction deltas are the
same, the phase difference across runs cannot be repaired by
re-fitting the x86 instruction costs.

The 15-Nov source-clocked native WAV already proved 160,000/160,000
PCM words bit-identical; that does not mean these autonomous model
statistics establish any PCM parity. A genuinely independent native
exported Windows VST3 audio run is required for every new code
revision. **Release gate remains NO-GO for autonomous timing.**


## 17.11.2026 – unabhängiger Originalzustandswechsel in neun Stereo-Scans

Der Zuarbeiter meldet für die beiden echten Original-Windows-95-Sitzungen
einen klaren Gegenbeweis gegen den ehemals genau passenden
**93-Takt-Übergangsansatz**: Die bisherigen neun Allokationen trafen
243/243 Übergänge, im neueren Gastlauf davon **0/243**; bei
**227/243** lagen die beobachteten Abstände exakt **14 CPU-Takte**
über der Prognose, 16 waren komplexer. Beide Aufnahmen durchliefen
4.176 fehlerfreie MIDI-Aufrufe; die neue Aufnahme enthält 1.529.747
konsistente Portereignisse. Die private Dynarec-Blockspur der
neuen Sitzung fehlt noch.

Als **Original-Zustandsbeobachtung** berichtet der Zuarbeiter:
`[ESI+0x18]=4` im älteren und `[ESI+0x18]=2` im neueren
Allokationspfad. Bei >2 geht der Original-x86-Weg direkt zu
`LCOD+0x4048`, bei <=2 durchläuft er zusätzlich
`LCOD+0x4029..0x4042`, also die Prüfungen der aktuellen und
benachbarten Stimme. Das ist eine *belegte plausible
Kontrollflussursache* für den systematischen Mehraufwand,
**kein** schon nachgewiesener exakter 14-Zyklus-Wirkzusammenhang.

Im bestehenden portierten `allocate_pair_mode1` ist die
**native** Modusgrenze `DeviceState.mode_count<=2` bereits
bei der Blockiert-Prüfung vorhanden (einschließlich Nachbarstimme).
Zusätzliche Windows-Regressionen vergleichen daher Zustände
0/1/2 mit 3/4 und prüfen beide möglichen Verläufe, ohne
das Verhalten mit einer konstanten Zeitzugabe zu übermalen.
**Die Registerbasis ESI im Original und ihre Abbildung auf den
nativen DeviceState sind separat zu authentifizieren:** Die
Ähnlichkeit der Offsets allein beweist keine Zeigeridentität.
`[EBP-0x18]` aus P107 ist wiederum ein unabhängiger
Stackwert und darf nicht ersetzt oder gleichgesetzt werden.

Bis P108 die fehlenden 16 Original-Jcc per CPU-Flags belegt,
die neue Dynarec-/P54C-Instruktionsspur für **beide**
Originalzustandsklassen vorliegt und unabhängige Win95-VST3-
Register-/PCM-Gegenproben bestanden sind, bleiben die 14 Takte
und die ehemaligen 91/93/95-Klassen **ausschließlich
Forschungsbefunde**, nicht Produkt-Zeitkonstanten.
