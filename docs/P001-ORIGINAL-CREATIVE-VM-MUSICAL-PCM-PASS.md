# P001 – unabhängiger echter Creative-MIDI-/EMU8000-Klangnachweis

## Erneuter Abschlusslauf mit korrigiertem Pentium-P54C-Programm – vollständig PASS

Die erste unabhängige Aufnahme oben war bereits musikalisch bitidentisch,
aber das Win95-Testprogramm stürzte *nach den gesendeten Noten* ab.
Die Ursache wurde in der tatsächlichen ausführbaren Win95-PE32-Datei
disassembliert: Compilerdefault i686 enthielt **CMOVNE (0F 45 C2) bei
Adresse 0x004014B7**, obwohl die VM einen **Pentium P54C bei 133 MHz**
ohne CMOV-Unterstützung emuliert. Die VM meldete die ungültige Instruktion
bei **0x004014B9**. Das ist ein CPU-Instruktionssatzfehler unseres
Win95-Testtools, **kein Fehler des originalen Creative-Treibers und
keine EMU8000-Klangabweichung**.

Im GitHub-CI wurde der Compiler auf Pentium-kompatible
**-march=pentium -mtune=pentium** umgestellt, mit einer
Disassembly-Prüfung, die CMOV und andere Post-P5-Befehle hart verwirft.
[Win95-PE32 Build #43084099814](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43084099814)
hat diese Prüfung bestanden. SHA256 des korrigierten, selbstgeschriebenen
P001MID.EXE:
**7f39d847fdc3fbbe351a539fee1cf0a5fbbab059bc25ed3d391c55f387f1d63f**.

Die unabhängige Original-VM wurde mit diesem korrigierten Win95-Testprogramm
**erneut** aus einer frischen privaten Festplattenkopie gestartet. Diesmal
stehen alle zehn erfolgreichen NoteOn/Off-Meldungen **und**
der finale **P001_GUEST_WINMM_MESSAGES_SENT_OK**-Marker im tatsächlichen
Win95-Gastlog. Das Programm verschwand regulär vom Windows-95-Desktop;
es erschien **keine** Absturzmeldung. Der finale Windows-Exitcode
wurde nicht separat vom Gastprozess abgefragt.

### Neue, gültige Musikkapazität – nicht die ältere Absturzaufnahme

| Echte Original-VM-Messung | Neuer Wert |
|---|---:|
| Original-ROM SHA256 | 4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e |
| EMU8000-RAM | 28 MiB = 28.672 KiB |
| Original-Creative-WinMM-Befehle | 5 NoteOn / 5 NoteOff, alle Returncodes 0 |
| Win95-Testprogramm | **SENT_OK, auf Desktop zurückgekehrt, keine Fehlerdialoge** |
| Eigene Original-EMU8000-Portereignisse | **93.445** (49.282 Writes + 44.163 Reads) |
| Portwrites während hörbarer Musik | 530 |
| Stereo PCM16 / 44.100 Hz | **4.486.440 Frames** |
| Original/native PCM16-Samples | **8.972.880 / 8.972.880 bitidentisch** |
| Nicht stumme Originalsamples | **398.765** |
| Sample-Spitzenwert | 6.893 |
| Hörbarer Bereich | Sekunde 66–70 |
| Gültige negative Ein-Bit-Mutante im Musiksignal | Frame 2.933.838, Kanal 1; Replay **Exit 11** |
| Original EMU8K_TRACE SHA256 | 2699947ffbf1cbab47be996b11fd05c20fca45424572db698e08ca9d6cce4ddc |
| Original AWE32_WAV SHA256 | 3ee11e9aa19138890532b3cdd228d0f07298c89211173f06643c46fe5e1270d8 |
| Native Adapter-WAV SHA256 | **3ee11e9aa19138890532b3cdd228d0f07298c89211173f06643c46fe5e1270d8** |
| Gastprotokoll SHA256 | e2585e81c9d3e22c1b5d6e8f31c3215cf2283a02d60d0603e8e66ee25ab493ec |

Die exakte Wiedergabe des **neu** aufgezeichneten Treibersignals:

    P001 fork-vs-native PCM16 IDENTICAL: 4486440 stereo frames, 8972880 individual samples, 93445 register events

Der im Repository hinzugefügte strenge originale Win95-/Creative-Oracle
verlangt ab jetzt auch den tatsächlichen Abschlussmarker SENT_OK.
Eine unvollständige oder abgestürzte Probe darf nicht als **vollständiger
Gastprozess-Referenzlauf** gelten. Der frühere Capture bleibt als
historischer technischer Nachweis erhalten, wird aber durch
diesen sauberen musikalischen zweiten Lauf abgelöst.


**11. November 2026 – private echte 86Box-AWE32-/Windows-95-VM, musikalischer Vergleich: PASS.**

Der frühere reine Boot-Vergleich war fast stumm. Diese separate
Aufnahme enthält nachweisliche echte MIDI-Noten des originalen
Creative-Win95-Treibers. Die Gastfestplatte und sämtliche Ton-/ROM-/
Treiberbytes bleiben privat. Dieses Dokument veröffentlicht
ausschließlich numerische Prüfergebnisse und Hashes.

## Referenzbedingungen

- Hardwarekalibrierter, unveränderter Fork: turican0/86Box-AWE32,
  Commit fee1e81977824fd2d5eed4d239589dc30d251dd7;
  EMU8000-Original-C-Blob e60eb10f00a8e1187380faa25989f6ba0433d99a.
- Private Wegwerfkopie von Windows 95 (512-MiB-VM-Datenträger);
  emulierte AWE64 Gold mit 28 MiB / 28672 KiB EMU8000-RAM.
- Echte AWE32-ROM-Identität SHA-256:
  4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e.
- Private Win95-Logdatei C:\P001MID.LOG bestätigt erfolgreiches
  midiOutOpen auf **SB AWE32 MIDI Synth [620]**, Program Change 0,
  CC7/CC11 sowie fünf MIDI-NoteOn-/NoteOff-Paare für MIDI-Pitches
  48, 60, 64, 67, 72. **Alle zehn** MIDI-Nachrichten meldeten
  WinMM-Rückgabecode 0.

Wichtig: Das Win95-MIDI-Probeprogramm verursachte erst nach den
protokollierten fünf Notenpaaren eine Windows-95-Meldung über eine
ungültige Operation. Sein Abschlussmarker wurde nicht geschrieben.
**Kein sauberer Gastprogrammabschluss wird behauptet.**
Die originalen Treiber-Registerereignisse und die echte digitale
Chipausgabe sind dennoch aufgezeichnet und separat verglichen.

## Echter musikalischer digitaler Referenzvergleich

| Metrik | Original VM / Adapter |
|---|---:|
| Echte Registerereignisse | **93.445**, davon 49.282 Writes + 44.163 Reads |
| Stereo PCM16 Frames / 44.100 Hz | **6.430.760** |
| PCM16-Samples tatsächlich verglichen | **12.861.520 / 12.861.520 identisch** |
| Erste abweichende Probe | **Keine** |
| Tatsächlich nicht stumme Originalsamples | **398.777** |
| Maximaler Absolutwert des Signals | 6.893 |
| Klangabschnitt nach echten MIDI-Kommandos | etwa Sekunde 66–71 |
| Original fork.wav SHA-256 | d976f51d30759f2feffab3ca97025d33c57e8ab5e210e6f0f9e32edfb10237f3 |
| Adapter native.wav SHA-256 | **d976f51d30759f2feffab3ca97025d33c57e8ab5e210e6f0f9e32edfb10237f3** |
| Original fork.trace SHA-256 | 9b6442f5ba348eca9f698b1d8ec8736a43c052258229d9b5dd95ef0699bc8e49 |

Das getrennte native Replay-Ausführungsprotokoll lautet:

    P001 fork-vs-native PCM16 IDENTICAL: 6430760 stereo frames, 12861520 individual samples, 93445 register events

### Negativkontrolle mitten im musikalischen Originalsignal

Eine private *Kopie* der echten Original-WAV wurde absichtlich
bei **Frame 2.930.600, Kanal 0** (nicht stumm) um exakt ein Bit verändert,
von sample 27 auf 26. Derselbe native Replay gab **Exitcode 11**
und den exakten ersten Fehler zurück:

    PCM PARITY FAIL: first differing frame 2930600 channel 0 fork 26 native 27 events 92979

Die ursprüngliche Vergleichs-WAV blieb unverändert. Damit ist
auch ausgeschlossen, dass der Test lediglich auf Dateivorhandensein
oder Nullsamples PASS meldete.

Zusätzlich hat das automatische Tool für den vollständigen echten
privaten Capture denselben Positivfall und eine separat automatisch
gewählte nicht stumme Samplemutante (Frame 2.934.741) bestanden.
Ein ehrlicher JSON-Bericht verbleibt im privaten Prüfverzeichnis.

## Reproduzierbare Abnahme

Das nun im Repository enthaltene Python-Skript
tools/p001_verify_independent_win95_midi.py führt die
Prüfung erneut durch. Es verlangt die identifizierte
Creative-Originaltreiber-Geräteauswahl, die vollständigen
fünf echten WinMM-Notenpaare, nicht stumme 44,1-kHz-
Chip-WAV, zeitlich gültige Originalregister,
vollständige bitidentische native Wiedergabe, WAV-SHA-Gleichheit
und eine wirksame **musikalische** Ein-Bit-Negativkontrolle.

Aufruf (private ROM-/VM-/WAV-/Logdateien niemals committen):

    python tools/p001_verify_independent_win95_midi.py \
        --rom PRIVATE/awe32.raw \
        --guest-log PRIVATE/P001MID.LOG \
        --trace PRIVATE/fork.trace \
        --fork-wav PRIVATE/fork.wav \
        --replay PRIVATE/awe64_replay \
        --out PRIVATE/NEW-CHECK

Das zuletzt gebaute vollständige Linux-86Box-Originalforkbinary
wurde durch [Actions 43073866147](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43073866147)
nach Pin/Chip-Quellprüfungen erstellt; das Windows-Minimal-VST3,
Steinberg-Validator und DLL-MIDI-Verhalten bleiben durch
[Actions 43080928409](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43080928409)
separat validiert. Der vollständige **unabhängige Original-Win95-
Gastlauf wurde in der privaten Container-Testkopie ausgeführt**,
nicht als CI-VM-Gastlauf ausgegeben. Keine private VM oder
WAV wurde in GitHub Actions hochgeladen.

### Exakte Freigabegrenze

**Erfüllt:** Die digitale Chip-PCM-Parität bei der identischen
beobachteten Originaltreiber-Registerfolge für diese
fünf tatsächlich gespielten GM-Programm-0-Noten.

**Noch nicht erfüllt:** Kein Vergleich sämtlicher GM-/ROM-Presets,
Creative SYNTHGM.SBK oder fremder SF2-Bänke; kein Beweis
physischer AWE64-Gold-Analog-Line-Out-Parität; keine
echtzeit- und speicherallokationsverifizierte Studio-One-/ASIO-
Produktabnahme. Kein vollständiger Produkt-/P002-/P003-PASS.
P001-Issue offen und PR bleibt Draft.
