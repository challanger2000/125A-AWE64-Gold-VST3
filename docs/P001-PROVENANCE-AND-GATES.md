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


## P001 additional direct-original-chip regression (2026-11-11)

`awe64_direct_fork_fixture` directly initializes the pinned original `snd_emu8k.c` in a separate process, generates a **synthetic 1 MiB ROM with nonzero waveform data**, programs the physical EMU8000 register ports (AWETEST tone setup), records the fork's own PCM16 and register readbacks, then invokes `awe64_replay` against it. This is an **adapter parity unit test against a direct original chip invocation**, not a trace or acoustic reference recorded from the Windows-95/86Box virtual machine. It never uses the former 125A modified engine or licensed ROM files.

The accompanying Python adversarial test verifies that (a) the original core actually produces nonzero PCM, (b) all digital samples equal the adapter's output with different callback partitions, (c) a one-bit corrupted sample is identified at its precise frame and channel, (d) a mutated register-read result fails, and (e) an absent WAV hard-fails. The first write occurs at frame 11: real fork traces legitimately need not begin with an I/O operation at frame zero. This removes a false-negative condition from `awe64_replay`.

An independent real 86Box/Creative captured reference with authenticated 28-MiB RAM/ROM identity **still remains the separate mandatory P001 acceptance gate**. A successful synthetic direct-core test must not be described as full 86Box VM/driver audio parity or as an actual physical card test.


## Native RAM port I/O negative boundary verification

The additional Windows contract now writes and reads RAM through the **original chip's EMU8000 port registers** (SMALW/SMLD and SMALR, including the one-word read pipeline) in addition to inspecting the pointer pages and `uploadRam` memory. It writes the first installed word (0x200000), the last sample-usable word (0xFFFFDF), and checks that an attempted write at the reserved FM boundary (0xFFFFE0) and a write below the installed RAM cannot corrupt sample DRAM. This checks the unchanged fork hardware register path; it is not a Creative driver/SBK upload test.

## Privater echter ROM-Audiotest ohne Media-Upload

`tests/direct_fork_fixture.cpp` besitzt jetzt den zusätzlichen Modus `--real-rom PRIVATE_ROM PRIVATE_OUTDIR`. Er rendert ein zwei Sekunden langes, über original EMU8000-Ports programmiertes Testsignal **direkt aus der tatsächlichen Creative-ROM**, mit Tonstart, Registeränderung und NoteOff. Dieselbe Registersequenz wird mit dem nativen Adapter über `awe64_replay` bytegenau nachgerechnet.

`tools/p001_real_rom_check.py` prüft vorab die Original-ROM-/SBK-SHA256, verlangt tatsächlich nicht stummes PCM, vergleicht sämtliche 176.400 Stereo-PCM16-Samples einschließlich RIFF-Datei-SHA und schreibt ein rein privates JSON-Protokoll. Jede Ausgabe erfolgt ausschließlich **außerhalb des Git-Repositorys**, ohne Originaldateien in CI, Issues oder Artifacts hochzuladen.

Nach dem Bauen auf Windows genügt `tools/P001-PRIVATE-REAL-ROM.bat "C:\\PRIVATE\\awe32.raw" "C:\\PRIVATE\\SYNTHGM.SBK"` für den vollständigen privaten Lauf. Eine lokale Original-ROM-/SBK-Prüfung mit den tatsächlich vorhandenen Bibliotheksdateien wurde durchgeführt und beide Fingerprints bestätigt; der **genuine-ROM-Chip-PCMAudiorun mit dem Fork selbst** ist erst dann PASSED, wenn dessen ausführbarer Test auf denselben privaten Dateien gelaufen ist. Der generische Windows-2022-Runner enthält das Eigentümer-ROM absichtlich nicht, daher kann dessen synthetischer grüner Build einen privaten real-ROM-Lauf nicht ersetzen.

Diese Prüfung ist noch **nicht** die unabhängige, von der laufenden 86Box-VM mit originalem Creative-Treiber aufgezeichnete `EMU8K_TRACE`/`AWE32_WAV`-Abnahme. Auch nach bestandenem Privat-ROM-Test bleibt die P001-VST3-Freigabe bis zu diesem echten Fork-VM-PCM-Nachweis gesperrt.
