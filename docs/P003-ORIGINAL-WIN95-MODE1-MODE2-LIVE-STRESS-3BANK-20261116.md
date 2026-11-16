# P003 – Original-Creative-Modus-1/2-Live-Coverage, Win95-Dreifachbank-Stresstest

Stand: 16. November 2026. Umsetzung auf dem bereits bestehenden
Entwicklungszweig `develop/p002-creative-register-oracle`.

## Konkreter Grund für den neuen Gast-Test

Das echte V4-Win95/86Box-Protokoll
`P003-20261116T003509Z-d6e65f07-ERGEBNIS.zip` (SHA256
`fd72103675d5c53d9517626ad0ff18f59afd505cbc9e80b497ff8d9af7221969`)
hat **816.594/816.594** Original-EMU8000-Portevents und
**816.583** bekannte physische I/O-EIP-Opcodepositionen belegt.
In den EBP-Rücksprunghinweisen waren Modus 0 (`LCOD+0x3EA1`)
mit 16.071 Portcallbacks vertreten, Modus 1 (`+0x3FE8`) und
Modus 2 (`+0x416C`) jeweils nicht.

Ein fehlender Eintrag im unvollständigen Gast-Rücksprungpfad beweist
keine generelle Nichtausführung. Der beobachtete Ein-Byte-Rücksprungversatz
bleibt ungeklärt. Die originalen Modus-1/2-Algorithmen sind deshalb zwar
als separate, synthetisch getestete C++23-Kandidaten vorhanden,
**aber ausdrücklich noch nicht in die normale VST3 aktiviert**.

## Neue Original-Win95-Prüfsequenz

Neue **optionale** `#define P003_ORIGINAL_MODE_STRESS`-Variante des
bestehenden `src/win95/p002_original_creative_ram_bank_capture.c`
inkludiert `src/win95/p003_original_win95_mode_stress.inc`.
Der bisher bewiesene normale `P002RAM.EXE`-Build ohne dieses Makro
bleibt unverändert; die ursprünglichen C4/D4-Gastproben laufen weiter
**vor** dem zusätzlichen Stresstest.

Der Test fordert über die **originale installierte Creative
`AWEMAN32.DLL`/AWEManager-API `AWE_SEND_MIDI=401`** an:

| Eingabe | Test |
|---|---|
| Bank 0 | Originale ROM/GM-Bank, nur Originaltreiberprogrammierung |
| Bank 1 | Die bisherige, SHA-verifizierte private SF2 des Besitzers |
| Bank 2 | Neue **synthetisch generierte** stereo-verknüpfte SF2 mit zwei unterschiedlichen 16-Bit-Zonen, Preset 7 |
| Programme pro Bank | 16 ausgewählte MIDI-Programme |
| MIDI-Kanäle | 0, 2 und 9 |
| Gemeinsame NoteOn-Phase | 36 NoteOns **vor dem ersten NoteOff** pro Programm |
| Original-Creative-MIDI-Aufrufe | **1.392 je Bank, 4.176 insgesamt** |
| Reverb/Chorus-Sends | MIDI-CC91/CC93 jeweils 0, keine FX-Parität daraus abgeleitet |

**Wichtig:** Die SF2-Sampletypen 4/2 (Stereo-Link) sind keine
belegte Interpretation des Creative-Descriptor-Flags `0x04`.
Die originale 1996-Bankverwaltung darf die synthetische zweite
SF2 zurückweisen; das ist dann ein reproduzierbarer **NO-GO**-Befund,
kein Anlass, eine andere Treiberantwort zu erfinden.

Die Original-EMU8000-Stimmenzuweisung, Registerzugriffe und CPU
Scheduler verbleiben **ausschließlich** im echten Win95-Creative-Treiber.
Der eigene VST3-Allocator wird im Gast überhaupt nicht ausgeführt.

## Getrennte und vollständige Beweise

- **Unverändertes Referenzprotokoll:** `C:\P002RAM.LOG` wird nicht um
  neue dynamische Schlüsselnamen verunreinigt.
- **Neues Gastprotokoll:** `C:\P003MODE.LOG` protokolliert die exakt
  erwartete Creative-API-Übermittlung. Der Host fügt es als
  `Original-Stimmenmodus-Stress.LOG` in die **private Ergebnis-ZIP** ein.
- `tools/p003_verify_original_win95_mode_stress_log.py` verweigert
  unvollständige, umsortierte, duplizierte, veränderte oder nicht
  erfolgreiche 4.176 Original-MIDI-Aufrufe. **API-Erfolg ist ausdrücklich
  kein Modus-1/2- oder PCM-Identitätsnachweis.**
- Der bisherige V4-Recorder schreibt alle originalen
  `EMU8K_TRACE`-Events und zugehörige CPU-TSC/CS:EIP/EBP/
  Rücksprungadreß-Hinweise. Der Host verlangt weiterhin die exakte
  physische **1:1-Portsequenz**, sonst Ergebnis **NO-GO**.
- Alle echten Besitzer-VM-, `SBAWE.VXD`-, ROM- und SoundFont-
  Originaldaten bleiben im unveränderten **privaten** ZIP und dem
  daraus extrahierten **wegwerfbaren** VM-Klon. Nichts davon
  wird auf GitHub veröffentlicht.

## CI und Windows-Auslieferung

- [Original Creative Win95-Stressgast i586 kompiliert und Negativtests
  bestanden](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43787795937):
  fertig, **PASS**; keine nicht-Win95-kompatiblen DLL-Imports.
- [Aktualisierter Windows-x64-Ein-Klick-Host, ursprünglicher Gast-Build
  und Tests bestanden](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43787743875):
  **2/2 Jobs PASS**; der Windows-Host kann `--original-mode-stress`
  und bindet den zusätzlichen strikten Log-Auditor ein.
- Das Besitzer-Gesamtpaket wurde **nur lokal** erstellt:
  `125A-P003-ORIGINAL-WIN95-STIMMENMODI-3-BANK-TEST-GESAMTPAKET.zip`
  mit 7 ZIP-Einträgen, CRC und separaten SHA-256-Einträgen geprüft.
  SHA256 des Gesamtarchivs:
  `71d1bce49b96dda00d0029b7364f66db9bd97b6a89ab87bc3eb9f21cee5481a8`.
  Unveränderte Original-VM und V4-EMU8000-86Box-Dateien sind
  byteidentisch zum bereits ausgeführten V4-Gesamtpaket.

## Abnahmegrenze und nächste Entwicklung

Ein Eigentümer-Win95-Live-Lauf ist erst nach Start des
`START-P003-STIMMENMODI-ORIGINAL.cmd` vorhanden.
**Build/Negativtests sind nicht gleich echte Win95-Ausführung.**
Nach Eingang der daraus erzeugten privaten `P003-...-ERGEBNIS.zip`:

1. Originale vollständige CPU-/EMU8000-Spur neu SHA-gebunden verbinden.
2. Mode1/Mode2 anhand der verifizierten Original-VXD-Codebereiche
   und tatsächlicher Rücksprung-Hinweise untersuchen.
3. Nur bei echter Ausführung Original-Descriptor-/Stimmenzustände
   und CPU-TSC-Abstände isolieren und gegen das native Modul prüfen.
4. Erst danach den entsprechenden originalbelegten Modus im
   VST3-Registerablauf einschalten und eine **neue unabhängige**
   Audio-/Port-Differentialprüfung durchführen.

**Autonome P003-Bitidentität weiterhin NO-GO.**
**Originaler hardwarekalibrierter EMU8000-Kern unverändert. Kein Merge.**
