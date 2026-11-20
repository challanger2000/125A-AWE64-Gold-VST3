# P007 – verbindliche Architektur und geprüfte Source-Einstiegspunkte

**Ziel:** genau ein `125A AWE64 Gold.vst3`-Bundle, in Studio Pro wie jedes andere VST3 ladbar. Weder sichtbare 86Box, noch externer Emulatorprozess, COM2-Named-Pipe, WAV-Recorder, VM-Einrichtungsstarter oder separate EXE im Windows-Host.

## Belegte Komponenten im Originalfork fee1e81977824fd2d5eed4d239589dc30d251dd7

| Original-Datei | Gefundene Funktion | Konsequenz |
|---|---|---|
| `src/CMakeLists.txt` | `add_executable(86Box...)` und getrennte Objektbibliotheken `cpu`, `snd`, `mem`, `machine`, `disk`, `char`, `plat`, `ui` | Erste P007-Prüfung ändert nur Build-Ziel zu DLL; keine Creative-Formeln neu schreiben |
| `src/86box.c` | `pc_init`, `pc_init_roms`, `pc_init_modules`, `pc_run`, `pc_reset_hard_init` | Originaler Start-/Laufzeitcode bleibt vorhanden, aber muss ohne Qt/SDL-Fenster und ohne DAW-EXE-Pfad-Annahme betrieben werden |
| `src/86box.c:1999ff` | `pc_run` führt `cpu_exec(cpu_s->rspeed/1000)` aus (im Normalmodus) und ruft UI-/Video-/Joystick-Funktionen auf | Nicht direkt aus VST3-`process()` aufrufen; separate emulierte CPU-Laufzeit mit headless Platform-Endpunkten |
| `src/include/86box/timer.h` | Original-Timer sind CPU-TSC-basiert, `timer_process()` muss beim Erreichen von `timer_target` laufen | Audio/Zeitstempel müssen mit Original-TSC und Takt/44100 verknüpft werden, statt OUT-Zeitabstände zu raten |
| `src/sound/sound.c:700ff` | `wavetable_poll` läuft als Original-86Box-Timer; `WTBUFLEN=980`, `WT_FREQ=44100`; original EMU8000-Puffer wird an `givealbuffer_wt` weitergereicht | Interner direkter PCM-Pfad ohne WAV/Dateisystem/NamedPipe. Blocklatenz messen, nicht blind DSP verändern |
| `src/char/char.c` | Ein- und Ausgabegeräte sind als `char_port_t` mit Read-/Write-Callbacks registriert | Später Gast-COM direkt an In-Memory-MIDI-Ring koppeln, Win95-Gasthilfe ruft ORIGINAL Creative WinMM |

## Entscheidungen für eine echte VST3-Engine

1. **P007-A:** Original-C-86Box-Kern (inkl. originaler CPU, RAM, AWE32/EMU8000 und Timer) statisch/dynamisch in denselben Win64-Prozess linkbar. Keine `CreateProcess`-Lösung. Ein reiner `LoadLibrary`-Test ist noch kein Boot.
2. **P007-B:** Eigene headless `plat`-/`ui`-Implementierung, Bildschirm- und Chipset-Emulation bleibt intern aktiv; keine Qt-Fenster, keine SDL-Audiogeräte oder OpenAL-Ausgabe im produktiven Plugin. `pc_init()` darf keinen DAW-Arbeitsordner oder EXE-Pfad als VM-Ressourcen-Pfad voraussetzen.
3. **P007-C:** User-eigene ROM-, Creative-/Win95-Dateien werden nur im PRIVATE Bundle verwendet. Gast-HDD wird niemals ohne Schreibschutzkonzept parallel geöffnet. Persistenter Gastzustand/SoundFont-Laden muss in einer privaten Arbeitskopie oder Copy-on-Write erfolgen.
4. **P007-D:** Originaler Win95-Creative-Treiber bleibt der echte MIDI->EMU8000-Kontroller. Guest-Relay bei Bedarf *nur innerhalb der emulierten Win95-Installation*, kein Host-Prozess, keine OS-Named-Pipe. Host-MIDI zeitstempeln, auf Emulationsthread abgeben, dort innerhalb der PC-Timerreihenfolge einspeisen.
5. **P007-E:** Emulator auf einem eigenen Thread; DAW-Audio-Callback liest nur aus einem festen block- und heapfreien PCM-Ring. RAM-/Disk-/Boot-/WinMM-Code darf auf dem Audio-Thread niemals laufen. `process()` wartet nie auf CPU, Disk, UI oder MIDI.
6. **P007-F:** Nur eine VST3-Bundle-Kopie als installierbares Artefakt. Falls interne DLLs/Ressourcen nötig sind, müssen sie innerhalb des `.vst3`-Ordners stecken. Kein Zusatzinstaller und kein separat gestartetes Programm.
7. **P007-G:** Zunächst genau eine emulierte Maschine/Plugin-Instanz beweisen. Der originale 86Box-Quellcode benutzt Prozess-Globals; echte Mehrfachinstanzen brauchen später einen gesonderten, unabhängig belegten Zustandstrennungsnachweis. Niemals ungeprüfte Mehrfachinstanzen freigeben.

## Abnahmestufen – nicht verwechseln

- **PASS P007-A:** Originalcore ist Windows-DLL und lässt sich vom selben Hostprozess laden. Kein Win95-Klangnachweis.
- **PASS P007-B:** Originales Windows95 bootet *in-process* und Original-AWE-WinMM wird bereit. Keine MIDI/PCM-Freigabe.
- **PASS P007-C:** Realer Studio-Pro-NoteOn wird im Gast von ORIGINAL Creative verarbeitet und kommt als ursprüngliches EMU8000-Stereo-PCM ins VST3 zurück. Keine ASIO- oder Offline-Freigabe.
- **PASS P007-D:** 44.1kHz/64,128,256 Puffer, ASIO, Host-Tempo, Latenz, Stop/Restart, Offline-Rendering, VST3-Projektrecall und 125A Test Lab nachgewiesen.

**NO-GO:** P006-DLL plus gestartetem externen 86Box.exe-/Supervisor-Prozess als vermeintlich fertig deklarieren. Keine Eigen-ADSR/Filter/Chorus/Reverb-Berechnung als Ersatz für den Originalkern. Keine private Win95-ROM-/VXD-Binary ins öffentliche GitHub.
