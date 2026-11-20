# P001 – nachweisbarer Windows-x64-VST3-Echtzeit-Heap- und Datei-I/O-Test

**11. November 2026 – CI PASS**, [Actions #43088493529](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43088493529), Quellstand `4d98dbb7c4ef58f4e2ed4c736f9db204693fb18e`.

## Konkret behobener Fehler im Original-Fork-Adapter

Der unveränderte Original-Fork `snd_emu8k.c` unterstützt über die Umgebungsvariable `EMU8K_STATE_DUMP` das Schreiben des EMU8000-Zustands in eine Datei. Falls diese Variable im DAW-Prozess gesetzt war, konnte `emu8k_update()` wiederholt `fprintf()` **direkt im rendernden Audiothread** ausführen. Der originale DSP darf nicht verändert werden.

Deshalb neutralisiert unser separater `src/chip_adapter/Chip.cpp` unmittelbar **nach** `emu8k_init` den optional geöffneten `state_dump`-Stream per `std::fclose` und setzt den Zeiger auf `nullptr`. Das geschieht ausschließlich bei Chip-Aktivierung, **vor** `process()`. Die unveränderte Git-Blob-SHA des Original-DSP bleibt `e60eb10f00a8e1187380faa25989f6ba0433d99a`.

`tests/chip_contract.cpp` aktiviert den ursprünglichen Debugpfad absichtlich über die echte C-Laufzeitumgebung und rendert **12.800 Frames**, also mehrere 4.410-Frame-Dumpintervalle. Erwartet und in CI gemessen: erzeugte Original-Debugdatei mit **0 Byte**, `state_dump == nullptr`; PASS. Die Testbedingung wird nicht durch Weglassen der Umgebungsvariable umgangen.

## Tatsächliches exportiertes VST3: Heap-Messung

`tests/p001_vst3_rt_host.cpp` lädt per Windows `LoadLibraryW` und `GetPluginFactory` genau die im Windows-2022-Runner gebaute echte `AWE64_Gold_P001.vst3`. Die Datei wird mit dem SHA-geprüften **originalen** AWE32/Creative-ROM (1 MiB, SHA256 `4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e`) initialisiert. Keine synthetische ROM oder Ersatzengine in dieser Abnahme.

**Testinstrumentierung ist ausschließlich im Test-Host**, nicht im Produktcode: Gepinnte MinHook v1.3.4, Commit `c3fcafdc10146beb5919319d0683e44e3c30d537`, fängt den Windows-Heap-Allokationspfad `ntdll!RtlAllocateHeap` ab. Eine echte `HeapAlloc`-Anforderung von 64 Byte **innerhalb des scharfgeschalteten Zählfensters** muss mindestens eine Allokation erfassen; sonst FAIL. Erst danach werden die tatsächlichen VST3-`process()`-Aufrufe überwacht. Allokationen, die während eines Audioblocks erzeugt und vor dessen Ende wieder freigegeben werden, wären sichtbar; der Test betrachtet nicht bloß die Heapgröße vor/nach dem Callback.

Während der Messung sendet der Host über den VST3-Eventbus reale NoteOn- und NoteOff-Nachrichten an die eine derzeit implementierte P001-ROM-Teststimme; Blockgrößen: 1, 7, 32, 64, 128, 255, 512, 1024 Frames.

| Messgröße (Run #43088493529) | Nachweis |
|---|---:|
| Tatsächlich ausgeführte VST3-Audio-Callbacks | **1.200** |
| Gesamte VST3-Audioframes | **303.450** |
| Nicht stumme digitale Ausgangs-PCM-Werte | **416.158** |
| `RtlAllocateHeap`-Aufrufe im Audio-Callback | **0** |
| Im Audio-Callback angeforderte Heap-Bytes | **0** |
| Während Callback erzeugte Original-Fork-Debugausgabe | **0 Byte** |
| Größte einzelne gemessene Verarbeitungszeit | **614,7 µs** (nur Messwert auf gemeinsam genutztem CI-Runner) |
| Alter Windows-Chip/RAM-/Direkt-Fork-Test | **2/2 PASS** |
| Steinberg-VST3-Validator | **537 PASS / 0 FAIL** |
| Echtes VST3-MIDI-NoteOn/NoteOff | **PASS** |
| Temporäres Original-ROM und WAV nach Test | **gelöscht** |

Das unabhängig laufende Win95-Creative-Original-MIDI-Replay (8.972.880 bitidentische Samples, fünf echte WinMM-Notenpaare) ist zusätzlich in `docs/P001-ORIGINAL-CREATIVE-VM-MUSICAL-PCM-PASS.md` dokumentiert. Die dortige VM-Referenz war ein eigener privater Lauf und **kein Teil** dieser Windows-Actions-Runner-Tests.

## Noch keine unbedingte Echtzeit-/Hostfreigabe

Dieser Test weist **keine Windows-Heap-Allokationen über den erfassten `RtlAllocateHeap`-Pfad** bei den 1.200 tatsächlichen Windows-VST3-Aufrufen nach und schließt einen konkreten Fork-Debug-Dateipfad. Er ist **kein Beweis**, dass alle denkbaren Windows-Speicherzuweisungswege (z. B. direkter `VirtualAlloc`), alle Laufzeiten und alle Audiogeräte abgedeckt sind. Messungen auf einem gemeinsam genutzten CI-Runner begründen **keine ASIO-Deadline-Garantie**.

Weiter offen bleiben ein Studio-One-8.1.2-Praxistest auf tatsächlicher Windows-/ASIO-Hardware, ein breiteres Echtzeitprofil inklusive eventueller anderer Allokations- und Synchronisationswege sowie P002/P003 für die volle Creative-SBK/SF2-Bank-/Programmübersetzung. Der originale Fork und die synthetische Vorgängerengine werden nicht modifiziert. P001 bleibt offen; PR #2 bleibt Draft und `main` unverändert.
