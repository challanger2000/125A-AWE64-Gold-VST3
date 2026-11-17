# P007-S — Actual P165 hardware GUI transplant (not Sampler keyboard)

**Scope:** Draft, 2026-11-17. Original 125A-AWE-Synthesizer P165 appearance transplanted into the separate in-process original Creative/EMU8000 VST3 research branch. No fake or new keyboard. No older 125A DSP copied into the new project.

## Sources precisely pinned

- Original P165 GUI CView classes `FunctionalLayoutBackground`, `GoldButton`, `GoldHardwareMenu` and `HardwareKnob` copied from `challanger2000/125A-AWE-Synthesizer` commit `8e8315c2ecab9f4a8bdbd415c1b7af5a8485a3c1` `src/vst3/PresetBrowserView.cpp`.
- The *original user's photographic P165 frontplate* `125A_P165_Original_Faceplate_1000x708.png` is fetched at CMake configure from that pinned source commit and its Git blob SHA `7e57ef8e0c90d84fa4c951bea7815b0233762ca7` is independently verified. No older P160/P164 artwork substitute.
- Original P165 gold button and master/standard 33-frame knob atlases are included in the Git tree bit-for-bit (source SHA matches destination SHA).
- VSTGUI owns real native controls and status text, with the same 1000×708 geometry and no on-screen piano keyboard. True original Win95 Creative READY is read from the original guest worker's atomic status.

## Genuinely wired in stage 1

- MIDI Bank Select (CC0 and CC32), Program Change and Prev/Next: discrete VST3 automatable parameters are consumed on **the VST3 audio callback**, which remains the **only producer** to the bounded, original in-process COM2 UART. The UI thread only calls the actual VST3 automation API.
- Post-EMU8000 host master volume control: unity produces the original PCM unchanged; an internal mixer/emulator DSP is not substituted.
- Status LCD displays genuine guest boot / original Creative WinMM READY, not an invented hardcoded green light.
- Original hardware envelopes, filter, reverb, chorus, binary banks and SF2 preset titles are **not yet mapped to authentic Creative/WinMM controls**; they are deliberately not presented as working controls. Their dynamic wiring is a later acceptance gate.

## Independent real original Creative MIDI bank/program QA: PASS

A private local Linux QA host used the real Windows 95 Creative AWE64 Gold driver and the original embedded CPU/UART/EMU8000, booted with the product's original keyboard boot assistant. On the active guest COM2 16550 at real READY, the host queued exactly the same packets the new GUI's audio callback emits:

- `P007_P165_ACTUAL_CREATIVE_PROGRAM_CHANGE bankMSB=1 bankLSB=1 program40=1`
- `P007_P165_PROGRAM_40_ORIGINAL_CREATIVE_NOTEON slice=39643 noteon=1`
- `P007_REAL_WIN95_CREATIVE_NOTEOFF slice=42644 success=1`
- `P007_REAL_PRIVATE_RESULT code=0 ready=1 note=1 noteOff=1 pcmNonzero=270098 peak=0.02462769 HddReads=1562 HddWrites=2642`
- `P007_PRODUCTION_GUEST_AUTOBOOT_ORIGINAL_CREATIVE_NOTEON_PCM_PASS` and process exit 0.

This is a true Win95/Creative end-to-end MIDI->PCM test, but it is **not** yet a real GUI mouse/automation/Studio Pro test. Private Windows 95/Creative/ROM binaries are not in public GitHub.

**Remaining:** actual native Windows VSTGUI compilation, real Win32 editor attachment, program/master parameter gestures and project save/restore validation, original Creative filter/ADSR/FX mapping, final studio DAW test. PR stays DRAFT.
