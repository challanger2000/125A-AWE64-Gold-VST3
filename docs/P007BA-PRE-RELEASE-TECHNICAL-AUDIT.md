# P007BA — gezielte Release-Risikoprüfung, 19.11.2026

**Ausgangslage:** Der echte private P007AZ-Original-Win95-Creative-SF2/ROM-Klangtest ist bestanden. Dieses Dokument erweitert **nicht** seinen nachgewiesenen Umfang.

## Zusätzliche gefundene Fehlerursachen

1. **Dauerautomation blockiert Bankwechsel:** Vor der Korrektur setzte jeder wiederholte VST3-Automationswert für Bank/Programm `force=true` im vierstufigen Bank-Selektor. Bei kontinuierlicher, *unveränderter* Automation konnte CC0/CC32/Program niemals abgeschlossen werden, sodass Noten warteten und Stille entstand. `P007MidiParamDedup.hpp` und `P007Native.cpp` senden einen erneuten Bankwechsel nur bei tatsächlicher Wertänderung. Eine absichtlich neue Projektzustandsladung löst weiterhin einen expliziten Resend aus.

2. **Weitere COM2-Überläufe bei MIDI-Akkorden:** P007AZ hatte die drei Bankauswahl-Nachrichten auf 20-ms-Abstände verteilt, aber mehrere NoteOn-Events desselben Host-Audioblocks konnten weiter die emulierte 16-Byte-16550-Hardwareempfangs-FIFO überfüllen. `P007SerialRxGuard.h` und `tools/p007_patch_upstream.py` nehmen neue Bytes aus dem bereits bounded host UART-Ring **erst, wenn die tatsächliche Original-16550 RX FIFO einen freien Platz hat**. Unveränderte originale 86Box-UART- und EMU8000-Routinen; nur die interne VST3-Datenzuführung wird backpressure-fähig. Der ursprüngliche virtuelle Port- und MIDI-Decoder bleiben unverändert.

**Gegenproben:** Ein synthetischer Guard-Stresstest simuliert 128 echte 6-Byte-Tunnelpakete gegen einen jeweils auf 16 Byte begrenzten 16550-Puffer und absichtlich langsamen Windows-95-Reader, prüft bytegenaue Reihenfolge, 0 Verluste und reproduziert den alten Überlauf als Negativkontrolle. Ein weiterer Test schickt 500 identische Automationspunkte gegen die reale `BankProgramPacer`-State-Machine und verlangt exakt drei MIDI-Auswahlnachrichten und anschließend COMMIT.

## Grenzen

Diese adversarialen Einheiten- und Windows-MSVC-Builds ersetzen **keinen neuen privaten Windows-95-Lauftest mit einer Akkord-/Sustain-Sequenz**. Die praktische Mehrinstanzgrenze (globaler originaler 86Box-Kern), unterschiedliche SF2-/SBK-Bänke, tatsächliche DAW-/ASIO-Praxis und die komplette Firmware-/Nutzer-Bank-Lifecycle-Abnahme bleiben getrennt. Für die 125A-GUI gilt: keine einmalige Win95-`0F FF` Debug-Exception als Plugin-Fehler ausgeben.

**Entscheidung:** Ein dokumentierter, testbarer Stabilitäts-Fix für den Release-Kandidaten; kein allgemeines FINAL und keine Studio-One-Abnahme ohne eigenständige Prüfung.

## Zusätzliche Release-GUI-/State-Korrekturen

3. **Stale SF2-Presetnamen nach Projektwechsel:** Ein korrekter VST3-State-Recall aktualisierte den SF2-Pfad, setzte den zuvor geladenen Presetkatalog aber nicht zurück. Das konnte bei noch offenem Editor die Namen einer früheren SoundFont unter der neu gewählten Bank anzeigen. Ein atomisches Invalidierungsflag markiert jetzt den Restore, und **nur der GUI-Thread** setzt den Browser auf neutrale Original-Creative-MIDI-Programmnummern zurück. Erst die bestätigte tatsächliche SF2-Bank wird wieder mit ihren eigenen Presetnamen angezeigt.

4. **Historische FX-Typ-Dropdowns ohne Original-SysEx:** Reverb- und Chorus-Typ waren im alten P165-Look noch bedienbar, obwohl die neue Original-Creative-Engine weder die ursprünglichen Parameter-IDs registriert noch den betreffenden SysEx-Befehl sendet. Diese beiden Menüs sind jetzt ausdrücklich **nicht bedienbar**, anstatt eine nicht stattfindende Klangbearbeitung vorzutäuschen. Die echten Reverb-/Chorus-**Send**-Regler bleiben unverändert funktionsfähig. Eine spätere FX-Typ-Implementierung bedarf separater originaler Creative-SysEx-Nachweise.

## Ergebnis der Windows-Gesamtprüfung auf dem finalen P007BA-Quellstand

- GitHub Actions `44536590790`, Commit `348fb38ee193605c2fd0be4e2520b6ad54fdd1de`: **SUCCESS**.
- Echter Windows-2022-/MSVC-Build eines native x64-VST3 mit Original-86Box-Kern und eingebautem Original-P165-GUI; **Steinberg Validator 47/47 PASS**.
- `P007BA_ORIGINAL_16550_FIFO_BACKPRESSURE_128_PACKET_LOSSLESS_UNIT_PASS` — mindestens ein voller Hardware-RX-FIFO-Zustand in adversarialer Simulation erreicht, alle ursprünglichen 6-Byte-Pakete verlustfrei weitergereicht. Ein reiner Modelltest, keine Original-Win95-Audioabnahme.
- `P007BA_DUPLICATE_HOST_BANK_PROGRAM_AUTOMATION_NO_STARVATION_UNIT_PASS` — identische Parameterwerte während 500 Audioblock-Runden lassen Bank/Programmcommits abschließen.
- `P007BA_ACTUAL_WINDOWS_VST3_REPEATED_BANK_PROGRAM_AUTOMATION_SETTLES_PASS` — zusätzlich **echtes kompiliertes Windows-VST3**, synthetischer Kern (absichtlich kein privater Windows-95-Gast), sämtliche 100 Audioblock-Runden mit unveränderter Bank-/Programm-Automation; Audio- und NoteOn-/NoteOff-Annahmen behalten ihre positiven und negativen Host-Gates.
- `P007AP_REAL_WINDOWS_VST3_FAILURE_INJECTION_AND_MULTIPLE_ACTIVATION_GUARDS_PASS` — zweiter simultaner Gaststart abgelehnt, ohne erstes Original-86Box-Gast-MIDI zu deaktivieren. Kein Nachweis paralleler voller Gastinstanzen.
- `P007AV_PUBLIC_CANNOT_FAKE_ORIGINAL_WIN95_SF2_PCM_PASS` — ohne private lizenzierte Original-VM keine echte Originalklangbehauptung.

**Offen vor finaler Nutzer-Freigabe:** einmaliger privater Win95-Regressionslauf für den geänderten echten MIDI-Serial-Bridge-Datentransport; erst danach ein abschließender Studio-One-/Fender-Studio-Pro-Praxistest.
