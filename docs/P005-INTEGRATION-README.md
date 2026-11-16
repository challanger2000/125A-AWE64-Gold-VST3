# P005 – Original-Creative C++23 → 125A-AWE64-Gold-VST3: Integrationspaket

Datum: 16. November 2026
Quelle: 125A-P003-Creative-Port(1).zip (unveränderte Original-Portmodule unter `original_creative_port/`)

## Umfang dieser Änderung

- Die Original-Creative-Portmodule `include/`, `src/`, `tests/` sind vollständig übernommen und nicht umgeschrieben. Keine proprietären Original-VXD-/ROM-/SF2-Daten.
- `integration/P005OriginalCreativeHostAdapter.hpp` delegiert NoteOn, NoteOff, ProgramChange, MIDI-CC und Pitch Bend direkt an `OriginalCreativeMidiRamRuntime::midi_event()`.
- Genau ein `MidiEngine.device`-Zustand wird sowohl für MIDI als auch für den Original-RAM-Scratch-Lader benutzt; derselbe physische `Chip` wird gebunden. Kein zweiter DSP, kein zweiter Controller-/Voice-Algorithmus.
- Fehlende Authentifizierung oder unvollständiger Deskriptor verhindert die Aktivierung. Quellen/Tabellen sind NICHT durch einen booleschen Testwert als echt bewiesen; die tatsächliche Besitzer-Provenienz muss der VST3-Loader in einer separaten Abnahme prüfen.
- `tests/test_p005_vst3_original_port_adapter.cpp` prüft Identität von MIDI- und RAM-Zustand, einmalige Chip-Bindung, Kontrollkanäle, verweigerte nicht verifizierte NoteOn und Nicht-RT-Neuinitialisierung.
- Keine Änderung an Original-EMU8000, CT1745 oder Win95-Portzeitstempeln.

## Lokale unabhängige Abnahme (Quellpaket)

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

- Linux GCC: **25/25 PASS**
- Linux Clang: **25/25 PASS**
- Clang ASan/UBSan: **25/25 PASS**

Die 24 Original-Tests stammen unverändert aus dem Kontrollchat, Test 25 gehört zum VST3-Eingangsadapter.

## Windows-VST3 Integration: bewusst NOCH NICHT als fertig abgenommen

Das vorliegende Paket kann als separate Source-Library kompiliert und in den echten VST3-Prozess eingebunden werden. **Noch nicht implementiert und abgenommen** ist die vollständige Umstellung des bestehenden `P002PrivateRomBank`-Pfads, einschließlich einer durchgehend attestierten echten `OriginalSf2SourceTables`-Erzeugung und Verknüpfung aller Original-Programmslots mit dem geladenen SF2. Wird dieser Eingang nicht bewiesen, darf das VST3 die neuen Original-Funktionen nicht stillschweigend verwenden oder `source_verified=true` aus dem Nichts setzen. 

Fehlt weiterhin: autonomer Win95-x86-/CPU-TSC-Zeitplan, kompletter Bank2-Vorzustand aus ursprünglichem MIDI, bitidentischer nativer Register-/DRY-PCM-Gegentest. Kein Release/keine VST3-Freigabe.

Dieses Paket ist daher ein eigenständiger, getesteter Integrationsschritt, **kein vollständiger Treiberport und kein getesteter Windows-VST3-Endpunkt**.