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

## 16.11.2026 – korrigierter originaler Bank-2-Vorzustand und CPU-TSC

Der Kontrollchat hat die Reihenfolge der originalen Windows-95-Gastaufrufe
korrigiert. **Die 4.176 MIDI-Stressaufrufe fanden erst NACH dem zweiten
RAM-SoundFont-Upload statt.** Die Reproduktion von Bank 2 darf deshalb
niemals einen vorherigen 4.176-Meldungen-Stresstest voraussetzen oder als
dessen Folge simulieren. Vor Bank 2 relevant ist die kurze originale
C4-/D4-Sequenz nach dem ersten RAM-Bank-Upload:

1. Original Creative Bank 1 laden; Original-MIDI-Reset/CC121, CC0=1,
   Program 0, Reverb-/Chorus-Sends=0.
2. WinMM C4 NoteOn (MIDI 60, Velocity 100), NoteOff.
3. Direkter Creative-MIDI-Pfad: CC0=1, Program 0, CC91=0,
   CC93=0, D4 NoteOn (MIDI 62, Velocity 100), NoteOff.
4. **Erst dann** Original-RAM-Bank 2 laden; anschließend die
   gesonderte MIDI-Stressphase, die nichts am Bank-2-Eingangszustand
   nachträglich ändern darf.

Dies ist ein **Test-/Integrationsziel**, kein Nachweis, dass der jetzige
P005-VST3-Adapter schon dieselben Vorzustands-/Hardware-Registerwerte
autonom erzeugt. Der Port verwendet dafür einen gemeinsamen
`OriginalCreativeMidiRamRuntime` mit genau einem `MidiEngine.device`.

Der neue unabhängige Originalbefund verwendet eine effektive CPU-Taktbasis
von **133 1/3 MHz** (400.000.000/3 Takte/s), nicht 200 MHz. Die
CPU-TSC-zu-EMU8000-Projektion bei 44.100 Audioframes/s ordnet nach
Kontrollbericht **1.533.303 von 1.533.305** ursprünglichen
Hardwareereignissen auf einen einzigen ganzzahligen Frame-/Phasenmaßstab
ab. Zwei sehr frühe E22-Leseereignisse bleiben Ausnahmen
(413 Frames Abweichung im unabhängigen Prüfer).

**Nicht daraus ableiten:** eigenständig bekannte CPU-Zeitpunkte
einzelner Creative-Funktionen. Der angekündigte separate C++23-Baustein
für den Zeitphasen-Nachweis wurde noch nicht als neue Quelle in diesem
P005-Quellzweig übergeben. Bis zum Import des source-authentifizierten
Bausteins, ihrer originalen CPU-Arbeitskosten und zum VST3-Gegentest
wird in P005 keine eigene 133-MHz-Heuristik und auch kein
200-MHz-Fallback zur angeblichen Originaltaktung erzeugt.
Keine stillschweigende Herkunftsbehauptung gegenüber dem Win95-Trace.

## Native Windows-Abnahme des Quellintegrationszweigs

Im GitHub-Windows-Runner mit `AWE64_ENABLE_P005_ORIGINAL_CREATIVE=ON`
haben **69/69 CTests** bestanden. Alle 24 originalen Portierungs-Tests,
der P005-Adaptertest und die bestehenden VST3-Source-/Chip-Tests
waren Teil dieses Laufes. Das originale EMU8000-/CT1745-Differential
ist ebenfalls bestanden. Dies belegt den Windows-x64-C++23-Quellbau
und die getestete gemeinsame MIDI/RAM-State-Grenze, **nicht** die
vollständige native VST3-Register-/PCM-Parität.



## P005 – Original-SF2-Layer-Laufzeitstufe (16.11.2026)

\`src/vst3/P005OriginalCreativeSf2NoteStage.hpp\` ist im echten VST3-Prozess
als **nicht-RT Vorbereitungsstufe** und mit derselben portierten Creative-
Quellbibliothek verbunden. Die Klasse verwendet ausschließlich
\`select_sf2_raw_layers\` und \`prepare_original_sf2_zones\` aus dem
portierten Original-SF2-Reader/Ressourcencompiler. Sie erzeugt selbst
**keine** Filter-, Pitch-, Hüllkurven- oder Stimmen-Algorithmen.

Ein authentifizierter Note-Kontext kann so im Voraus in maximal sechs
Original-SF2-Layern vorbereitet und mit \`VerifiedSf2Bank\` unmittelbar
an \`P005OriginalCreativeHostAdapter\` übergeben werden. Quelle/Map
werden **nicht** kopiert oder erfunden. Die Vorbereitungsstufe ist
bewusst nur für eine konkrete Bank-/Programm-/Tasten-/Velocity-Kombination
freigeschaltet; das ersetzt noch keine vollständige freie 77-Preset-
Klangbibliothek. Eine nicht authentifizierte Tabelle wird abgewiesen.
Der komplette Produktions-MIDI-Zweig bleibt so lange ungeschaltet,
bis die Tabellen für den gesamten geladenen Bankkontext authentifiziert
und die Original-CPU-Zeitfolge reproduziert sind.

Der unabhängige öffentliche Test
\`tests/p005_vst3_sf2_note_stage.cpp\` prüft:
RIFF/SF2-Struktur → 1996-Creative-Quell-Layerreader →
Originalressourcen-Compiler → feste Quell-Zone →
gemeinsamen MIDI/VXD-DeviceState → reale ChipPort-Bridge → NoteOff.
Dafür werden synthetische SF2-Bytes/Tabellen verwendet; private
Creative-Quelldateien bleiben lokal.
Die kombinierte C++23-Quell-Suite: GCC/Clang/Clang-ASan/UBSan
jeweils **27/27 PASS**.

**Lokal, nur mit Originaldateien verifiziert:** Der bestehende portierte
\`select_sf2_raw_layers\` liefert für die 77-Preset-Original-Bank 1
bei Program 0 / C4 / D4 jeweils 2 Layer und bei Program 7 jeweils 4
Layer. Die originale Bank 2 hat für Program 7 bei C4 / D4 jeweils 2
Layer. Kein privater SF2-Sampleinhalt wurde eingecheckt.

### Ergänzender Kontrollchat-Originalbefund: CPU-TSC und RAM-Upload

Der Kontrollchat meldet **666.041** TSC-Gruppen mit 2–3 Ports;
**867.254** benachbarte Events teilen denselben aufgezeichneten TSC,
obwohl feiner aufgelöste CPU-Zyklen innerhalb der Gruppe weiterlaufen.
Eine unkontrollierte Verfeinerung würde **5.312** Original-Portereignisse
um ein Audioframe versetzen. Deshalb **nicht** einfach feine Zyklen auf
den für die Audioframeprojektion maßgebenden Capture-TSC addieren.

Bei aufeinanderfolgenden RAM-Wortzugriffen: 104 Takte in den meisten
Fällen, 100 Takte in insgesamt 3.526 Fällen; jeweils folgt ein
0-Samplewort. **Nicht** pauschal 104 Takte einbauen: nur durch
Original-REA-Funktionszweige erzeugtes Timing darf die autonome
CPU-Taktplanung steuern. Diese Arbeit bleibt beim Kontrollchat, um
eine doppelte eigene Schedulerimplementierung zu vermeiden.
