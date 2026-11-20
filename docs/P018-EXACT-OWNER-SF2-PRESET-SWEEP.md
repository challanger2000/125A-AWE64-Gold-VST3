# P018 – 77 echte Besitzer-Presets statt 77 angenommene MIDI-Slots

Stand: 20.11.2026. Entwicklungszweig `research/awe32emu-native-isolated-20261120`, kein Eingriff in P007.

## Verifizierter P017-Befund

Der echte Windows-x64-VST3-Hosttest `P017-ERGEBNIS.txt` wurde lokal mit Besitzer-`awe32.raw`, Creative `SYNTHGM.SBK`, `Synth 512 Bank.sf2` und Vintage Dreams erfolgreich beendet (`P017_TEST_EXITCODE=0`).

Privat-PCM-Tests für P014 (sample-offset) und P015 (active Project Recall mit ROM und SF2, v1/v4) haben PASS. ROM, GM-Drums, 16 MIDI-Kanäle, CC7, RPN-Bend-Range, zwei SF2 A/B/A und 44.1/48/96 kHz wurden erfolgreich geprüft. Diese Resultate bleiben gültig.

**Wichtige Fehlerkorrektur des alten 77er-Sweeps:** Der damalige Code iterierte `0..76`, nicht die tatsächlichen SF2 Preset-IDs. Der Besitzer-`phdr` (echte RIFF-SoundFont-Datei, 77 Einträge plus Terminator) belegt **alle MIDI-Programme 0..77 außer 63**. MIDI-Programm 63 ist KEIN Besitzer-Preset, kann als ROM-Fallback hörbar sein. Programm 77 `Gated Screamer` wurde von P017 ausgelassen. Die alte Meldung `P010_WIN64_ORIGINAL_VST3_77_DISTINCT_OWNER_PRESETS_PASS` beweist daher nur 77 verschiedene Testklänge, **nicht** alle 77 tatsächlichen SF2-Presets.

In den alten C3/Velocity100-Messungen treffen drei Programme den negativen int16-PCM-Grenzwert (insgesamt 112 ausgegebene Stereo-Samples):
- Programm 33 `Stacked Mega-Phaser`: 68
- Programm 34 `Twips Ring`: 16
- Programm 54 `Sawtooth Hit`: 28

Der Zähler zeigt einen **Grenzwert**, nicht zwingend eine klangliche Abweichung oder unnatürliches Hard-Clipping. Originale AWE64-Referenzdaten unter identischen Bedingungen fehlen weiterhin; weder SoundFont-Wellenformen noch Chip-Pegel werden pauschal verändert. Der native VST3-Host meldet 980 Samples Latenz bei 44,1 kHz; für spielbare Live-Latenz ist eine gesonderte Untersuchung vorgesehen.

## P018-Codetest

`research/native-awe32emu/p009-vst3/tests/P010NativeHostQA.cpp` wurde erweitert:

1. Nicht-Echtzeit-Parser für RIFF `sfbk` → LIST `pdta` → `phdr` mit strikter Chunk-Längen-/Terminal-/Duplikatprüfung.
2. Auswahl nur real vorhandener Preset-MIDI-Programmnummern aus der Datei; 77 echte IDs, 63 fehlt, 77 ist enthalten.
3. Ein realer VST3-PCM-Render pro tatsächlicher MIDI-Programmnummer; Rückweisung stummer und bei C3 identischer Prüfklänge (konkreter Besitzer-Test, kein allgemeines SF2-Gesetz).
4. Separater ROM-Fallback-Gegentest für die fehlende Nummer 63; **niemals** als Besitzer-Preset mitzählen.
5. Pro Programm mit PCM-Grenzwerten bei Velocity100 zusätzlich Rendering bei Velocity80 und Velocity60 und detaillierte Ausgabe. Originalsignalweg unverändert.

Öffentlicher Windows GitHub Actions Run [44784257563](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44784257563): **Build, Steinberg-Validator und öffentlich ausführbare Tests erfolgreich**. Der eigentliche neue **P018-Privat-PCM-Test ist noch NICHT bestanden**, bevor das lokal unter Windows mit Originaldateien ausgestattete neue Testpaket erfolgreich ausgeführt wurde.

## Noch offene QA-Grenzen

Die nachgewiesene PCM-Gleichheit beim Projekt-Recall gilt für die ausgeführten Noten, Kanäle und Controller, nicht für jeden denkbaren Klang, jede Velocity, Polyphonie, Controller-Kombination oder die bitidentische Creative-Win95-Hardware-Referenz. Projekt-Recall der konkreten eingesetzten SoundFont-Datei (Dateiidentität/embedded data) ist nicht automatisch nachgewiesen. P007 und Studio-Pro-Endabnahme bleiben getrennt.


## P018 owner Windows acceptance — actual result

The owner supplied the complete `P018-ERGEBNIS.txt` after executing the
private test package on Windows. Result: `P018_TEST_EXITCODE=0`, followed by
`P018_USER_SF2_WIEDERHERGESTELLT`. The original sounds remain private.

**Passed, ACTUAL private Windows VST3 audio:**
- `P018_OWNER_SF2_EXACT_PHDR_77_REAL_PROGRAMS missing63=1 includes77=1`.
- Exactly 77 genuine owner SF2 `phdr` program entries (0..62 and 64..77),
  all non-silent with distinct float PCM hashes for C3 velocity 100.
- Last previously omitted MIDI program 77 returned nonzero audio:
  RMS 0.031960521, peak 0.164367676, hash `70112533C9C10325`.
- Program 63, absent from the owner's SF2, produced exactly the same PCM
  hash in bank1 as bank0 ROM: `116C7F2948020325`.
- `P018_77_EXACT_SF2_PRESETS_REAL_VST3_PCM_PASS tested=77 clips=112 affected=3`.
- P014 private ROM/SF2 sample-offset automation and P015 active v4 and legacy v1
  PCM Recall tests for bank0+bank1 again PASS. A/B/A, GM drums, 16 MIDI
  channels, RPN and controller tests remain PASS.

**Fullscale-boundary observations with C3, velocity 100, master=1:**
`P010NativeHostQA::render` counts output `abs(float PCM)>=0.999999`,
not a provenance-coded identification of hardware clipping:
- Program 33, Stacked Mega-Phaser: 68 samples; at velocity 80 and 60: 0.
- Program 34, Twips Ring: 16 samples; at velocity 80 and 60: 0.
- Program 54, Sawtooth Hit: 28 samples; at velocity 80 and 60: 0.
- Total 112 fullscale-boundary stereo samples at velocity 100.

Do **not** silently add normalizing, limiting, or global gain attenuation:
native EMU8000 fixed-point behavior may contain actual fullscale samples.
Compare identical driver/chip register and PCM traces with genuine Win95
Creative reference before deciding whether these samples are emulator errors.

**Next important issues:** 980-sample reported latency at 44.1 kHz
(approximately 22.22 ms) and proportional sample-count at 48/96 kHz,
private authentic Win95 register/PCM parity, bank-file-identity project
recall, full polyphony/velocity coverage, and real Studio Pro final audition.
P018 is completed for the exact tested note/velocity/preset coverage,
not a blanket release approval.
