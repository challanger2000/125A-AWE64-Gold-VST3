# P007Z — Original P165 UI restoration, real Windows evidence and release blockers

Date 2026-11-18. Development branch: fix/p007z-p165-original-complete-gui-20261118.
Base P007Y: b21c3b1e061c17f5ca6f1b3092864633e916e695.
Owner-approved original GUI: challanger2000/125A-AWE-Synthesizer branch fix/p165-original-faceplate-layout, source src/vst3/PresetBrowserView.cpp and companion classes. The owner also provided the exact previous VST3 and screenshot.

## Confirmed failure

P007W/Y newly authored a minimalist P007P165View instead of porting the available full P165 view. It deleted all 12 original knobs, ROM/SF2, both FX mode menus and MOD RESET. 47/47 Steinberg validation and audio state/synthetic host tests passed because none checked native graphical completeness. P007Y should never have been offered as a complete GUI.

## Correction and actual execution

P007Z's compiled VST3 uses the OWNER-APPROVED original P165 PresetBrowserView source, photographic 1000×708 faceplate, original knobs, buttons and original gold menus. Minimal P007P165View is NOT compiled. The P007 original 86Box/EMU8000 DLL, private Win95 guest path and original Creative MIDI-to-PCM are unchanged. Native old Master ParamID 1003 is correctly adapted to current P007 1001; bank normalized range adapted from old 16383 to original Win95 127.

GitHub Actions run 44294138538 at cafd79909467f10032e1fdc6b35744ff9e9f1928: SUCCESS. Real Windows native VST3 47/47 Steinberg Validator, 900 callbacks/90 MIDI public host checks and synthetically supplied PCM at 44.1/48/96 kHz remain PASS. Real Steinberg EditorHost screenshot/P165 UI positive test PASS: exactly twelve full original 58px gold controls individually checked for visibility, genuine original 134px master control, both effect type menus, MOD RESET and both ROM/SF2 buttons, Next/Prev mouse changes actual program, Browser row selection updates real MIDI program, Synth returns, Master drag changes real sprite.
Actual source render evidence: P007Z-Actual-Original-P165-Complete-GUI-Screenshots artifact in this run. This checks VISUAL COMPLETENESS, not automatically the full Win95 sound control.

## CONTROL TO REAL ENGINE MATRIX — NO RELEASE UNTIL RESOLVED

| Control | Visual P165 | Actual original Win95/Creative engine |
|---|---|---|
| MIDI Program + Prev/Next/Browser | PASS | PASS, actual MIDI Program Change |
| Bank selection | PASS | PARTIAL: CC0/CC32 forwarded; metadata and nonzero bank choices not fully verified |
| Master Volume | PASS | PASS in prior P007Y test, post-EMU8000 original PCM |
| AMP ADSR (4) | PASS | NOT CONNECTED; intentionally disabled |
| Filter ADSR (4) | PASS | NOT CONNECTED; intentionally disabled |
| Filter Cutoff + Resonance (2) | PASS | NOT CONNECTED; intentionally disabled |
| Reverb/Chorus Send (2) | PASS | NOT CONNECTED; intentionally disabled |
| Reverb/Chorus Type menus | PASS | NOT CONNECTED; intentionally disabled |
| MOD RESET | PASS | NOT CONNECTED; intentionally disabled |
| ROM button | PASS | Original ROM already bundled and auto-loaded; hot replacement unsupported, explicit notice |
| SF2 button | PASS | Original Creative 16-bit AWE Manager bank loading not implemented, explicit notice |
| Browser patch names | PASS | Only honest generic MIDI program numbers until original Creative catalog metadata available |
| Original guest in Studio Pro | NOT RUN P007Z | Original Win95→EMU8000 audio previously passed P007Y independent host |

Historical Creative primary sources document an actual NRPN path for hardware generator editing: CC99=127, CC98 selects LSB, CC6/CC38 supply 14-bit value with 8192 center. AMP Env2 A/D/S/R generator NRPN LSB: 11/13/14/15. Filter Env1: 5/7/8/9. Cutoff/Resonance:21/22. Chorus/Reverb Sends:25/26. The original Windows driver also supports Reverb and Chorus variant SysEx, but current P007 guest relay does not handle SysEx. These are DOCUMENTED ROUTES ONLY, not proven P007 audio functionality.

NO-GO: do NOT label this candidate finished or supply a misleading full-functional GUI to the owner until actual controls each change original Creative PCM correctly, are correctly automated/saved/recalled in project, and native Studio Pro acceptance passes. Structural GUI restoration alone cannot clear that release gate.