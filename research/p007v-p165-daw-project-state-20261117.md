# P007V — Real DAW project state + original P165 GUI (17 Nov 2026)

**Status:** Research, not a final Studio Pro/ASIO release. No private Windows 95, Creative, SoundFont or original ROM binaries are uploaded to GitHub.

## Actual VST3 implementation

- Implements `getState(IBStream*)`, `setState(IBStream*)` and `setComponentState(IBStream*)` on the existing `SingleComponentEffect`.
- Only **original Creative MIDI Bank Select, Program Change** and **post-EMU8000 output master gain** are serialized. The 512 MiB private Win95 image remains outside the project file in its existing per-user writable store, not in the DAW project.
- Versioned, canonical **16-byte** project-state format with original parameter values and FNV1a hash over the payload. Corrupt, unknown-version or out-of-range input is rejected without modifying active settings.
- Bank/program/master values are atomic across GUI/host control and real-time audio callback. Loaded states update the actual P165 editor parameters and request resend of the original Creative MIDI bank/program when its guest UART reports READY. Original EMU8000 core/DSP remains unchanged.

## Independent Windows CI acceptance

[Windows-2022 P007V CI **PASS**](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44152625740)

- **80/80** versioned state codec roundtrip combinations and **1,280/1,280** one-byte corruption rejection cases run in Release with assert enabled: `P007V_PROJECT_STATE_ROUNDTRIP_PASS cases=80 tamper_rejections=1280`.
- **47/47** native Steinberg VST3 validator tests passed.
- Actual Win32 Steinberg EditorHost loaded native compiled P165 frontplate and passed original VSTGUI status/canvas/program Prev/Next/Master and SYNTH/BROWSER page/row-selection interactions.
- Windows x64 native plugin module binary SHA-256 `a9c165c5ec36e87d71a07bfd04743c5ea9b867d47029c3c7e66bbf2e90545a49`.
- Original in-process 86Box core binary SHA-256 `87b295d3286f2ad426725d7f342ee2d8594bdd28ba9a1412429af310d3526a10`.

## Private single-VST3 research ZIP

Created only in the current private local workspace: `125A-AWE64-Gold-P007V-P165-STATE-PRIVATE-20261117.zip`. It contains **617 files inside exactly one `AWE64_Gold_P007.vst3` directory**, including the original user-supplied 512 MiB Windows 95 guest disk, the original Creative driver + ROM and all native emulator DLLs. It replaces only verified P007V Windows CI files, leaving private OS/data untouched.

- ZIP CRC: **PASS for all 617 entries**.
- Private original Win95 image SHA-256 unchanged: `e7a62dfcd8906e4aefffe0ffea8d93c6fe615f8be26a279b9086b4eb68ae8802`.
- Private research ZIP SHA-256 `a9d667af20583e61e524cbe9681ca7de2f88660306b8752430c76dea23446661`.

**Limits:** The P007V state codec and native plugin build are validated, but a complete project save/close/reopen inside Windows Studio Pro, final ASIO audio playback, original Creative SF2 preset metadata, ADSR/filter/FX controls and multi-instance behavior are not yet accepted. Do not imply VST3 product release.
