# P007AG — Original Windows-95/Creative AWE64: remaining ten P165 controls

2026-11-18. Research branch `research/p007ag-ten-creative-controls-matrix-20261118`, based on verified P007AE (original Win95 PCM Filter Cutoff and AMP Attack PASS), plus cleared P007AF expected Win95 UD0 exception.

## Original verified base

**P007AE private genuine Creative test previously passed**:
- true 86Box x86 CPU + Win95 Creative WinMM + original EMU8000 PCM, not a software DSP substitute
- initial reference, CC7 mute/restore, Filter Cutoff NRPN 21, AMP Attack NRPN 11, neutral return A/B/A
- Win95 guest already inside ONE self-contained VST3 folder; no original binaries committed
- 16550 FIFO-safe MIDI control pacing >=20ms per six-byte packet

**Remaining ten physical P165 knobs needing independent original PCM**:

| P165 VST3 | Control | Creative NRPN LSB | Test normalized |
|---:|---|---:|---:|
| 1101 | AMP Decay | 13 | 85% |
| 1102 | AMP Sustain | 14 | 95% |
| 1103 | AMP Release | 15 | 100% |
| 1104 | Filter Env Attack | 5 | 90% |
| 1105 | Filter Env Decay | 7 | 90% |
| 1106 | Filter Env Sustain | 8 | 95% |
| 1107 | Filter Env Release | 9 | 90% |
| 1109 | Filter Resonance | 22 | 100% |
| 1110 | Reverb SEND | 26 | 100% (and 0% DRY) |
| 1111 | Chorus SEND | 25 | 100% (and 0% DRY) |

## Code and behavior

- `P007TenControlMatrix.hpp` enumerates exactly those ten IDs, musically relevant comparison windows and **three separate results**:
  - `MEASURABLE`: genuine original Win95 Creative PCM changes at onset, sustain or effect-tail window while source patch restores in A/B/A
  - `INCONCLUSIVE_WITH_THIS_ROM_PATCH`: authentic NoteOn and restoration succeeded but this single GM Program 40 / pitch 60 produced insufficient evidence. **This is NOT a failed original driver or a functioning knob.** It may need another ROM patch or SF2 programming.
  - `A_B_A_REFERENCE_UNSTABLE`: original baseline/return changed too much to accept comparison; cannot claim a measured effect.
- `P007TenControlMatrixTest.cpp` negative-controls silence, dropped B note, unchanged sound, reference drift; also confirms positive changes for envelopes, filter, release, chorus, reverb.
- `P007ActualHostQA.exe <PRIVATE_ORIGINAL_VST3> --private-matrix` is **QA ONLY**, never shipped inside installed VST3. It rejects fake/synthetic-core or missing licensed Win95 disk before any originality claim, boots true private Win95/Creative in throwaway TEMP profile, waits WinMM READY, plays **22 original notes** in a single guest session, performs 10 independent B/A comparisons against original reference, reports all 10 with actual PCM RMS/roughness/tail metrics, and prints a success marker ONLY if all ten yield stable, measurable acoustic effects. Program ID 40, note C3/MIDI 60, original chip 44.1kHz.
- If any control is inconclusive, the executable exits nonzero but still produces all individual measurements, so follow-up is specifically targeted; no generalized or fabricated "10/10" PASS.
- Windows CI compiles the public standalone numerical classifier with adversarial fixtures, the genuine VST3 and Win32 editor, Steinberg validator; it also rejects `--private-matrix` on the disposable *synthetic-only* package. **Public CI is NOT the original Win95-audio proof.**
- P007AE VST3/86Box CPU/EMU8000, private Win95 disk, approved P165 73%-dark GUI and existing user controls are **unchanged**. Only independent QA and test suites change.

## Explicit release blockers

**NOT YET TESTED** on owner's private Windows-95 installation: results for all ten items. Some default GM SoundFont programs may not expose filter envelope or FX dependence; lack of acoustic difference is therefore inconclusive, not automatically broken. Genuine Creative SysEx Reverb/Chorus **type selection**, SF2/SBK uploading into Win95, browsing loaded bank metadata, and final Studio Pro/ASIO acceptance remain open.

Test instructions are included in the eventual private package. No licensed private media may be placed in GitHub or public CI.
