# P022 – P021 private false-silence abort: test-duration mismatch

**20 November 2026.** User reported P021 private test **FAILED** and supplied complete three logs: `P021-ERGEBNIS.txt`, `P021-VST3-HOST.txt`, `P021-ORIGINAL-CHIP-PCM.txt`.

## Actual P021 result (do not call P021 owner test passed)

- **Real private Windows 64-frame VST3 host path PASSED**: original ROM, SYNTHGM.SBK, all real SF2 preset IDs (77/77), active v1/v4 project recall, MIDI/16 channels, A/B/A, 44.1k audio and the revised 48/96k signal path. The 48/96k old approximate-core negative hashes are correctly rejected.
- **Standalone original-chip multirate source test FAILED.** Its log begins `P021_ORIGINAL_EMU8000_NON_NATIVE_RATE_FAIL P021 original chip PCM is silent`; its DRY 8000 and 48000 Hz comparisons had exact PCM (0 differing samples out of 16,384); then the next 96000 Hz case aborted *before PCM error comparison* because `signal > 100000000` was false for the reference signal.
- Combined BAT file: `P021_OWNER_CHIP_RATE_PROBE_FAILED rc=1`, `P021_TEST_EXITCODE=1`.

**Root cause in TEST IMPLEMENTATION:** `P021OriginalChipResamplingProbe.cpp` captured 8192 *frames* for every output rate. That is a radically different test duration: 1024 ms at 8 kHz, 170.7 ms at 48 kHz, 85.33 ms at 96 kHz and 42.67 ms at 192 kHz. The fixed absolute reference sum-of-squares `>1e8` was therefore sensitive to both duration and SF2 note attack. It failed to establish a meaningful native-PCM comparison at 96 kHz. The log proves the signal-energy threshold was not reached; it does **not** prove actual digital zero samples or an EMU8000 implementation defect.

## Corrected P022 gate

The test probe now:
1. Acquires precisely the **same 750 ms physical duration** at every output rate (6000 frames at 8k, 36000 at 48k, 72000 at 96k, 144000 at 192k), plus reference 44.1k PCM extending 750 ms with interpolation guard samples.
2. Computes the **rate-normalized reference RMS** instead of the frame-count-sensitive fixed absolute energy. Emits it for each DRY/FX case. Requires a meaningful actual reference PCM RMS above 5 signed-int16 LSB for the 750 ms interval.
3. Compares actual host-rate 86Box chip PCM against independently resampled 44.1k original 86Box PCM with at most 1 signed-int16 LSB error; all 8 DRY/FX rate combinations must pass.
4. Prints unambiguous P022 equal-duration PASS marker only after all eight comparisons finish.

This is a change to **test methodology only**. The exact P021 VST3 with real 64-frame chip and corrected resampling stays the same; no audible output, DSP, bank, driver or latency code is touched.

## Evidence/limitations

The public Windows CI must pass after the updated probe is compiled. The corrected **private owner** PCM probe still needs to be executed under Windows; until then the P021 full original-chip multi-rate gate is **not yet accepted**. Do not repeat all 77 VST3 instrument tests unnecessarily: those already passed in the user's P021 host log and are unchanged.

No original private Creative ROM or SoundFont file is committed to GitHub. P007 is unchanged. Studio Pro end tests remain deferred.

## P022 public Windows validation: PASS (real original-chip code, public samples)

CI [run 44800839344](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44800839344): successful Windows build, 47/47 Steinberg tests. The revised 750ms original-chip multirate test completed all 8/8 DRY/FX comparisons:
- 8 kHz: 6,000 stereo frames; zero differing values / 12,000 stereo values.
- 48 kHz: 36,000 stereo frames; zero differing values / 72,000 stereo values.
- 96 kHz: 72,000 stereo frames; zero differing values / 144,000 stereo values.
- 192 kHz: 144,000 stereo frames; zero differing values / 288,000 stereo values.
- The exact-pinned 44.1k reference PCM RMS was ~113 signed-int16 LSB DRY and ~134 signed-int16 LSB FX during 750ms (not silent).

Further [run 44801114810](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44801114810) also **SUCCESS**, with new CI enforcement of exactly eight `P022_OWNER_ORIGINAL_CHIP_RATE_WINDOW_*` cases each containing `duration_ms=750`, plus the exact `P022_EQUAL_750MS_ALL_RATES_GENUINE_CHIP_PCM_PASS_8_OF_8` completion marker.

The owner has not yet executed P022. A private one-click ZIP was created with only the rebuilt standalone Windows probe and SHA256-verified owner ROM, Creative SBK and original 77-preset SF2. No VST3 binary changed in P022; owner is **not** asked to retest all 77 presets or project recall.

**Status as of final private P022 acceptance: P021 initial combined test FAILED because of a faulty probe; P022 corrected public CI PASS and corrected owner-ROM/SBK/SF2 test PASS. The successful VST3 host evidence from P021 is retained.**

## P022 owner's actual private Windows original-asset test: PASS (20 Nov 2026)

The owner executed the revised, small private P022 test package on Windows, returned its complete `P022-ERGEBNIS.txt`, and the results explicitly report:

- `P022_PRIVATE_ORIGINAL_CHIP_EQUAL_DURATION_8_OF_8_PASS`
- `P022_P021_VST3_HOST_REMAINS_PREVIOUSLY_PASSED_NO_RETEST`
- `P022_TEST_EXITCODE=0`
- For DRY and FX, all 8k/48k/96k/192k 750ms test windows have **zero differing stereo int16 PCM values** versus the independently resampled original 44.1k 86Box EMU8000 signal, `max_error_lsb=0`, `rms_error_lsb=0`.

| Rate | Stereo PCM values in each DRY/FX comparison | DRY original RMS (int16 LSB) | FX original RMS (int16 LSB) | Different samples |
| --- | ---: | ---: | ---: | ---: |
| 8 kHz | 12,000 | 3804.35796051 | 4129.89398877 | 0 |
| 48 kHz | 72,000 | 3804.34801148 | 4129.88917793 | 0 |
| 96 kHz | 144,000 | 3804.34399160 | 4129.88500069 | 0 |
| 192 kHz | 288,000 | 3804.34174897 | 4129.88355511 | 0 |

**P022 corrected private resampling parity is accepted for the tested original user-bank program and 750ms DRY/FX windows.** The previously failed P021 combined BAT shall **not** be relabeled a PASS; rather, the initially failed private sub-test was corrected and verified separately, whereas its original private Windows VST3 host test already passed.

Still open for a release: actual Win95 Creative/vintage physical AWE64 hardware-vs-native register/PCM parity; exact MIDI event timing and interpolation/lookahead at 8/48/96/192k; stress and polyphony combinations beyond tested set; bank/SF2 file identity persisted through DAW project reopen; plugin packaging/licensing and final real Studio Pro QA. Preserve owner assets privately; no change to P007.

