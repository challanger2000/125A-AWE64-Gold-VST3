# P007-U — Actual approved P165 editor / program-browser acceptance

**Date:** 2026-11-17. Research branch (draft PR #15) only; product Windows VST3/Studio Pro still needs final ASIO and GUI acceptance.

## Source-accurate original P165 hardware art

- Original photographic 1000×708 P165 faceplate pinned and SHA checked: Git blob `7e57ef8e0c90d84fa4c951bea7815b0233762ca7`.
- Original P165 native 33-frame hardware master knob atlas, gold button atlas and native VSTGUI menu drawing.
- Original P165 SYNTH/BROWSER selection, original gold and dark translucent browser controls reinstated **with no on-screen piano keyboard**.
- Actual original-EMU8000/Win95 guest READY message shown in amber LCD; no fabricated "ready" indicator.
- Functional true host VST3 automation for MIDI program/bank/Prev/Next and post-EMU8000 master gain (zero mutes output; unity leaves original 44.1k PCM unchanged).
- BROWSER lists/searches the 128 *numerical* Creative MIDI Program Change IDs, selects rows with actual host automation. Do **not** confuse these with externally verified SF2 names: original Win95 patch/bank metadata is **not** yet transmitted to the VST3.
- No fake ADSR/filter/FX plugin DSP or decorative dead knobs. Dry/FX actual guest route work is separate.

## First genuine EditorHost graphical and mouse acceptance

[Windows CI 44147654812](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44147654812) loaded the true P007S original-core Windows VST3 into real Steinberg EditorHost on a Windows-2022 desktop and passed **14/14** original photo/logo/knob/buttons/canvas and Next/Prev/master mouse interactions.

[Native compiled P007-U Browser CI 44149743930](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44149743930) independently rebuilt the real P165 Windows VST3 module with the original in-process 86Box DLL and VSTGUI code. Steinberg's post-build validation succeeded and a real EditorHost process opened the 1000×708 GUI. It passed **19/19** Win32 pixel/gesture assertions including:
- P165 original photographic gold logo drawn at original geometry;
- native gold hardware bank/program selectors and original large 134px knob present;
- Next and Previous clicks change the true amber LCD program display;
- knob mouse drag changes its actual PNG frame and the displayed percentage;
- BROWSER displays the original P165 black/gold searchable list and row 001 click changes the actual VST3 Program parameter and dynamic LCD text;
- SYNTH returns to the working master-knob page;
- EditorHost did not terminate during GUI tests.

All screenshots are preserved in the GitHub Actions artifact `P007U-P165-Win32-GUI-Browser-Screenshots`. No private Win95 disk, licensed Creative ROM, or Creative driver was used or uploaded for the EditorHost pixel test.

The final caption contrast/legibility refinement is being rebuilt and tested separately. Do not describe that specific post-refinement build as accepted until its action succeeds.

## Actual private original Creative MIDI path

Private original Windows95 Creative WinMM guest Program Change BankSelect+Program40→NoteOn/off→original EMU8000 PCM **PASS** (270,098 nonzero PCM samples, peak 0.02462769) as separately established on the source-identical original Linux QA core. This is not a direct proof of screenshot/EditorHost host MIDI audio but verifies that the exact MIDI protocol used by the controls reaches original Creative.

## Not yet implemented or released

Actual SoundFont preset names, loaded-bank list, original Creative ADC filter/envelope/FX UI command mapping, VST3 project state-save/restore, multi-instance, Windows Studio Pro/ASIO/audio-latency and offline-render QA remain open.
