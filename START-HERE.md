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

## Verbindliche Hardware- und Bankkonfiguration (P001–P003)

Die Zielkonfiguration ist eine **Creative Sound Blaster AWE64 Gold mit 28 MiB EMU8000-Sample-RAM**, dem vom Nutzer bereitgestellten **Original-ROM** und den tatsächlich geladenen Soundbanken. Diese Komponenten sind Teil der Klangerzeugung, kein späteres GUI-Extra.

- **EMU8000-RAM:** 28 MiB = **29.360.128 Bytes = 28.672 KiB** (bei 16-Bit-Samplewörtern 14.680.064 Wörter). Den Chip mit **28672 KiB** RAM initialisieren. Tatsächliche Allokation, Grenzen und Adressierung müssen nachweislich stimmen; niemals nur im Display „28 MB“ behaupten. ROM und RAM getrennt halten, die korrekten Adressräume/Offsets des Original-Forks verwenden; keine künstliche Größenanpassung der Samplebanks.
- **Original-ROM:** Die vom Nutzer bereitgestellte echte EMU8000-ROM-Datei laden, Herkunft/Format/Größe prüfen und die tatsächliche Chipinitialisierung mit ROM belegen. ROM-Samples müssen auf dem digitalen EMU8000-Ausgang erklingen. Die ROM-Bank bleibt im Browser und MIDI-Programmraum logisch eigenständig.
- **Original-Soundbank:** Unterstütze die tatsächlich verwendete Creative-Soundbank **SYNTHGM.SBK**, sofern als private Nutzerdaten vorhanden; ihr Inhalt ist von dem Chip-ROM zu unterscheiden. Die öffentliche Repository-/Release-Ausgabe darf weder das originale Creative-ROM noch geschützte Creative-Bank-/Treiberdateien enthalten. **Eine erfolgreiche Dateiöffnung oder Presetliste allein genügt nicht.** Bei externen SF2-Dateien, insbesondere `Synth 512 Bank.sf2` (77 echte Presets) und den weiteren vorhandenen Testbanken, muss der echte Sample-Upload in den zugewiesenen RAM, die Generator-/Modulatorübersetzung und die registergesteuerte Wiedergabe nachgewiesen sein.
- **Bankauswahl:** ROM, Original-SBK und zusätzliche SF2s klar trennen; keine künstliche Auffüllung von 77 auf 128 Presets. Bank-/Programmnummern und Program-Change/CC0, Browser, Previous/Next sowie Save/Restore dürfen die Zuordnung nicht verändern.
- **Zustand/Host:** Beim VST3-Laden Konfiguration `ROM geladen / Größe`, `RAM 28 MiB / tatsächlich belegt`, `Soundbank geladen / aktive Bank / Presets` aus den echten Engine-Zuständen ableiten, nicht aus dem GUI-Modell allein. Fehlerfälle (fehlendes ROM, ungültige Bank, RAM voll, Auslesefehler) klar signalisieren und nicht stillschweigend einen anderen Klangpfad nutzen.
- **Abnahmetests:** 28-MiB-Grenzen und RAM-Zugriff am unteren/oberen Ende, ROM-Notenwiedergabe, Sample-Upload + Wiedergabe aus SBK und SF2, Programmwechsel und Zustand nach Speichern/Wiederöffnen. Vergleiche jeweils mit derselben Konfiguration und denselben Eingaben im hardwarekalibrierten Fork mit Original-Creative-Treiber.

**Reihenfolge:** RAM-Konfiguration und ROM-Initialisierung bereits in **P001** technisch umsetzen und prüfen; tatsächliche Creative-SBK-/SF2-Bankladepfade und Preset-Abnahme verbindlich in **P002/P003**. Ohne nachweislich spielende ROM- und Samplebank-Pfade kein vollständiges Produkt.

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

## Aktueller P002-Entwicklerstand (13.11.2026)

Die native Original-Creative-RAM-SF2-NoteOn-Pipeline existiert jetzt
direkt in der exportierten VST3 als opt-in privater Forschungsmodus.
Authentifizierte Win95-Creative-RAM-Samples und originalverifizierte
VXD-Ressourcen werden für selbstständig aus MIDI generierte 61 Register-
OUTs pro Stimme (122 bei den zweilagigen Testnoten) genutzt.
Details, Aufrufbedingungen und offene DRY-PCM-Gates:
[docs/P002-ORIGINAL-RAM-MIDI-SAMPLE-BINDING.md](docs/P002-ORIGINAL-RAM-MIDI-SAMPLE-BINDING.md).
Der native Chip-Adapter erzeugt jetzt zusätzlich bei ausdrücklich
angeforderter privater RAM-Referenzmessung eine **44,1-kHz-framegenaue
16-bit-Port-OUT-Aufzeichnung**, vollständig im Voraus dimensioniert.
Der Audio-Callback führt dabei **keine Datei-/Heap-Operationen** aus.
Schreibweise, SHA-Quellenprüfung, strikter Win95-vs-VST3-Portvergleich,
Original-Dry-WAV-Prüfung und weiterhin offene Akustik-Gates:
[docs/P002-NATIVE-ORIGINAL-RAM-REGISTER-AND-DRY-PCM-GATE.md](docs/P002-NATIVE-ORIGINAL-RAM-REGISTER-AND-DRY-PCM-GATE.md).

Der ursprüngliche EMU8000-Kern ist unverändert. Eine bestandene
Quellcode-/Registerprüfung ist **keine** Freigabe für bitgenaue
Original-Win95-Audioidentität. Diese muss unabhängig im privaten
Test Lab nachgewiesen werden. Keine Studio-One-/ASIO-Prüfung vor
fertiger GUI.

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
