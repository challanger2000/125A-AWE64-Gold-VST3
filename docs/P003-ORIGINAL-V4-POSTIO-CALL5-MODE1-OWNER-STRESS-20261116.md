# P003 – 16.11.2026: 86Box Post-I/O-PC, originale CALL+5 und Stereo-Modus 1

## Quellgrundlage und Grenzen

- Private Besitzeraufnahme `P003-20261116T014751Z-3594ea53-ERGEBNIS.zip`,
  SHA-256 `2b507624a159f4f8ed9c69e22b3a300bbbe8937aecb0d6d3b254ae641ca2f3c6`.
- Unabhängiges Kontrollpaket `125A-P003-Konsolidierte-Original-V4-Kontrolle-20261116(1).zip`,
  SHA-256 `43086ea7a81f5bbfa81bc7888a5063a0f3ef7e6ac4ffb014a23831ff9b6737ab`.
- Beide ZIP-Dateien lokal geöffnet, CRC der neuen Aufnahme geprüft; die private
  VXD/Win95-VM selbst ist nicht Bestandteil dieses öffentlichen Berichts.
  Die ursprüngliche Creative-LCOD-CALL-Stellen-Verifikation stammt aus dem
  SHA-gepinnten Kontrollpaket (SBAWE.VXD `d343f242…`).
  Eine unabhängige erneute SHA-Prüfung der VXD-Binärdatei **in dieser neuen Sitzung**
  kann ohne die private VXD-Datei nicht behauptet werden.

## Unabhängig erneut gezählte Originaldaten

| Prüfung | Befund |
|---|---:|
| 3-Bank-Win95-AWE_SEND_MIDI-Aufrufe | 4.176 / 4.176, 0 API-Fehler |
| Original-CPU- / EMU8000-Portevents | 1.533.305 paarweise geprüft |
| Port / Richtung / bekannte Registerwerte falsch | 0 / 0 / 0 |
| CPU-TSC-Rückläufe | 0 |
| Byte-Reads ohne vollständige 16-Bit-CPU-Wertbeobachtung | 5 |
| Original-I/O-PC durch 12 LCOD-Anker abgedeckt | 1.533.294 |
| Andere aufgezeichnete PC-Events | 11 |
| **Korrekte echte sessionbezogene LCOD-Ladebasis** | **`0xC10151E4`** |

86Box instrumentiert PC **nach** dem I/O-Opcode. Die privaten
LCOD-Instruktionsanker markieren jeweils das letzte IN/OUT-Opcodebyte.
Folglich `base = physical_pc - (anchor_last_byte + 1)`.
Die bisherige Berechnung `physical_pc-anchor_last_byte` lieferte die
falsche Basis `0xC10151E5` und produzierte ein künstliches CALL+4-Problem.
Gespeicherte Original-x86-Rücksprungadressen benötigen **keine** Korrektur.

## Modus-1-/Stereo-Livebefund

| I/O-Zeitpunkt-Stackindiz | Beobachtete Portcallbacks |
|---|---:|
| Rücksprung in originalen Modus-0-Codebereich | 447.040 |
| Rücksprung in originalen Modus-1-Codebereich | **4.867** |
| Rücksprung in originalen Modus-2-Codebereich | 0 |
| Exakte CALL+5-Adresse des originalen Stereo-Paar-Dispatcher-CALL `LCOD+0x5522` | **5.017** |
| Exakte CALL+5-Adresse des Mono-Dispatcher-CALL `LCOD+0x5560` | 106.202 |

Die 5.017 Paar-Rücksprung-Hinweise verteilen sich auf 150 (Stacktiefe 1)
und 4.867 (Stacktiefe 2) Portcallbacks, in **25** zeitlich getrennten
zusammenhängenden Abschnitten. Das sind **keine** 5.017 getrennten
Funktionsaufrufe oder Stereo-Noten.

Die zusätzliche synthetische `Vergleichs-Bank-2.sf2` hat zwei
reziprok verlinkte Sample-Header:
Links Sampletyp 4, `sampleLink=1`; Rechts Sampletyp 2, `sampleLink=0`.
Das Original-Gastlog bestätigt alle drei Bankphasen und API-Erfolg. Es
beweist für sich allein keine bankgenaue musikalische Wiedergabe.

**Beweisschluss:** In der neuen echten CPU-/Chipaufnahme liegen erstmals
authentische Portzeit-Stackhinweise auf genau dem vom unabhängigen
Original-x86-Kontrollpaket statisch verifizierten Stereo-/Modus-1-Caller
und innerhalb des originalen Modus-1-Codebereiches. Noch keine
beobachtungsgenaue Rekonstruktion beider physischer Stimmen samt
Pan/NoteOff/Steal- und Register-TSC-Differenzial; kein autonomer PCM-Beweis.
Modus 2 wurde damit nicht nachgewiesen.

## Quellkorrekturen im bestehenden PR-#4-Entwicklungszweig

1. `p003_verify_original_vxd_live_callers.py`: LCOD-Basis korrekt aus
   `post_IO_PC - (anchor + 1)`; physisches I/O-Anker-Mapping entsprechend.
   Echte CALL+5 werden statt künstlicher CALL+4-Diagnose bestätigt.
2. `p003_audit_original_v4_allocator_modes.py`: Original-CALL+5-Paarzählung,
   korrekte Berichtsfelder und Warnungen.
3. Python-Regressionstests für verschobene I/O-PCs, exakte CALL+5-Returns,
   Stereo-/Mono-CALL-Identitäten und fehlerhafte Grenzfälle.
4. `CreativeVxdAdditionalAllocatorModes.cpp`: Source-ordered Skip-Entscheidung
   für blockierte Stimmen vor der unnötigen Chip-Snapshot-Pflicht in Modus 1/2;
   keine pauschale Vorab-Complete-Pflicht im Research-Dispatcher.
5. C++-Regressionstests für übersprungene, bewusst unvollständige
   Voice-Snapshots und dieselbe Situation im Dispatcher.

**Status:** Die Quelländerungen sind eingecheckt. Eine neue Windows-CI-
Abnahme der aktuellen HEAD war zum Zeitpunkt dieser Dokumentation
nicht bestätigt. Die normalen VST3-Note-On-/Stereo-Pfade bleiben
unverändert. Hardwarekalibrierter EMU8000-Kern unverändert.

**Nächste technische Gate:** echte Paar-Voice-ID-/Registersequenz,
Stereo-Pan/NoteOff/Stealing, CPU-TSC und native autonom erzeugte
Register-/PCM-Abweichungen gegen genau diese Originalquelle prüfen.
**P003 autonomous NO-GO; PR #4 nicht mergen.**
