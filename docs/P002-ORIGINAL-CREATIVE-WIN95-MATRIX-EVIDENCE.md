# P002 – tatsächlich aufgezeichnete Original-Creative-Win95-MIDI-Matrix

**Status: Original-Creative-Gastlauf, Registerreferenz und digitaler Chip-Replay PASS. Die eigenständige P002-MIDI-zu-Registerübersetzung ist ausdrücklich NOCH NICHT bestanden.**

Alle Rohmedien, die private Win95-Gastplatte, das originale Creative-ROM, die WAV-Dateien und die rohe Registerspur bleiben in einer privaten Wegwerfkopie außerhalb dieses öffentlichen GitHub-Repositories. Nur Hashes und technische Messwerte stehen hier.

## Original-Referenz, nachgewiesene Provenienz

- Gepinnter kalibrierter 86Box-AWE32-Fork: turican0/86Box-AWE32, Commit fee1e81977824fd2d5eed4d239589dc30d251dd7; unveränderter Chip-Quellblob e60eb10f00a8e1187380faa25989f6ba0433d99a.
- Tatsächlicher Gast: Windows 95, Pentium P54C 133 MHz, AWE64 Gold, 28.672 KiB EMU8000-RAM; privates originales 1-MiB-ROM SHA-256 4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e.
- Testprogramm: selbstverfasstes P002MID.EXE, echtes Pentium-kompatibles 32-Bit PE32, SHA-256 c2b61ceb1962cc7157e6f9388d54d37e9b3d5ca18aa52d601c1ffc9b42a5cc73, Windows Build [Actions #43117961635](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43117961635).
- Original Creative WinMM-Gerät wurde tatsächlich als SB AWE32 MIDI Synth [620] erkannt und geöffnet. Die zweite verfügbare Option SB16 Music Synthesizer [220] wurde **nicht** verwendet.
- Die private Gastausgabe C:\P002MID.LOG bestätigt **62 nummerierte MIDI-Nachrichten, alle Returncode 0**, sieben Szenarien und den Abschlussmarker P002_GUEST_ORIGINAL_WINMM_MATRIX_SENT_OK count=0x0000003E. Protokoll-SHA256 b9b484bff66ac65c46267019f901e9087065aceebb3bfce8a67868e82dcab0dc.

## Der echte digitale Vergleich

| Messung | Ergebnis |
|---|---:|
| Originale WinMM-Nachrichten | **62/62 erfolgreich** |
| Szenarien | **7** |
| Positive MIDI-NoteOn | **13** |
| Beobachtete echte EMU8000-Stimmenstarts (DCYSUSV=0x00FF) | **13/13 zugeordnet** |
| Originalregister Write / Read | **50.137 / 44.308**, zusammen 94.445 |
| Original-WAV | 44,1 kHz, 16 Bit, Stereo, **11.683.560 Frames** |
| Original → nativer EMU8000-Chipadapter | **23.367.120 / 23.367.120 PCM16-Samples exakt identisch** |
| Tatsächlich relevante Samples mit abs(PCM16)>4 | **732.855** |
| Original max. abs PCM16 | **7.842** |
| Musikalisches Signal | ca. Sekunden 128–137 |
| Original WAV SHA256 | 6caffd53416426ebcf7039ee6bcf8b595f771954123e31f9d50c07cefeaeee9c |
| Native WAV SHA256 | **identisch zu Original** |
| Original EMU8K_TRACE SHA256 | 9409cdbe229e38d9191eb21c9fbc3e24b690cc05daa206e59b6cb2340f4fb05f |

Der getrennt ausgeführte, schon in P001 etablierte echte native Adapter gab aus:

    P001 fork-vs-native PCM16 IDENTICAL: 11683560 stereo frames, 23367120 individual samples, 94445 register events

**Negative Kontrolle:** Ein ausschließlich auf einer privaten Original-WAV-Kopie bei einem nicht stummen Sample an **Frame 5.865.300, Kanal 1** verändertes PCM16-Bit (signed Wert 285 → 284) wurde mit Exitcode **11** exakt an diesem Frame/Kanal abgelehnt. Die Original-WAV blieb unverändert.

Wichtig: Ein Chipadapter, der die bereits vom Originaltreiber geschriebenen Register bitgenau nachspielt, ist **keine eigenständige native Creative-MIDI-Treiberübersetzung**. Das obige Ergebnis ist ein starkes unabhängiges Chip-/Originalregister-Orakel für P002, aber keine verfrühte P002-Gesamtfreigabe.

## Echte MIDI-zu-Voice-Zuordnung

Die 13 echten positiven NoteOn-Nachrichten wurden der Reihe nach **13 beobachteten originalen EMU8000-Voice-Starts** zugeordnet. Eine NoteOn-Nachricht mit Velocity 0 erzeugte keinen zusätzlichen Start.

Der einzige für die Plausibilität verwendete Zeitbezug zwischen originalen Win95-GetTickCount-Millisekunden und dem unabhängigen Fork-Sampleclock war:

    chip_frame ≈ 44.09391004 × original_guest_tick_ms + 5623659.5043

Maximale Residualabweichung **0,373 ms**, RMS **0,201 ms** über alle 13 Zuordnungen. Diese Korrelation ist **keine erlaubte Toleranz** beim späteren strengen Registervergleich; dieser verlangt exakte Original-Registerwerte, Reihenfolge und Framezeiten ohne Verschiebung.

### Drei Originalanschläge derselben ROM-Note, GM-Programm 0

| MIDI Velocity | Hardwarevoice | IP | IFATN | Cutoff (hoch) | Attenuation (niedrig) |
|---:|---:|---|---|---:|---:|
| 1 | 0 | 0xC962 | 0xB2AD | 178 | 173 |
| 64 | 3 | 0xC962 | 0xB248 | 178 | 72 |
| 127 | 0 | 0xC962 | 0xB22A | 178 | 42 |

Daraus darf keine künstlich angenäherte allgemeine Velocitykurve oder pro-Preset-Pegelanpassung behauptet werden; es sind **gemessene Originalwerte** für diese Note/dieses ROM-Programm.

### Weitere belegte Originaltreiber-Reaktionen

- **CC7** auf 20: IFATN=0xB285; auf 127: IFATN=0xB22D.
- **CC11** auf 30: IFATN=0xB26C; auf 127: IFATN=0xB22D.
- **Pitchbend** Minimum: IP=0xC6B8; Mitte: IP=0xC962; Maximum: IP=0xCC0C; zurück zur Mitte: IP=0xC962.
- **Sustain:** NoteOff mit gehaltenem CC64 verursachte keinen unmittelbaren Release-Registerwrite, erst Sustain-Off löste den echten Release der gehaltenen Hardwarevoice aus.
- **Mehrstimmigkeit:** Die gleichzeitig gestarteten Stimmen und das Retrigger verwendeten u. a. die physikalischen Stimmen 3,6,9,12; dies sind **keine MIDI-Kanäle**.
- **Drums:** Die echte GM-Kanal-10-Kick-/Snare-Sequenz startete physische EMU8000-Stimmen mit IFATN=0xFF25 / 0xFF2A.
- **CC91/93:** Im unmittelbar folgenden Portfenster der gehaltenen Note wurden keine direkten EMU8000-Registerwrites aufgezeichnet. Das beweist **nicht**, dass der originale Creative-Treiber diese Controller generell ignoriert; die Auswirkungen auf spätere Stimmen/Fx-Zustände müssen separat geprüft werden.

## Wiederholbare P002-Gates und weiterer Entwicklungsauftrag

tools/p002_original_matrix_audit.py prüft privat ROM-Digest, tatsächlichen Win95-Creative-Gerätenamen, 62/62 WinMM-Nachrichten, sieben Szenarien, 13 originale Notenstarts, korreliertes Originalzeitverhalten, nicht stummes Original-PCM, Original-EMU8K_REGISTER-Trace, den bitidentischen eigenen Chip-Renderer und eine absichtlich **hörbare** ein-Bit-Fehlermutation. Fehlende private Daten, falsche Geräte, beschädigte MIDI-Logs und unkorrelierte Noten müssen hart scheitern. Die öffentliche Windows-CI führt nur **synthetische konträre Testdaten** für dieses Prüftool aus; sie enthält keine private VM.

Noch nicht implementiert: die tatsächlich eigenständig erzeugte P002-Registerfolge aus MIDI-Daten (Stimmenallokation, Program-/Bankwahl, Filter, ADSR, Effekte) und ihr eigenständiger Originaltreiber-Registervergleich. Bank-Sample-Upload/SF2 Generatoren bleiben P003. Das bereits vorhandene 125A Plugin Test Lab bleibt während der Entwicklung integriert; Studio One und physisches ASIO erst zur abschließenden GUI-/Produktabnahme.
