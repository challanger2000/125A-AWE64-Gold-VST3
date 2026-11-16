# P002 – Original-Creative-Windows-95-Velocity-Sweep für zwei GM-Programme

**Ergebnis: Original-Win95-/Creative-Treiber-Matrix, 127 Velocity-Stufen je Instrument und digitale EMU8000-Chip-Parität bestanden.**
**Nicht erreicht:** autonome native MIDI-zu-EMU8000-Registererzeugung. Dieser Nachweis ist ein unabhängiges ORIGINAL-Registerorakel, noch keine P002-Gesamtfreigabe.

## Was tatsächlich ausgeführt wurde

- Echte Windows-95-Gastumgebung in der privaten Wegwerfkopie der bestehenden 86Box-AWE32-Testmaschine, originaler Creative-WinMM-Ausgang SB AWE32 MIDI Synth [620].
- Unveränderter, hardwarekalibrierter Chipfork turican0/86Box-AWE32, Commit fee1e81977824fd2d5eed4d239589dc30d251dd7; originaler EMU8000-Quell-Gitblob e60eb10f00a8e1187380faa25989f6ba0433d99a.
- 28 MiB EMU8000-RAM, SHA256 des ursprünglichen 1-MiB-Creative-ROMs 4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e.
- Tatsächlich im Win95-Gast gestartetes, von uns selbst entwickeltes CRT-freies i586-Pentium-PE32 P002VEL.EXE, SHA256 03925f956c34de136565f279e867b055b5d977fac694d7b8f613fa68558c360b. PE/Compiler-Pin durch [Actions #43122682781](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43122682781) erfolgreich geprüft, nicht als simulierte Gastausführung ausgegeben.
- Pro GM-Programm 0 und 40: Note 60 bei **allen 127 Velocity-Werten von 1 bis 127**, mit jeweils NoteOff; anschließend eine gehaltene Note für CC7, CC11, CC10 und Pitchbend.
- Vollständiges Original-WinMM-Gastprotokoll: **640/640** sequenzielle Nachrichten mit Rückgabecode 0 und abschließendem Marker P002_GUEST_ORIGINAL_VELOCITY_SWEEP_SENT_OK count=0x00000280, SHA256 9b7634d5aa10e7283603ad46a828d70acb737267e509529971f47a0233a240e3.
- Originaler unabhängiger 86Box-Chip-Registerabgriff: **120.467 Portereignisse**, davon **72.864 Writes** und **47.603 Reads**. Registerspur SHA256 51ca1b2065279329ec1ba7c8d181f9b8537f4bafdbe454889c98d7292b4c679f.
- Exakt **256** MIDI-NoteOn-Meldungen mit Velocity >0 (254 Velocity-Messnoten und 2 gehaltene Controller-Noten) korrespondieren chronologisch zu **256 realen originalen EMU8000-Stimmenstarts** mit tatsächlich gelesenem IP und IFATN. Linearer unabhängiger Gasttick-/Chipframe-Plausibilitätsfit: 44,093231679 Frames/ms, **max. Residue 0,519 ms**, RMS 0,289 ms. Kein solches Residuum ist eine zulässige Toleranz für spätere Registerparität.
- Separat aufgezeichnete Originalchip-WAV: **6.839.420 Stereo-Frames / 13.678.840 PCM16-Samples**, 44.100 Hz, signed 16-bit, 2.816.345 strikt von null verschiedene Werte, davon 2.489.552 mit absolutem Samplewert über 4; Spitzenwert abs 6.893. Echte hörbare Abschnitte bei ungefähr Sekunde 81–114.
- Getrennt gerenderte native EMU8000-Adapter-WAV: **13.678.840/13.678.840 PCM16-Samples bitidentisch**. Beide WAV SHA256 f31e0598b5d4399abd582b03316b8f449885e4ca547fd6a94032bcd8f216853f.
- **Negativkontrolle an echtem Musiksignal:** Original-WAV nur in privater Kopie bei **Frame 3.903.536, Kanal 1** von PCM16 **−259 auf −260** (ein Bit) verändert. Separater Chip-Replay meldete exakt diesen ersten Fehler bei Frame/Kanal und beendete mit **Exitcode 11**. Die Original-WAV blieb unverändert.

## Alle 254 individuellen Originaltreiber-Messwerte

Der vollständige numerische, nicht urheberrechtlich geschützte Messwertauszug umfasst für **jede** Velocity zwischen 1 und 127 und beide GM-Programme: MIDI-NoteOn-Zeitstempel, physische EMU8000-Stimme, tatsächlichen Originalchip-Frame, Pitchregister IP, Filter-/Dämpfungsregister IFATN (inklusive extrahierter High-/Lowbytes). Die unveränderte **private** CSV-Datei heißt measured-gm0-gm40-velocity-registers.csv; SHA256 b9b30b9b583c8befbf43c2f8b0f3b725f9cbf6eeca71e120a091aa8cd094d8d2. Für die Kontrollauswertung ist zusätzlich ein privates gemessenes JSON gespeichert. Keinerlei ROM-, Treiber- oder WAV-Inhalt wird in den öffentlichen Git-Branch kopiert.

| Originalregister/Messung (gleiche MIDI-Note 60) | GM-Programm 0 | GM-Programm 40 |
|---|---:|---:|
| Messpunkte je Programm | 127 | 127 |
| Konstantes Pitchregister IP während Sweep | 0xC962 | 0xBC66 |
| IFATN: Attenuation bei Velocity 1 | 173 | 165 |
| IFATN: Attenuation bei Velocity 127 | 42 | 34 |
| Anzahl unterschiedlicher originaler Attenuation-Werte | 50 | 50 |
| Cutoff bei Velocity 1 | 178 (0xB2) | 116 (0x74) |
| Cutoff bei Velocity 70 | 178 | 116 |
| Cutoff bei Velocity 71 | 178 | 117 |
| Cutoff bei Velocity 100 | 178 | 165 |
| Cutoff bei Velocity 127 | 178 | 209 |
| Anzahl unterschiedlicher Cutoff-Werte | 1 | 58 |

**Zusätzliche 127-Punkte-Beobachtung:** Für jede der 127 Velocity-Stufen war die originale Dämpfung von Programm 0 **exakt acht Einheiten höher** als bei Programm 40. In beiden Programmen nimmt die Dämpfung monoton mit höherer Velocity ab und zeigt dieselben 50 Abstufungen. Das ist ein starker Hinweis auf eine gemeinsame Velocity-Dämpfungsumrechnung **plus instrumentabhängigen Basisparameter**, aber keine isoliert bewiesene allgemeine Treiberformel: weitere Programme, Attenuation-Zonen und Bankdaten müssen sie unabhängig bestätigen.

**Entscheidender Filterbefund:** Programm 0 hielt bei allen 127 Anschlägen originalen Cutoff 178 konstant. Programm 40 blieb dagegen nur bis Velocity 70 bei 116; dann stieg die Filterfrequenzsteuerung in 57 weiteren Stufen bis 209. Es wäre **falsch**, eine globale Velocity→Cutoff-Regel oder einen pauschalen Filter-Gain-Patch aus diesen beiden Programmen abzuleiten. Originale Programmdaten, Filtergeneratoren, Velocity-Modulatoren und die tatsächliche Creative-Treiberumrechnung müssen getrennt untersucht werden.

## Konsequenzen für die Implementierung

1. Den ursprünglichen EMU8000-Chip unverändert lassen. Keine individuelle manuelle Presetangleichung.
2. Als nächste P002-Aufgabe eine **eigenständige**, nach Originalunterlagen abgeleitete MIDI-/Bank-/Voice-/Controller→EMU8000-Registererzeugung aufbauen. Original-MIDI bytes, Bankzustand und absoluter Chipframe müssen unabhängig der echten Win95-Spur gegenübergestellt werden.
3. Die Originalinstrumentparameter für Velocity-Attenuation-Offset und Velocity-Filterreaktion müssen aus den Original-Creative-Bank-/Treiberregeln kommen, **nicht aus einem aus zwei Presets geratenen Sonderfall**.
4. Bestehende Windows-CI und das **125A Plugin Test Lab P73/P74/P75** weiter bei jeder Implementation nutzen. P003 betrifft vollständigen SYNTHGM.SBK/SF2-Bank-/Sample-Upload. Die echte Studio-One-/ASIO-Abnahme kommt **erst bei der finalen GUI-/Produktabnahme**.
5. Die tatsächlichen privaten Referenzmedien, die originale Win95-Gastfestplatte und die Originaltreiber bleiben außerhalb GitHub. GitHub-Status ist P002 weiterhin **DRAFT / IN PROGRESS**.

**Geltungsbereich:** Der unabhängige digitale Chip-PCM-Vergleich mit einer realen, vom Original-Creative-Treiber erzeugten Registerspur ist vollständig bestanden. Eine korrekte eigenständig vom nativen P002-Code generierte Registerspur ist **nicht** nachgewiesen; die digitale Gleichheit beweist keinen vollständigen Creative-Treiber, kein ganzes Presetset und keine physische Analogkarten-Gleichheit.
