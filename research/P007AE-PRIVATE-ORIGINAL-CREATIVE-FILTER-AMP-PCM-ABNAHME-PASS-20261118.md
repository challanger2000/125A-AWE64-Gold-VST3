# P007AE — ERSTMALS ORIGINAL CREATIVE NRPN → PCM A/B/A BESTANDEN (OWNER PRIVATE)
  
**Datum:** 18.11.2026 · **Status:** Originalklangnachweis für **Filter Cutoff und AMP Attack** PASS, **nicht** für alle zwölf P165-Regler.

## Prüfumgebung und Authentizität

Vom Eigentümer tatsächlich unter Windows 11 ausgeführter, in die ursprüngliche VST3 geladener Originaltest `P007ActualHostQA.exe <AWE64_Gold_P007.vst3> --private-nrpn`, aus privatem Paket `125A-AWE64-Gold-P007AE-KLANGDIAGNOSE-PRIVATE-20261118.zip`.

- Originaler, gegenüber P007AD unveränderter 86Box/EMU8000-Kern (SHA-256 `87b295d3286f2ad426725d7f342ee2d8594bdd28ba9a1412429af310d3526a10`).
- Private originale Windows-95-HDD (SHA-256 `e7a62dfcd8906e4aefffe0ffea8d93c6fe615f8be26a279b9086b4eb68ae8802`), originaler Creative-AWE-WinMM-Treiber; Testpaket-VST3 SHA-256 `8a9c63dd09561bb602e25004bf310341cb582db4adc8adec071a6fcb713a9249`.
- Kein synthetischer Core oder Ersatzfilter, tatsächliche originalen EMU8000-Audiomesswerte; Benutzerdatei `P007AE-ORIGINAL-CREATIVE-DIAGNOSE-ERGEBNIS.txt` im Chat vorgelegt und überprüft. Die private Windows-95-Festplatte und Quell-Audiodaten wurden **nicht** im öffentlichen Repository gespeichert.
- Erfolgreicher, echter Creative `WINMM_READY` und aufnahmebasierter A/B/A-Audiotest mit insgesamt acht Noten; der Originaltest läuft nach unverändertem Win95-Boot.

## Nachgewiesene Messwerte – Onset-RMS auf je 43.008 Stereo-Frames

| Stellung/Take | RMS | Normierte Rauheit (Ableitung) | Abnahme |
|---|---:|---:|---|
| A1 Original, trocken | 0,004055691 | 0,103344329 | Referenz |
| A2 unveränderte Wiederholung | 0,004029072 | 0,103275407 | stabil (0,7 % RMS-Abweichung) |
| Normal-MIDI-Controller CC7=0 | 0,000000000 | 0 | **komplett digital stumm** |
| CC7=100, normaler Klang | 0,004754673 | 0,114054850 | Wiederherstellung PASS |
| Filter Cutoff = 10 % (Original-Creative-NRPN Nr. 21) | 0,001173822 | 0,044052759 | **RMS 75,3 % weniger gegenüber CC7=100**; Klangstruktur ebenfalls verändert |
| Filter Cutoff = 0 % / SoundFont-neutral | 0,004933498 | 0,117593765 | ursprüngliche Filterreaktion zurück |
| AMP Attack = 65 % (Original-Creative-NRPN Nr. 11) | 0,000342371 | 0,083979621 | **RMS 92,8 % weniger gegenüber CC7=100** im Anschlagsfenster |
| AMP Attack = 0 % / SoundFont-neutral | 0,004776893 | 0,114603296 | ursprüngliche Attackreaktion zurück |

Testabschluss im Eigentümer-Protokoll, **alle fünf Bedingungen = 1**:
```text
P007AE_PRIVATE_ORIGINAL_PCM_DIAGNOSTIC stable_original=1 cc7_mute=1 cc7_restored=1 filter_nrpn=1 amp_attack_nrpn=1 original_win95_creative=1 synthetic=0
P007AD_PRIVATE_REAL_WIN95_CREATIVE_NRPN_ABA_PCM_PASS
P007W_ACTUAL_WINDOWS_VST3_HOST_QA_PASS
```

## Ursacheneingrenzung und Korrektur

P007AD zeigte in demselben originalen Creative-Gast **keinen** merklichen Filter-/AMP-Unterschied, trotz erfolgreicher originaler NoteOn-Wiedergabe. Im damals verwendeten NRPN-Sender wurden vier 6-Byte-Controller-Pakete (24 Bytes) gleichzeitig für einen emulierten 16550-UART mit 16-Byte-Hardware-FIFO eingereiht. `p007_midi_submit` bestätigte nur die **Host-Queue**, nicht den Empfang durch Win95.

P007AE verteilt die NRPN-Befehle als **ein** 6-Byte-Paket pro mindestens 20 ms Audiozeit, zeitlich unabhängig von kleinen Studio-Pro/ASIO-Blockgrößen, mit FIFO-Ablehnungsretry und vollständigem Transaktionsstatus. Anschließend wurden die 8 authentischen PCM-Szenarien bestanden. Dies **stützt** die Überlaufhypothese, beweist aber für sich genommen keinen vollständigen UART-Paketverlust-Trace. Die originale 86Box/EMU8000-Audioengine wurde nicht verändert.

## Offene Punkte — kein Release-PASS

1. **Nur zwei** der zwölf nativen P165-Klangregler sind durch authentischen Original-PCM A/B/A geprüft (Filter Cutoff, AMP Attack). Die restlichen zehn sind zwar VST3/NRPN-seitig verbunden, aber akustisch noch **nicht** bewiesen. Reverb/Chorus-Sends insbesondere müssen 0 % wirklich DRY ergeben.
2. Auswahl der Reverb-/Chorus-**Typen** per originalem Creative SysEx ist noch nicht implementiert.
3. Native SF2/SBK-Ladefunktion in den originalen Win95-Creative-Treiber und korrekte SoundFont-Presetnamen fehlen.
4. Finale Studio Pro/ASIO-Abnahme (Audio, GUI, Wiedereinschalten, Projekt-Recall, mehrere Instanzen) ist nicht erfolgt.
5. Beim Win95-Start trat erneut die 86Box-Diagnose `Illegal instruction 00008B55 (FF)` auf, ohne diesen Klangtest abzubrechen. Stabilitätsursache gesondert prüfen.

**Fazit:** Der Anwender hat einen echten Originalklang-Regler-Durchbruch geliefert. **P007AE private originale A/B/A Filter + Attack = PASS; Produkt-Endfreigabe = NO-GO**. Keine ungetesteten anderen Regler oder SoundFont-Funktionen als fertig ausgeben.
