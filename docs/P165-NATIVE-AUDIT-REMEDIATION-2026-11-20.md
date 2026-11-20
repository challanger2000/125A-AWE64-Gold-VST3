# 125A AWE64 Gold – P165 native audit remediation register

**Date:** 20 November 2026  
**Product:** One Windows x64 native original-chip EMU8000 VST3, **no external 86Box/Win95 process**  
**Working branch:** `feature/native-p165-complete-integration-20261120`, verified source `1f9051fae83f3347d96663df0b77ace58e9e13ca`  
**Baseline reference:** Original 86Box driver/chip `turican0/AWE32Emu@17b0578fe59909dfaa784b5531d6d1a3a80729ce`  
**GUI source:** Last original Win95 P007BW P165 faceplate, LCD x=370 inset and dark 73% hardware-knob islands preserved.  
**Original audit:** [adversarial read-only defect report](https://github.com/challanger2000/125A-AWE64-Gold-VST3/blob/audit/p165-adversarial-readonly-20261120/docs/AUDIT-2026-11-20-P165-NATIVE-ADVERSARIAL.md).

**Gate:** STILL **NOT FULL AUDIT, NOT RELEASE READY**. This is a remediation/evidence record, not an assertion that unexecuted tests passed.

## Reproduced issues and corrective status

| ID | Original defect | Corrective source | Measured evidence | Status |
|---|---|---|---|---|
| F-01 | V5 project restore dropped saved 16ch RPN/NRPN, while V4 passed | `P009Native.cpp` reads V4 advanced-state prefix for **V4 and V5** | Same previously failing adversarial Windows Steinberg-host V4→V5+GUI→reopen oracle now `AUDIT_V5_GUI_AND_RPN_INDEPENDENT_PROJECT_RECALL_PASS` | **PASS tested** |
| F-02 | Last P165 Reverb/Chorus type menus were rendered but had `setMouseEnabled(false)` | Actual menus reenabled; VST3 1400/1401 messages reach original 86Box INIT/HWCF registers | Source-negative guard + actual Win64 EditorHost interaction; **real native VST3 wet SF2 PCM differs when effects types change** | **PASS tested** (synthetic fixture) |
| F-03 | `getState` read processor-owned non-atomic MIDI/patch arrays while `process` wrote them; `setState` modified chip while audio active | `NativeRuntimeRestore` host→audio mailbox, audio-thread-only chip restore at block boundary, lock-free retired commands, per-field atomic coherent mirror read with epoch consistency guard; exact V1-V5 data format retained | Actual native Windows VST3 **5,000 audio blocks concurrently with 400 full 16-channel V4 RPN project restores/saves**; no torn snapshots; separate RT heap QA and 47/47 validator PASS | **PASS defined stress**; sanitizer/host lifecycle matrices NOT RUN |
| F-04 | “USER_SF2_RESTORED” only copied a fixed file, not tested external file project recall | Actual `P010NativeHostQA --synthetic` creates two **different** public wet original-chip SoundFonts and generates a V5 external absolute path + SHA256; checks sound before/after reopening and with wrong bundle-local default; corrupt digest fails without changing good project | Win64 real Steinberg-host source bank A/B genuine 86Box PCM and file SHA positive/negative: `P165_PUBLIC_REAL_VST3_SYNTHETIC_86BOX_SF2_V5_SHA_POSITIVE_NEGATIVE_PCM_PASS` | **PASS synthetic actual VST3**; private original 77 SF2 positive V5 path/PCM still NOT RUN |
| F-05 | Same-bank browser reselection after MIDI CC0 had a boolean-only fake success callback | P165 GUI enqueues an atomic real host bank reassert, consumed on the original-chip audio thread with Creative bank and program | Code path reviewed; complete end-to-end MIDI CC0+GUI click+real PCM NOT RUN | **PARTIAL** |
| F-06 | 0% SEND was an additive neutral overlay, not a physical zero send | Original `Synth.cpp` driver-register output now has two **explicit** manual/original modes, stored as V5 `effectModes[0/1]` bit `0x0100`, no V5 byte-size change. Untouched preset's default SEND still produces bit-exact original chip registers; first mouse touch enables absolute send register (0–255). Exactly 0% manual = **zero** even after channel CC91/93; GUI RESET returns to unaltered patch. A same-sample host mode event dominates SEND value independent of event enumeration. | Real Win64 VST3 with generated **wet** public SF2: `P165_ACTUAL_ORIGINAL_EMU8000_VST3_DEFAULT_VS_ZERO_VS_100_SEND_PCM_PASS`. All default P022 chip PCM and 47/47 validator still passed on first manual-mode commit. Actual same-sample mode-before-SEND-value RESET→ORIGINAL PCM equality was also verified in the real VST3 Windows test. | **PASS zero/100/original PCM and RESET-to-ORIGINAL equality** |
| F-07 | Unconsumed staged/retired Synth owns full native 28-MiB chip without teardown | `terminate` and destructor drain staged native Synth, retired chip and pending GUI/state; non-RT editor idle and host save reclaim completed chip swaps | Normal Win64 host lifecycle/validator PASS; sanitizer with intentionally unusual close-in-the-middle schedule NOT RUN | **PARTIAL** |
| F-08 | Nonstandard/inconsistent P009 moduleinfo metadata + public distribution licensing | Strict valid `moduleinfo.json`, consistent product/class version `0.9.0`, unchanged stable FUID, verified by Windows CI/Steinberg validator | `P165_VST3_STRICT_VALID_JSON_FACTORY_FUID_AND_VERSION_PASS` | **PASS technical metadata; public licensing OPEN / BLOCKER** |
| F-09 | Event/CC queue overflows returned error and silence, but might leave held original-chip voices sounding | Original Creative CC120 All Sound Off sent to all 16 channels before fail-closed overflow return; zero heap | Code path reviewed; dedicated >512-event/NoteOff recovery with real held voices NOT RUN | **PARTIAL** |

## Exact test provenance

- **[LATEST COMPLETE Windows original-chip/VST3 QA: 47/47, 8/8, 5000/400 concurrency, 2 soundbank SHA/PCM, wet FX sends/types and real P165 EditorHost – run 44825506636](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44825506636)**, exact source `1f9051fae83f3347d96663df0b77ace58e9e13ca`. Verified Windows CI SUCCESS. SOURCE DEFAULT send mode remains truly untouched original SoundFont PCM; obsolete additive stage was eliminated from original chip voice parameter adaptation.
- [Public Windows original-chip/SF2 V5 SHA+PCM, 47/47, EditorHost PASS – run 44823300479](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44823300479), source `315f73f17decda0625352d837df4e8bd0ef239de`.
- [Real same-instance concurrent getState/setState/audio **5000/400 PASS**, real external SF2/PCM and EditorHost – run 44823963813](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44823963813), source `e3bb1a173038a7befa5f0683803ccaeb1bc49dae`.
- [Real original EMU8000 wet-chip reverb/chorus type PCM and DEFAULT/0%/100% hardware SEND **PASS** – run 44824605373](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44824605373), source `74a2d8510a27ff00221b74b71a853567c59c397f`.
- [Original P165 faceplate, 47/47, 8/8 native-rate PCM and V5 GUI/RPN host – run 44821310569](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44821310569).
- The earlier private `AWE64-GOLD-ABSCHLUSS.txt` reports real ROM/SBK, 77/77 original owner's SF2 PCM and 8/8 reference-rate PCM **on an older commit**. **Do not transfer that private PASS to later V5/FX/RT changes without re-running.**
- The static manual-zero register branch writes 0 into the **original 86Box EMU8000** reverb/chorus send bits after the authentic Creative 0.9× CC91/93 summation. No alternate DAW DSP/reverb or limiter exists.
- The PCM parity comparator uses the *same pinned original 86Box chip* with different windows/rates, and does **not** prove physical AWE card parity. User has no such card; physical hardware test is not a gate.

## Owner GUI/DAW semantics

- Hardware SEND knob at untouched 0 displays **DEFAULT**, indicating genuine per-patch soundfont/Creative send levels, not a zero register.
- First mouse click on a SEND control activates the explicit manual mode; the numeric 0% then means real original-chip zero SEND. 100% writes the full 255 send byte for subsequent new notes. GUI RESET restores **DEFAULT** and untouched original chip coefficients.
- VST3 automation IDs `1100..1111` remain unchanged. New `1420/1421` expose original/manual send modes for project recall. V5's two 16-bit FX type slots carry bit `0x0100` for manual mode; the lower 3 bits remain FX types `0..7`. Legacy V1-V4 and earlier V5 states with no mode bits remain readable.
- Host/GUI never changes live native Synth pointers or controller registers from within VST3 `setState` during audio playback. Request is processed at the next actual callback before MIDI points or NoteOn.
- The original `P007` Win95 reference branch, owner ROM and Creative SBK, original private SF2 bytes and DSP-core baseline SHA have **not been overwritten or publicly committed**.

## Mandatory gates remaining

1. **COMPLETED:** Up-to-date full Windows CI on exact source `1f9051fa` passed, including real-chiptone effect DEFAULT/0%/100%, FX presets, mode-before-value RESET to original and GUI interaction. No final user DAW acceptance has been inferred from this success.
2. Run the new private original-owner **positive** V5 external SF2 SHA/path + audible PCM A/B/reopen test on Windows using actual owner files; no ROM/SBK/SF2 to public GitHub.
3. Dedicated bounded MIDI event overflow >512, stuck-note recovery, same-bank GUI + CC0 + actual native audio, and repeated interrupted asset-swap/destruction tests; independently challenge these assumptions.
4. Full 125A Plugin Tester and FINAL Fender Studio Pro 8 real graphical usage/automation/open-close/project save+reopen test only when the consolidated instrument is ready.
5. Determine clear permissions and GPL/Steinberg SDK legal obligations for any **public** distribution. The owner private archive is not release-license evidence. AWE32Emu does not publish a clear permissive blanket license for all driver layers.
6. A second independent adversarial audit (e.g. Codex) using `125A-Engineering/QA/AUDIT-COMPLETENESS-CONTRACT.md` with no leniency for unverified areas.

**Current disposition: substantial verified corrections, but NOT a completed FULL AUDIT and NOT a public release authorization.**

## F-05/F-07/F-09 targeted follow-up (candidate; CI verification required)

- **F-05 code repair:** the ROM button now routes through the same bank/program reassert function as other browser selections. This closes the previously unhandled case of MIDI CC0 changing the real chip while GUI and host still display ROM. Actual GUI click + MIDI CC0 + PCM remains **NOT RUN**.
- **F-07 new Windows VST3 adversarial lifecycle gate:** repeatedly stage two different SHA-verified public SF2 instruments, save/compare V5 identity, process some but not all swaps, close/reopen/deactivate while 28-MiB candidates remain staged or retired. This is **NOT RUN until CI**; sanitizers and concurrent teardown remain separately **NOT RUN**.
- **F-09 new Windows VST3 original-chip audible gate:** build an independently sustained (SF2 sampleModes=1) public dual-layer patch, start a held note, reject 513 host NoteOff events or 513 CC points, require the rejected audio block to be zero, verify genuine held-voice extinction in later PCM and subsequent new playable notes. This is **NOT RUN until CI**; the previous one-shot public fixture would have been a false-positive voice-release oracle.
- This change does **not** authorize public release or transfer synthetic tests to private ROM/SBK/user SF2 acceptance. The unmodified P007 Win95 branch remains a separate reference.

### First adversarial-run finding (2026-11-20)

[Win64 run 44829340946](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44829340946) **FAILED** at new F-09 sustain-source precondition: the 768-frame original public one-shot had naturally ended before the held-note observation. No F-09/F-07 PASS was established. The follow-up increases only the *test* sample length to 44,100 frames per voice while retaining looping generator 54, adds early/late PCM checks, and tests that old CC0 does not override a later host bank choice in project recall (F-05). All follow-up test results are **PENDING exact-SHA Windows CI**; no original DSP core changed.

### Source-based correction to synthetic F-09 envelope (2026-11-20)

Windows run 44829978397 failed F-09's early audible fixture condition: first=0.000004041, held=0. The F-05 actual VST3 CC0 versus host-bank project recall/PCM gate passed independently. Pinned upstream AWE32Emu SoundFont.cpp (lines 1114-1141) specifies that a missing sustainVolEnv generator maps to Creative volume-envelope register sustain ZERO (decay to silence). Explicit SF2 generator 37=0 cB maps to original EMU8000 0x7F full-volume sustain. The F-09 generated looping test bank now has BOTH generator 37 and generator 54, and a full second of PCM. Verify the following Windows CI result before marking F-09 or F-07 PASS; no production DSP was changed.

### Native Win64 F-09 host-harness capacity correction

[Windows run 44830706267](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44830706267) failed while preparing 513 VST3 events, **not** while testing synthesizer overflow. Genuine sustained original-chip positive PCM was now proven (`first=13.728658215`, `held=13.317269059`) and F-05 CC0+host bank PCM/recall passed. The host SDK's `EventList(maxSize=50)` default had silently limited the fixture. The new adversarial host preallocates an independent genuine `EventList(514)` off audio, then challenges the plugin's 512 boundary. This test has not yet passed. This is a test-host correction, no core sound change.

### F-09 overflow reaches true held voice: wet-tail isolation required

Windows run 44831309815 failed F-09's audible post-overflow extinction check. This is NOT a PASS: true sustained source was audible, and the plugin rejected >512 events, but post-rejection PCM was still detected. Original Creative defaults can feed hardware reverb/chorus; CC120 intentionally kills voices, not legitimate FX tails. The next falsifiable A/B test sets explicit MANUAL 0% on BOTH send controls before the initial NoteOn, records quantitative held/after PCM and independently tests 513 NoteOff events and 513 CC points. If dry PCM still persists, a genuine stuck-note defect remains OPEN. Core DSP is unchanged.
