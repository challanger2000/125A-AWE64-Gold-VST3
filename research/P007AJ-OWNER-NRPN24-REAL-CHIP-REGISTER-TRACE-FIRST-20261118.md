# P007AJ — vier Filter-ENV-Regler: echte EMU8000-Registerzugriffe statt weitere blinde Klangtests

**Datum:** 2026-11-18. **Status:** Root-cause research; no new user test released, no VST3 or original chip source edited.

## Original private owner evidence, P007AI

Original user file: `P007AI-FILTER-ENVELOPE-TIEFE-ERGEBNIS.txt`. The owner ran a real original in-process 86Box/Win95/Creative guest with audio output through the original EMU8000; 14 notes, WinMM READY, P007W native host PASS, no synthetic guest.

- Filters ENVELOPE Attack/Decay/Sustain/Release (VST3 1104–1107): **0/4 independently acoustically established**, all `INCONCLUSIVE_WITH_THIS_ROM_PATCH`, no unstable reference according to the broad 28% reference checker, actual original source restored after final CC121.
- P007AG + P007AH + P007AE: previously **8/12 actual original chip sound-verified** P165 controls. Do NOT downgrade these eight based on this inconclusive follow-up; do NOT promote four unresolved.
- Real NRPN24 signed depth test results: original RMS `0.001931393`; negative -96 `0.001806812` (**-6.45%**); positive +96 `0.001943613` (**+0.63%**). P007AI autonomously chose **-96**, but the rerecorded "activated depth" baseline `0.001975162` was **+9.3%** relative to the earlier -96 sample, and the first "neutral return + depth reapply" RMS `0.001749848` was **-11.4%** relative to that activated baseline. Thus **no reliable evidence the same signed NRPN24 value actually reached and remained in original Creative's voice setup**. Our existing `P007TenControlMatrix.hpp` considers a return "stable" even with 28% RMS / 32% roughness deviation; sufficient to reject broken large effects, **not** appropriate to prove subtle 2–10% filter-ENV differences.
- The trace does **not** indicate missing notes, Win95 failure, original chip failure, or a reason to change the real EMU8000 synthesis core.

## Verified original upstream 86Box firmware / software evidence

The compiled **OWNER-PRIVATE `125A_AWE_Core.dll` inside `125A-AWE64-Gold-P007AI-FILTER-ENVELOPE-TIEFE-PRIVATE-20261118.zip`** has the exact literal `EMU8K_TRACE` and `AWE32_WAV` strings. That binary inspection confirmed trace implementation is present in the built original driver core, not merely source code.

Verified pinned original 86Box-AWE32 source `turican0/86Box-AWE32` SHA `fee1e81977824fd2d5eed4d239589dc30d251dd7`:
- `src/sound/snd_emu8k.c:1341–1358`: `emu8k_inw` calls `emu8k_trace_read(addr,val)`; **every original chip I/O write** passes through `emu8k_outw` and calls `emu8k_trace_write(addr,val)`.
- `src/sound/snd_emu8k.c:2806–2807`: `emu8k_trace_advance(num_samples)` increments the true native chip 44.1 kHz timebase.
- `src/sound/snd_emu8k_trace.c`: built-in `EMU8K_TRACE` environment variable enables **original guest-driven register read/write log**: `<chipFrame> <ioPortHex> <registerValueHex>`; `R <frame> <port> <value>` denotes read. Source original developer's comment explicitly says this preserves AWE32 driver register comparison. No synthesizer rewrite required.
- Original Creative Developer Information Pack Part IV **p83** explains exact original NRPN CC99/98/6/38 signed 14-bit encoding. **p93** confirms NRPN LSB24 Envelope1→Filter Cutoff depth is **not realtime** (`Realtime: No`), legal `[-127,127]`, 56.25 cents per step. Original source: https://www.phatcode.net/res/244/files/adip301.pdf . Send before NoteOn, not just to an already sustained note.
- Exact original Win95 relay `experiments/p007-inprocess-86box/guest/P007_Win95_NoCRT.c` forwards each valid 6-byte UART packet through `midiOutShortMsg(synth,msg)`, but existing P007AI host QA **only checks transmit accepted by host UART**, not whether original Win95 MIDI driver applied all four NRPN24 bytes and changed EMU8000 voice registers.

## Required next sound-proven approach

**Avoid another arbitrary parameter-value experiment**. Instead add a **bounded, opt-in test-only diagnostic of the actual original EMU8000 register-writes** without mutating chip register logic, ROM/Win95 HDD, the original VST3 GUI, or playback calculations.

1. Real private guest loads one Pad8 original ROM preset; compare **otherwise identical** notes with same channel, velocity, pitch and note length.
2. Confirm exact NRPN24 selector/data and at least one B/A controlled filter-envelope NRPN have been handed to original Win95 `midiOutShortMsg`, or instrument only the original EMU8K port write trace to observe resulting chip configuration. **Do not equate host `p007_midi_submit=1` with guest receipt**.
3. Decode original `emu8k_outw` voice-register configuration for the four envelope-related registers and filter modulation routing. Require original-patch A reference + B target + A restored ***same depth*** to match appropriately; note measured note-to-note RMS drift.
4. If register writes differ as specified yet PCM is subtly affected, change only test sensitivity/preset/analysis. If **no** register difference exists, isolate MIDI driver NRPN handling, register mapping or missing modulation generator first. If CC121 unexpectedly resets a custom depth, reapply and **verify on the chip** before declaring the A/B/A comparison valid.
5. Developer caution: `EMU8K_TRACE` in this exact original source logs EVERY original read and write and flushes each event to a file; **unbounded tracing during the full 1–3 minute Win95 boot can generate enormous logs and alter emulation timing. DO NOT instruct owner to set EMU8K_TRACE blindly**. First implement a bounded/offline-summarized test-only observation window or an in-memory port event counter associated with controlled NoteOn events. No private original VM media or traces uploaded publicly.

This is **technical diagnosis**, not an implementation patch nor a successful original four-knob sound certification. User gets a new test only after the trace is genuinely bounded, safe, validated by Windows CI and assembled with immutable original licensed assets.
