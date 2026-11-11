# START HERE — 125A AWE64 Gold VST3

**Verbindlicher Entwicklungsauftrag, Stand 11.11.2026.**

## Produktziel

Ein natives **Windows-x64-VST3** für Fender Studio Pro / Studio One, das die Klangerzeugung der **Creative Sound Blaster AWE64 Gold** möglichst originalgetreu wiedergibt. Vorhandenes **125A-AWE-GUI** später übernehmen. Keine Mac-/Linux-Pluginversion. **Klang vor Oberfläche und Teststatistik.**

## Einzige verbindliche virtuelle Klangbasis

- Upstream: https://github.com/turican0/86Box-AWE32
- Git-Tag: `v6.0-AWE32-0.93` (annotierter Tag)
- Aufgelöster Commit: `fee1e81977824fd2d5eed4d239589dc30d251dd7`
- Originaler Chipkern: `src/sound/snd_emu8k.c`
- Erwarteter Git-Blob-SHA: `e60eb10f00a8e1187380faa25989f6ba0433d99a`
- Klangvergleich: Original-Creative-Treiber in der vorhandenen Windows-95-/86Box-AWE32-Fork-Installation, `EMU8K_TRACE` und `AWE32_WAV`; zusätzlich vorhandene echte AWE32-/AWE64-Gold-Hardwareaufnahmen.

**Nicht verwenden:** den alten **modifizierten** produktiven `src/engine/snd_emu8k.c`-Kern aus `125A-AWE-Synthesizer`. Dort vorhandener Code ist für Diagnosen und GUI-Migration lesbar, aber **nicht** die Grundlage der neuen Klangerzeugung. Keine neuen Filter-, Pegel-, Sustain-, Chorus-, Reverb- oder Interpolations-„Verbesserungen“.

## Architektur / klare Eigentümerschaft

1. `third_party/86box-awe32/`: unveränderte, versionsgebundene Quellbasis mit Lizenz-/Herkunftsnachweis, nach Möglichkeit als isolierter Upstream-Snapshot. Prüfen, dass die **tatsächlich gebaute** DSP-Quelldatei dem gepinnten Original entspricht.
2. `src/chip_adapter/`: minimaler nativer Adapter für ROM, RAM, Register-I/O, Takt und PCM-Abgriff; keine klangverändernden DSP-Eingriffe; Echtzeit- und Pufferregeln isoliert.
3. `src/creative_compat/`: MIDI/SF2/SBK-/Treiberverhalten **aus echten Original-Creative-Registerfolgen rekonstruiert**; nicht aus dem alten 125A-Mapping kopiert.
4. `src/vst3/`: Steinberg-VST3-Hostadapter, MIDI-Ereignisse, Audio-I/O, Preset-State und Sample-Rate-Anbindung ohne eigene Klangfarbe.
5. `src/gui/`: erst nach den klanglichen Kernabnahmen das bisherige 125A-GUI und dessen Assets kontrolliert übernehmen.

**Wichtige Grenze:** Windows-95-Treiber-Binärdateien laufen nicht nativ in einer Windows-11-VST3-Instanz. Ihre **beobachtete Registeransteuerung** wird funktionsgleich rekonstruiert. Keine originalen Creative-ROMs, Treiber oder geschützte SF2-Bänke in öffentliche Releases oder das Repository hochladen.

## Reihenfolge der Implementierung (nicht überspringen)

### P001 — Identität des Chipkerns

- Repo-Scaffold, Lizenz-/Abhängigkeitsprüfung und tatsächlich kompilierter **unveränderter** Fork-EMU8000.
- Gleiche ROM, Registerwrites, Timing und Chipkonfiguration wie der Fork.
- **Bitidentischer digitaler PCM-Ausgang** auf kontrollierten Referenz-Traces; bei Unterschieden nachweisbar lokalisierte Ursache, kein pauschales Gain-Matching.
- Erst dann ein minimales VST3 zur Ausgabe des Referenzkerns; keine GUI-Arbeit.

### P002 — Creative-Treiberverhalten

- Vorhandene Win95/Creative/86Box-Fork-Installation als Registerorakel verwenden.
- ROM-Bank, Stimmenallokation, Note-on/off, Velocity, Controller, Pitchbend, Hüllkurven, Sampleadressen, Timing und FX-Sends.
- Originalregister gegen native Register **Wert/Reihenfolge/Zeit** vergleichen; digitale PCM separat vergleichen.

### P003 — SF2/SBK

- Sample-Upload und Dämpfung gemäß Originaltreiber; vollständige SoundFont-Generator-/Modulator-Pfade statt Preset-Hacks.
- **P229 übernehmen als Fehlermuster**: 117 reproduzierte Clipping-Fälle / 3.082 Tests, 216 falsche IFATN-Werte / 790; diagnostische Originalwerte reduzieren 117 auf 2 — **keine** damalige produktive Korrektur.
- Alle vier vorhandenen Testbanken einbeziehen, darunter die 77-Preset-Bank; ROM separat, keine künstliche Auffüllung auf 128, identische Zuordnung zwischen Browser/MIDI/Prev/Next/Engine.

### P004 — VST3- und GUI-Abnahme

- Windows-x64-VST3, real-time-sicher, korrektes Host-MIDI-Timing, Host-Sample-Rates, Speichern/Laden, Parameterautomatisierung, keine ungewollte Pegeländerung.
- Tests im bestehenden 125A Plugin Test Lab; akustische Prüfung gegen Fork und physische Hardwaredaten, danach bestehendes GUI einbauen.
- Studio-One-Praxistest erst nach bestandenen technischen und akustischen Gates, keine Verlagerung der Fehlersuche auf den Anwender.

## Test-/Freigaberegel

Jeder relevante Commit erhält einen reproduzierbaren Positiv- und Negativtest plus Angabe, **welche** Akustik- oder Registerfrage geprüft wurde. `CI PASS` bedeutet nur, dass **diese** Tests erfolgreich waren, niemals automatisch Klangtreue. Keine „grünen“ Tests durch ausgelassene, ungültige oder nur auf den Referenzwert reduzierte Messungen. **Original-Digital-PCM unter identischen Chipbedingungen ist zunächst das entscheidende Orakel.** Physischer AWE64-Gold-Line-Out enthält zusätzliche Analog-/Aufnahme-Effekte und wird getrennt bewertet.

## Projektführung

- Neues Repository: `challanger2000/125A-AWE64-Gold-VST3`.
- Alt-Repository: `challanger2000/125A-AWE-Synthesizer` bleibt als eingefrorene Quelle für GUI, Assets und Befunde erhalten; **keine** weiteren Engine-Patches dort als Ersatz für diesen Neustart.
- Nur **ein** aktiver Entwicklerzweig / eine klar benannte Implementierungsaufgabe gleichzeitig; PR vor Zusammenführung, keine unkontrollierten parallelen Eingriffe in dieselbe Audioengine.
- Der Entwickler soll **Code und überprüfbare Klangkorrekturen** liefern; keine Statusberichtsschleifen.
- Der Nutzer muss keine weiteren manuellen Aufnahmen, Tests oder Zwischenschritte ausführen.

## Lizenz/Vertrieb — vor externen Quellen zwingend klären

Der 86Box-AWE32-Fork ist als **GPL-2.0** gekennzeichnet. Bei direkter Übernahme sind Herkunft, vollständige Lizenzhinweise, Weitergabe des korrespondierenden Quellcodes und die Lizenzverträglichkeit aller VST3-Abhängigkeiten **vor Produktvertrieb** zu klären. GPL verbietet Verkauf nicht; proprietäre Verteilung des abgeleiteten Gesamtsystems ist damit nicht einfach frei wählbar. Keine urheberrechtlich geschützten Creative-ROM-/Treiberdateien mitliefern.

**Erster Entwicklerauftrag:** P001 tatsächlich implementieren und Chip-PCM gegen denselben Fork vergleichen. Keine neue Klangmodellierung, keine kosmetischen Arbeiten.
