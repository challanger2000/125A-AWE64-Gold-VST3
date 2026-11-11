# P001 provenance and acceptance (not a release report)

## P001 acceptance vs. final product acceptance — clarified 11 November 2026

**P001 is technically implemented, tested, and ready for the independent control chat's review of [Issue #1](https://github.com/challanger2000/125A-AWE64-Gold-VST3/issues/1).** The original Issue #1 requires an unchanged pinned EMU8000 kernel, original ROM and 28 MiB RAM, native Windows register/PCM adapter, real independent fork original-driver PCM parity with mismatch controls, a minimum Windows VST3 with MIDI NoteOn/NoteOff and no audio render-thread heap allocations under measured conditions, and Windows CI. The corresponding real tests and version fingerprints are documented below; this is **not** a full-product soundbank/GUI/release PASS.

**Correct development order and use of the existing 125A Plugin Test Lab:**

1. **P001 technical core (now ready for control review):** verified source/ROM/RAM, genuine original Creative Win95 driver musical PCM, native exported Windows VST3, Steinberg validator, sampled MIDI and callback heap/no-debug-I/O probes. Control chat reviews the evidence; pending review is a governance state rather than a failed technical test.
2. **P002/P003 and subsequent development:** original Creative `SYNTHGM.SBK` and SF2 loading, GM/bank/program mapping, actual original driver note/controller semantics, multi-voice behavior and further engine functionality. Run repeated **technical and automated tests in the shared [125A Plugin Test Lab](https://github.com/challanger2000/125A-Plugin-Test-Lab)** using P74/P75 universal native Windows VST3 QA, available P65/P67 product coverage, independent audio/state/reload/stress cases, and real source/binary SHA provenance. Explicit missing coverage stays `NOT RUN`, not PASS. A minimal no-GUI VST3 is valid for headless P73 inventory and technical QA.
3. **Only at final GUI/product acceptance**, after soundbanks, controls, presentation and instrument behavior are implemented, undertake actual end-user **Studio One + physical ASIO** acceptance on the user's Windows machine. Earlier automated Host/Steinberg/Lab tests remain mandatory during development but **must not be held hostage by premature hardware/GUI testing**.

The unmeasured physical ASIO/Studio One end-user session is **deliberately deferred**. It is **not a requirement for P001 completion under Issue #1** and should not be repeated as an early P001 blocker. The draft PR stays unmerged and the issue stays open until an independent control review; neither status proves P001 technical work incomplete.


**CURRENT RT GATE (11 Nov 2026):** Real Windows exported VST3 callback audit using a positive-control-verified `ntdll!RtlAllocateHeap` hook in an independent host: **1,200 callbacks / 303,450 frames / 416,158 nonzero values / 0 heap calls / 0 allocated bytes**, original env-triggered EMU8000 state-dump file **0 bytes** after 12,800-frame regression. Windows Actions **#43088493529 SUCCESS**, 537 Steinberg validator tests, 2/2 CTests. Details and limits in `docs/P001-RT-HEAP-AND-DEBUG-IO-AUDIT.md`. Studio One/ASIO on physical Windows audio hardware and other allocation paths remain unverified.


**LATEST SUPERSEDING EVIDENCE:** 11 Nov 2026 independent Win95 original-Creative FIVE-NOTE musical digital PCM parity PASS, 12,861,520/12,861,520 samples, 398,777 nonzero, 93,445 real guest chip-register operations. See docs/P001-ORIGINAL-CREATIVE-VM-MUSICAL-PCM-PASS.md. Earlier NO-GO language below describes historical pre-capture gaps, not the current conclusion. Studio One/RT and full SBK/SF2 are still open.

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

**P001 final independent 86Box-VM PCM gate remains BLOCKED.** The genuine-ROM direct original-C-core vs adapter test now passes (176,400 exact PCM samples), and a minimal exported Windows VST3 exists and passes Steinberg validator and a genuine-ROM host MIDI test. This is not proof of a separately booted original Creative Win95 VM nor the physical AWE64 Gold; the issue remains open.

## Tests and negative controls

- `p001_chip_ram_rom_contract`: actual 28 MiB mapping, first and final sampleable RAM word, reject reserved FM and out-of-bounds writes without mutation, reject malformed ROM, deterministic block partition. Always-active checks under Windows Release.
- `tools/check_private_rom.py`: wrong digest and unsupported size fail closed.
- `awe64_replay`: missing/invalid WAV, empty/nonmonotonic trace, register-read mismatch, and first PCM mismatch all fail with distinct exit codes. Must not be called a successful *fork differential* until an independent reference has actually been supplied.
- Pending: independently captured 86Box/Win95/Creative register + WAV and its mutant, complete bank loading through original Creative port traffic, Studio One/ASIO verification, and measured realtime allocation/deadline behavior. Genuine-ROM direct chip and actual VST3 unit/host tests already pass.

Issue #1 is **not closed**. Direct original-chip PCM identity and exported VST3-ROM test-audio are measured; independent booted Win95/Creative VM, physical AWE64 Gold, SBK/SF2 and production/preset acoustic fidelity are **not** measured.


## P001 additional direct-original-chip regression (2026-11-11)

`awe64_direct_fork_fixture` directly initializes the pinned original `snd_emu8k.c` in a separate process, generates a **synthetic 1 MiB ROM with nonzero waveform data**, programs the physical EMU8000 register ports (AWETEST tone setup), records the fork's own PCM16 and register readbacks, then invokes `awe64_replay` against it. This is an **adapter parity unit test against a direct original chip invocation**, not a trace or acoustic reference recorded from the Windows-95/86Box virtual machine. It never uses the former 125A modified engine or licensed ROM files.

The accompanying Python adversarial test verifies that (a) the original core actually produces nonzero PCM, (b) all digital samples equal the adapter's output with different callback partitions, (c) a one-bit corrupted sample is identified at its precise frame and channel, (d) a mutated register-read result fails, and (e) an absent WAV hard-fails. The first write occurs at frame 11: real fork traces legitimately need not begin with an I/O operation at frame zero. This removes a false-negative condition from `awe64_replay`.

An independent running 86Box VM/Creative captured reference with authenticated 28-MiB RAM/ROM identity **still remains a separate mandatory P001 acceptance gate**. Synthetic or genuine-ROM direct-core/adapter tests must not be described as full Win95 VM/driver audio parity or as an actual physical card test.


## Native RAM port I/O negative boundary verification

The additional Windows contract now writes and reads RAM through the **original chip's EMU8000 port registers** (SMALW/SMLD and SMALR, including the one-word read pipeline) in addition to inspecting the pointer pages and `uploadRam` memory. It writes the first installed word (0x200000), the last sample-usable word (0xFFFFDF), and checks that an attempted write at the reserved FM boundary (0xFFFFE0) and a write below the installed RAM cannot corrupt sample DRAM. This checks the unchanged fork hardware register path; it is not a Creative driver/SBK upload test.

## Privater echter ROM-Audiotest ohne Media-Upload

`tests/direct_fork_fixture.cpp` besitzt jetzt den zusätzlichen Modus `--real-rom PRIVATE_ROM PRIVATE_OUTDIR`. Er rendert ein zwei Sekunden langes, über original EMU8000-Ports programmiertes Testsignal **direkt aus der tatsächlichen Creative-ROM**, mit Tonstart, Registeränderung und NoteOff. Dieselbe Registersequenz wird mit dem nativen Adapter über `awe64_replay` bytegenau nachgerechnet.

`tools/p001_real_rom_check.py` prüft vorab die Original-ROM-/SBK-SHA256, verlangt tatsächlich nicht stummes PCM, vergleicht sämtliche 176.400 Stereo-PCM16-Samples einschließlich RIFF-Datei-SHA und schreibt ein rein privates JSON-Protokoll. Jede Ausgabe erfolgt ausschließlich **außerhalb des Git-Repositorys**, ohne Originaldateien in CI, Issues oder Artifacts hochzuladen.

Nach dem Bauen auf Windows genügt `tools/P001-PRIVATE-REAL-ROM.bat "C:\\PRIVATE\\awe32.raw" "C:\\PRIVATE\\SYNTHGM.SBK"` für den vollständigen privaten Lauf. Beide privaten Originaldateien wurden per SHA-256 geprüft. Zusätzlich wurde das **byteidentische echte ROM aus dem offiziellen 86Box-ROM-Repository** in Windows-2022-CI temporär geladen und mit dem unveränderten Originalkern sowie dem VST3-Chipadapter tatsächlich gerendert: **176.400/176.400 PCM16-Samples bitidentisch**, 176.296 nicht stumme Samples (siehe Actions #43064109027 und #43068548526). Das Original-SBK wird in diesem ROM-Test nicht geladen; keine privaten Medienbytes werden veröffentlicht.

Diese Prüfung ist noch **nicht** die unabhängige, von der laufenden 86Box-VM mit originalem Creative-Treiber aufgezeichnete `EMU8K_TRACE`/`AWE32_WAV`-Abnahme. Der minimale, klar als **P001-ROM-Testinstrument** bezeichnete VST3 wurde nach dem direkten ROM-Core-Parity-Nachweis entwickelt und inzwischen erfolgreich unter Windows getestet; die **P001-Gesamtabnahme** bleibt bis zur unabhängigen 86Box-VM-PCM-Referenz offen. Es gibt keine Produkt-/Klangtreuefreigabe.

## CI genuine owner-identical ROM (without private upload)

The public `86Box/roms` Git blob at `sound/creative/awe32.raw` is **byte-for-byte identical** to the owner's supplied private `awe32(1).raw` (SHA256 `4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e`, Git blob `345b5a177e1a75a41bf65d31e1a220c1641c72cb`). The original test input is obtained transiently from pinned `86Box/roms` source commit `8cbb1acc575418e8afe9741a656dfd1c657574d1`; the 1-MiB file and its digital WAV/trace are deleted from the Windows runner after computing private sample comparison. The project's **existing private** `SYNTHGM.SBK` is **not** downloaded, published, or loaded by this ROM-only CI test.

The same original unmodified `snd_emu8k.c` is driven directly and through the adapter. The test requires 88,200 44.1-kHz stereo frames, **176,400 exactly matching PCM16 samples**, a non-silent note with start/attenuation change/stop and byte-identical private WAV SHA256. It hard-fails on any mismatch; this is the first real-original-ROM direct-core adapter parity proof in CI, **not** the independent Win95/Creative/86Box VM trace gate. The definitive run SHA and actual measured nonzero count must be taken from the Windows Actions result, not assumed from this source addition.

## Minimaler original-ROM-Windows-VST3 – tatsächlich durchgeführte Abnahme

Verbindlicher Pin: Originalfork `fee1e81977824fd2d5eed4d239589dc30d251dd7`, unveränderter DSP-Blob `e60eb10f00a8e1187380faa25989f6ba0433d99a`; Steinberg SDK `v3.8.1_build_84` SHA `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`.

P001 VST3-Quellcode liegt unter `src/vst3/P001Minimal.{h,cpp}` plus `P001Factory.cpp`; lädt das externe ROM bei `setActive(true)` vom **privaten** Host-Pfad `AWE64_P001_ROM_PATH`, allokiert das reale 28-MiB-RAM **vor** der Audioverarbeitung, bietet ein Stereo-Instrument bei genau 44.100 Hz als 32-Bit-Float an und rendert per samplegenauem MIDI-VST3-Event ein **einziges AWETEST-Test-ROM-Instrument** über originale Chip-Register. Nicht 16-kanalig polyphon, keine GM- oder SBK-Presetübersetzung, keine P002/003-Treibersemantik. Fehlendes ROM ist ein tatsächlicher Aktivierungsfehler, keine stumme Ersatzengine. Studio One/ASIO und freie Sample-Rates werden noch nicht behauptet.

### Authentisches Windows-Ereignisprotokoll

[GitHub Actions #43068548526](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43068548526), commit `ce22e0cbf11201a37bebd5f46b2d2fa28c571658`, **SUCCESS**:

- VST3-DLL aus dem tatsächlich gepinnten Originalkern kompiliert; SHA-256 `45e478bfaef832634ca00984c2f5ad3fd04b1aa33cefffd1c66260ae721627cf`.
- Steinberg SDK Validator **537 PASS / 0 FAIL**, Original-ROM im Prozess während der Validierung vorhanden. Andere Abtastraten als 44.100 Hz werden bewusst verweigert, nicht falsch umgerechnet; nicht jede SDK-Prüfung ist ein Studio-One-Praxistest.
- Unabhängiger `awe64_p001_vst3_host.exe` lädt die **exportierte DLL** über `GetPluginFactory`, vereinbart Stereo, rendert Original-ROM-Audio über MIDI und prüft NoteOn bei PCM-Frame **137** und NoteOff bei **1151**; Ausgabe **18.432 Frames**, **18.272 nicht stumme Samples**, Samples bis NoteOff in Vergleichsläufen identisch und nach NoteOff messbar verändert.
- Automatisierte **2/2 CTest PASS** für echten RAM-Port-DMA und original-kernbasierten synthetischen PCM-Differentialtest; Original-ROM-Differenz **176.400/176.400 Samples PASS**; Manipulationen von PCM und Registerantworten führen erwartungsgemäß zu FAIL.
- Auftrennung: Dieser VST3-Hosttest nutzt echte ROM-Mediendaten und echte DLL/Hostevents, aber **keinen unabhängig laufenden Win95/86Box-/Creative-Treiber** und keinen Studio-One-Test. Audio-Thread-Allokationsfreiheit ist durch den minimalen Quellpfad plausibel und das Design beabsichtigt, aber noch **nicht** durch unabhängiges ETW-Laufzeitprofil vollständig belegt.

### Gefundener echter MIDI-Fehler und Korrektur

[Früherer Windows-Run #43067785464](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43067785464) zeigte bei realer DLL-Audioausgabe, dass `DCYSUSV=0x8080` keinen hörbaren Release hervorbrachte: Bit 7 deaktiviert den originalen Hüllkurvengenerator. Die P001-Teststimme verwendet seit `ce22e0c` `DCYSUSV=0x807F` (Releasebit + aktiver Generator + schneller Test-Release). **Keine Änderung am Fork-DSP.** Die endgültige, programmabhängige originale Creative-NoteOff-Registerfolge bleibt ausdrücklich P002.

### Noch offen / NO-GO

- Register/WAV-Differenzialtest gegen eine **separat laufende** 86Box-AWE32-VM mit Original-Windows-95-Creative-Treiber und identischer RAM-/ROM-/Registersequenz.
- Eigenständige Creative 1MGM-Soundbank-Presets, tatsächliche Drums, SF2/Generatoren, echte Treiber-GM-/Controllerlogik.
- Unabhängiges Echtzeit-Allokations-/Deadline-Profil und praktische Studio-One-Test-Lab-/ASIO-Prüfung.
- GPL-2.0-/Steinberg-SDK-Lizenzverträglichkeit vor jeder Verteilung; keine privaten ROM/SBK-/Treiber-Binaries in einer Release.

Der PR bleibt **Draft**, P001 bleibt **offen**. Ein grüner Build ist weder P002/P003 noch die vollständige AWE64-Gold-Klangtreue.
