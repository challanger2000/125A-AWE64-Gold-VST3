# 125A AWE64 Gold — Windows VST3

**Stand 19.11.2026: P007BA technischer Release-Kandidat; finale Studio-One-Abnahme noch ausstehend.**

Ein einzelnes Windows-x64-VST3-Bundle mit der unveränderten, originalen EMU8000-Engine aus [turican0/86Box-AWE32](https://github.com/turican0/86Box-AWE32), einem intern gestarteten Windows-95-Gast und dessen originalem Creative-AWE64-Treiber. Externe 86Box-Prozesse/Programme sind für den normalen Betrieb nicht vorgesehen. Die proprietären Windows-95-/Creative-/ROM-Daten bleiben ausschließlich in einem privaten Eigentümerpaket und **nicht** in diesem öffentlichen Repository.

## Tatsächlicher Nachweisstand

- **P007AZ Originalklangtest PASS (private Owner-Windows-Prüfung):** Creative-AWEManager lädt die echte User-SF2-Bank 1, der originale EMU8000 spielt sie und kehrt anschließend zur echten ROM-Bank 0 zurück. Getrennte 304/76 Original-Registerereignisse und unterschiedliche auditive PCM-Fingerprints. Der Fehler war ein zu schneller 18-Byte-Block aus MIDI-CC0, CC32 und Program-Change an einer 16-Byte-UART-FIFO; P007AZ sendet diese einzeln mit zeitlichem Abstand. [Originalklang-Abnahme](docs/P007AZ-OWNER-WIN95-CREATIVE-SF2-ROM-E2E-ACCEPTANCE-20261119.md).
- **P007BA Windows-CI PASS:** 47/47 Steinberg-Validator-Tests; echter nativer x64-VST3-/Original-86Box-Build; adversarialer 128-Paket-RX-FIFO-Flow-Control-Test, 500 identische Bank-/Programm-Automationspunkte und ein echter Windows-VST3-Host-Gegentest mit identischen Parametern auf jedem Audioblock; Original-CPU und Original-EMU8000-Quelldateien unverändert. Der geänderte UART-Zuführungspfad benötigt **zusätzlich** einen privaten Original-Win95-Regressionslauf, bevor er als real gegengeprüft gilt. [P007BA Release-Audit](docs/P007BA-PRE-RELEASE-TECHNICAL-AUDIT.md).

## Bedienung und Grenzen

- Originale ROM- und durch Creative geladene Benutzer-SF2-Sounds, DAW-Projekt-Recall, Host-MIDI/NoteOff, 12 originale Creative-NRPN-Steuerungen und Hardware-Master.
- Das geladene SoundFont liefert nur seine **tatsächlich vorhandenen Presets**; keine künstliche Auffüllung von 77 auf 128. Nach einem Projektwechsel wird der alte SF2-Presetkatalog GUI-thread-sicher verworfen.
- Reverb-/Chorus-**Send**-Regler funktionieren. Reverb-/Chorus-**Typauswahl** ist bewusst deaktiviert, da die originale FX-Typ-SysEx-Anbindung nicht implementiert ist; keine scheinwirksame Host-Automation.
- **Ein aktiver originaler 86Box-/Win95-Gast pro DAW-Prozess.** Der Originalkern nutzt globale Zustände. Der zweite gleichzeitige Gaststart wird abgelehnt, ohne die erste Instanz zu beschädigen. Dies ist eine dokumentierte Einschränkung, kein vollwertiger Multi-Instance-Support.
- Original-SBK getrennt von SF2/ROM, reale Akkord-Last mit Win95-Receiver, vollständiger DAW-/ASIO-Praxistest und physischer AWE64-Gold-Line-Out-Vergleich sind **keine** durch die bisherigen Nachweise automatisch abgedeckten Behauptungen.
- Die absichtliche Windows-95-Ausnahme `0F FF` wird als reine 86Box-Debuglogmeldung behandelt; kein Hinweis auf illegale Software oder sichtbare Plugin-GUI-Fehlermeldung.

## Entwicklung / rechtliche Rahmenbedingungen

Projektauftrag: [START-HERE.md](START-HERE.md). Gepinnter 86Box-AWE32-Upstream: `v6.0-AWE32-0.93`, Commit `fee1e81977824fd2d5eed4d239589dc30d251dd7`. Keine Klangänderung an der originalen `src/sound/snd_emu8k.c`.

86Box-AWE32 verwendet GPL-2.0. Für jede Weitergabe müssen Lizenz- und Quellcodepflichten einschließlich Abhängigkeiten erfüllt werden; die private Nutzung legitimiert keine öffentliche Weitergabe von Windows-95-/Creative-/ROM-Dateien. Der historische Synth-Quellcode liegt separat im Repository [125A-AWE-Synthesizer](https://github.com/challanger2000/125A-AWE-Synthesizer).
