# P002 — Creative original API, register oracle, acceptance

**IN DEVELOPMENT — not an original driver sound fidelity PASS.**

## First confirmed Windows implementation / shared Test Lab evidence

[Actions #43094301052](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43094301052), source head ad3a5340be282935d6162ac3d9e9b5dae1de92ae, **SUCCESS** in both jobs:

- **3 / 3 CTests PASS** on Windows x64 Release: verified unchanged original upstream EMU8000 chip / RAM and direct chip differential with real negative controls plus the newly compiled 16-channel Creative WinMM front-end **160 MIDI event cases**.
- **12 / 12 Python register-oracle positive/negative tests PASS**: first exact discrepancy for chip-clock frame, port, bit, read/write, event order, missing/extra event, invalid trace clock/syntax. These test the comparator, NOT an original-controller implementation.
- **Existing shared 125A Plugin Test Lab** at pinned main commit **7213fe1f81f4036f74431cb5b2d7dbd09b73ee5c** was really cloned and source-built in an independent Windows job. Its real native P74/P73 host loaded the actual AWE64_Gold_P001.vst3 exported bundle and produced status **INVENTORY_COMPLETE**, discovery PASS, **0 automatic GUI clicks**, commercial DAW **NOT RUN**, release_approved **false**. The report was uploaded as the job's short-retention read-only artifact. No fake GUI, controller, soundbank or original-voice PASS was claimed.
- Real Windows P001 VST3 module is built from pinned upstream fork, Steinberg SDK 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96, and original P001 source remains untouched.

**Not completed:** The new P002 translator has not yet generated original-Creative-equivalent native register traffic, nor has the actual original-Win95 controller/velocity/voice matrix been acoustically validated. P002 is therefore still **IN PROGRESS**, with P003/full bank loading and final GUI/Studio-One/ASIO later.

 P001 was technically approved in PR #2. Source: the controlling review comment at https://github.com/challanger2000/125A-AWE64-Gold-VST3/pull/2#issuecomment-6805882547

## Primary-source pin and hierarchy

Creative Sound Blaster AWE32 Developer Information Package v3.01, 2 August 1996, publicly archived at https://github.com/qb40/sound-blaster-awe32, source commit **69e09cbf59fd4ff7dce3482406029c88a05ab878**. Source files directly inspected:
- WINDOWS/INCLUDE/AWE_API.H: numerical definitions for AWE_SEND_MIDI=401, AWE_LOAD_USER_BANK=204, AWE_VIENNA_LOAD_PRESET=2006, AWE_VIENNA_NOTE_ON=2009, AWE_VIENNA_NOTE_OFF=2010, AWE_VIENNA_CONTROLLER=2011, AWE_VIENNA_PITCHBEND=2012.
- WINDOWS/INCLUDE/AWE_DLL.H: historical 16/32-bit AWEMAN32 interface and DWORD-like user handle.
- WINDOWS/SAMPLE/AWEDEMO/WINAWE32.C: original Creative sample invokes the AWEManager entry point, OPEN, SEND_MIDI, SELECT_SYN_BANK and LOAD_USER_BANK.
- WINDOWS/SAMPLE/VISAMP/VISAMP.C: original Vienna sample load, preset load, note-on/off examples.
- DOS/EMUINC/EMU8000.H: original EMU8000 hardware initialization arrays.
- DOS/LIB/SBKLIB/INCLUDE/CTAWEAPI.H, DOS/LIB/SF2LIB/INCLUDE/CTAWEAPI.H: historical bank structures for P003 interpretation.

Additional original PDF primary sources located in the archive: DOC/EMU8KPGM.PDF (EMU8000 Programmer's Guide) and DOC/ADIP301.PDF (AWE Developer API). The files are identified here, but **their full page-by-page contents have not yet been independently parsed**, so this commit claims no specific PDF rule. The historic Windows Creative .DRV/.VXD/.DLL files and DOS libraries are **NOT** runnable 64-bit driver source; do not copy or link them into the VST3 without resolving 16/32-bit dependencies, legality and realtime issues.

**Independent executable oracle always takes priority:** actual Creative Win95 driver running inside pinned calibrated turican0/86Box-AWE32 with matching original 1-MiB ROM, 28-MiB EMU8000 RAM, original MIDI bytes and chip frame clock, EMU8K_TRACE and AWE32_WAV. Do not modify pinned original EMU8000 C file or introduce preset-specific filter/attenuation hacks.

## First P002 implementation

- src/creative_compat/CreativeApiIds.h: factual Creative 1996 message numbers; no copied driver body or binary.
- src/creative_compat/CreativeMidi.{h,cpp}: 16-channel WinMM short MIDI decoder, byte-accurate raw messages, velocity-zero NoteOff, logical per-channel bank MSB/LSB, program, CC7/CC11/CC10/CC64, pitchbend. **Not yet a Creative voice register implementation**.
- tools/p002_register_oracle.py: compares each original driver vs native generated trace event by absolute 44.1-kHz frame, read/write, port, value and same-frame order; first discrepancy blocks. Zero tolerance, no sorting, gain correction or arbitrary alignment.
- tests/p002_midi_contract.cpp plus tests/test_p002_register_oracle.py: nonvacuous positive and adversarial malformed/missing/read-direction/bit-flip/clock/reordering contracts.
- Windows Release CTest and Python regression; preserve all P001 baseline tests and original chip SHA.

## P002 obligatory experiments with the real Win95/Creative original

Capture each scenario with same ROM, 28 MiB RAM, original bank/initial state, MIDI bytes and sample-clock origin, and derive a **separately generated** native MIDI-to-register stream. Compare exact register value, read/write, order and frame. Render and compare digital PCM as a second independent gate.

1. Original ROM program 0 + NoteOn/Off velocity 1/64/127; velocity-zero NoteOn; attack/release timing.
2. Multiple overlapping voices, retrigger, note stealing and voice allocation; original channel 10 drums.
3. Program changes, CC0/CC32 bank selection, CC7 volume, CC11 expression, CC10 pan, CC64 sustain, CC120/121/123 resets/all-off.
4. Pitchbend low/mid/high, channel/aftertouch and supported controller nuances.
5. Filter cutoff/resonance, filter/amplitude envelopes, initial attenuation and FX sends (including CC91/93) as established by actual driver trace.
6. Real Creative SYNTHGM.SBK/ROM bank separation (P002 semantics); P003 handles actual bank sample upload and full SF2 generator/modulator translation.

**Strict provenance rule:** a byte-for-byte trace match proves *only* the compared file data are identical; copied original trace is NOT independent native driver translation. A real native translator and evidence of its MIDI input and initial conditions must be provided before any P002 driver equivalence PASS.

During development use the existing 125A Plugin Test Lab P73/P74/P75 for real headless VST3 inventory, and later reviewed P65/P67 native state/audio/feature contracts. Do not invent or auto-pass unavailable feature tests. **Final Studio One and physical ASIO only at end after completed GUI**; no new early hardware requirement.
