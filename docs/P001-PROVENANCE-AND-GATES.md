# P001 provenance and acceptance (not a release report)

- New project: `challanger2000/125A-AWE64-Gold-VST3`, development branch `develop/p001-fork-chip-parity`.
- Original source: `turican0/86Box-AWE32` annotated tag `v6.0-AWE32-0.93`, commit `fee1e81977824fd2d5eed4d239589dc30d251dd7`.
- Built C DSP file, unchanged: `src/sound/snd_emu8k.c`, Git blob `e60eb10f00a8e1187380faa25989f6ba0433d99a`.
- The complete pinned source tree is a Git submodule under `third_party/86box-awe32` (no patch). CMake refuses the wrong commit or modified chip blob; source is compiled **from that submodule**.
- Fork license: GPL-2.0; `third_party/86box-awe32/COPYING`. Derived binary source/licensing and Steinberg SDK compatibility must be reviewed before external distribution. No Creative ROM/SBK/driver is included.
- Core: 44.1 kHz, 32 voices, original filter/chorus/reverb/interpolation, no DSP patches.
- Native services are outside original source. The fork expects a single VM clock global: this standalone port uses a per-audio-thread clock cursor. Native register commands must be dispatched on the processing owner thread; cross-thread host event ingress is a later isolated queue.
- RAM: native fork init argument is **28672 KiB**; checked allocation/address map 0x200000–0xFFFFFF. The top 32 words from 0xFFFFE0 are reserved for the original FM mapping; direct sample uploads must reject that range. This distinction matters: installed physical RAM is 28 MiB but not every address can be used for sample upload.
- ROM: private user `awe32.raw` is loaded through a configurable ROM-file service, with format size guard 1048574/1048576. The fork itself applies its original AWE-DUMP header correction if present. Compare external SHA256 to independently recorded digest before an acceptance render.
- `SYNTHGM.SBK` is **not** the chip ROM; SBK/SF2 upload and Creative driver register translation are P002/P003 and are not claimed done.
- The synthetic ROM in `p001_chip_ram_rom_contract` proves memory/buffer invariants only. It is **not** an audio-fidelity reference, nor proof of audible ROM instruments.

## Digital oracle (fail-closed)

The only P001 bit-parity acceptance run uses an **independent** capture from the pinned 86Box-AWE32 fork: `EMU8K_TRACE` (exact 44100-Hz frame/port/value records) and `AWE32_WAV` (signed stereo PCM16 at the chip tap). Capture must start at the same chip frame zero. Use the **same private ROM SHA256**, initialization, 28-MiB RAM contents, and audio timebase. Avoid using arbitrary free-running boot WAV with a newly cropped trace. If the reference includes guest memory uploads, the replay must reproduce the same RAM contents/sequence; missing RAM upload evidence is a **blocked** comparison.

Run `python tools/check_private_rom.py --rom <private/awe32.raw> --sha256 <expected SHA>`, then:
```
awe64_replay --rom <private/awe32.raw> --trace <independent.trace> --fork-wav <independent.wav> --out native.wav
```
Return code 0 means exact equality for **all compared interleaved PCM16 samples** and all returned register read values. A missing reference, mismatched capture boundaries, ROM mismatch, first differing register, frame or PCM sample is a hard fail, not silence-normalized or skipped PASS.

**P001 acceptance remains BLOCKED until the exact independent ROM/trace/WAV data are available and executed.** No VST3 module, Creative driver or GUI is claimed implemented merely because the chip contract builds. Per Issue #1 the minimal VST3 is the *next step after* this parity gate; shipping an unverified module would violate the stated implementation order.

## Tests and negative controls

- `p001_chip_ram_rom_contract`: actual 28 MiB mapping, first and final sampleable RAM word, reject reserved FM and out-of-bounds writes without mutation, reject malformed ROM, deterministic block partition. Always-active checks under Windows Release.
- `tools/check_private_rom.py`: wrong digest and unsupported size fail closed.
- `awe64_replay`: missing/invalid WAV, empty/nonmonotonic trace, register-read mismatch, and first PCM mismatch all fail with distinct exit codes. Must not be called a successful *fork differential* until an independent reference has actually been supplied.
- Pending: negative mutation of valid independently captured WAV, full sample-RAM loading through original Creative port traffic, fork reference ROM voice, actual VST3/Studio One integration.

Issue #1 is **not closed**. No production PCM identity or acoustic fidelity has been measured by these source changes alone.
