# P007-R — Original Creative Windows95 -> EMU8000 at 48 kHz host output

**Date:** 17 November 2026. **Status:** PRIVATE Linux QA original guest **PASS**; Windows x64 VST3 / DAW acceptance still pending.

## Reason
The pinned original 86Box-AWE32 `src/include/86box/sound.h` defines **`WT_FREQ = FREQ_44100`**. The original AWE64 Gold hardware wavetable mixer therefore produces **44,100 stereo frames per second**, regardless of the external DAW's session sample rate. Without post-mixer rate conversion, a host at 48,000/96,000 samples/s would play the original 44.1-kHz audio too fast and sharp.

## Change
P007-R adds a private, fixed-stack, single-consumer `P007OutputRate` adapter between the **unaltered original PCM ring** and the VST3 host audio output. At 44.1 kHz, samples are copied as-is, with **no interpolation or sample modification**. At other host rates, output linear interpolation advances through original samples at the ratio `44,100 / hostSampleRate`. It performs no allocation, blocking call, thread creation or shared mutex in `process()`; the DSP, filter, voice allocation, tuning and register simulation of `snd_emu8k.c` are **unmodified**.

## Unit checks
- Host **44,100 Hz:** 2,570 source samples -> 2,570 output samples, exact original values.
- Host **48,000 Hz:** 2,363 source samples -> 2,570 output samples, expected time alignment.
- Host **96,000 Hz:** 1,182 source -> 2,570 output, expected time alignment.
- Host **32,000 Hz:** 3,543 source -> 2,570 output, expected time alignment.
- Cross-block continuity (257-sample block × 10), stereo sign/amplitude, underrun+reprimes, invalid host sample-rate fallback: **PASS**.
- [Native Windows original-core and VST3 CI PASS](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44136861756): four host sample rates, stereo continuity, invalid-rate fallback and underrun tests pass; **47/47 Steinberg VST3 validator**; in-folder original core DLL load passes. The validation error from the prior run was addressed by preserving Steinberg's `setupProcessing` lifecycle semantics.

## PRIVATE real Windows95/Creative sound check at **48,000 Hz**
Used exact pinned original 86Box core, a **private copy** of the actual Win95 HDD, Creative AWE64 Gold VXD/DRV, genuine ROM assets, original Win95 guest relay and automatic keyboard boot assistant. The Linux host deliberately provided no audio callbacks during the approximate 40-second boot; after real Creative READY, it simulated a real-time **48-frame stereo callback every 1 ms**, consuming the original 44.1-kHz wavetable via the P007-R source-compatible rate adapter. No audio wav files were replayed or substituted.

Recorded output:
- `P007_AUTOMATIC_GUEST_BOOT_AID tick=7500 key=F1` and `tick=37500 key=ENTER` from original emulated hardware.
- `P007_REAL_WIN95_CREATIVE_READY slice=39562 noteon=1`
- `P007_REAL_WIN95_CREATIVE_NOTEOFF slice=42563 success=1`
- `P007_HOST_48000_REAL_PRIVATE_RESULT code=0 ready=1 note=1 noteOff=1 pcmNonzero=294958 peak=0.02451096 HddReads=1562 HddWrites=2642`
- `P007_REAL_CREATIVE_48000_HOST_RATE_NOTEON_PCM_PASS`; process exit code 0.

**Limitations:** 48k adapter uses initial linear reconstruction (not yet a studio-quality bandlimited SRC). Only 44.1k is bit-identical to the original wavetable output. The private-test pass is for the same-source Linux QA emulator and does **not** yet establish native Windows VST3 StudioPro/ASIO audio, GUI, latency or multi-instance acceptance. All proprietary files remain outside public GitHub.

## Private single-VST3 package
A locally generated research package **125A-AWE64-Gold-P007R-44100-48000-96000-PRIVATE-20261117.zip**, 89.99 MiB compressed, contains exactly one `AWE64_Gold_P007.vst3` folder with 21 native Windows CI components from the passing run, the unchanged private 512 MiB FAT16 Win95 Creative guest and ROM assets. The ZIP CRC, original OS disk SHA-256, original Creative VXD/DRV, guest relay and source-pinned core are checked; **no private binaries are stored in the public repository**. Native VST3 SHA256 `dd263014ea853e9686aa3f189231abe68f81a23ba086dc0f78142e3b19aa3d76`. Original core DLL SHA256 `eea32ff8727cf38f1cf80dfdaca00c78e136dc1047249d36d58249d19ce5349a`.
