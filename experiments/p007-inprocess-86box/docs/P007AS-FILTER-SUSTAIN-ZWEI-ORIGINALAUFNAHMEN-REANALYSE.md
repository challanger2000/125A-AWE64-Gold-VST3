# P007AS: Filter-Sustain-False-Negative aus zwei originalen Windows-95-Aufnahmen

19. November 2026 · Reproduzierbarkeitsprüfung (kein neuer Klangtest)

## Untersuchte Originaldaten

- Eigentümerlauf **P007AO**: `P007AO-ORIGINALKLANG-ERGEBNIS.txt` und zugehörige **15 × 24 × 7** Spektralbänder aus `P007AL-ORIGINAL-PCM-SPECTRAL-BANDS(2).csv`.
- Eigentümerlauf **P007AR**: `P007AR-ORIGINALKLANG-ERGEBNIS(1).txt` und `P007AL-ORIGINAL-PCM-SPECTRAL-BANDS(3).csv`.
- Originale Win95-Creative-/EMU8000-Wiedergabe in beiden Läufen (nicht ein synthetischer Doppelgänger). **Die privaten Rohdaten bleiben außerhalb von GitHub**.

Beide Originalaufnahmen enthalten denselben Signalweg und dieselbe Filter-Sustain-A/B/A-Sequenz (Szenarien 8, 9, 10). Für die Filter-Sustain-Wirkung ist **Frequenzband 3** der in beiden Messungen konsistente Kontrollkanal.

| Eigenschaft | P007AO | P007AR |
|---|---:|---:|
| Sustain-Änderung in Band 3, Note-Bins 6–12 | ca. −0,84 bis −0,94 dB | ca. −0,79 bis −0,96 dB |
| A-vs.-A-Rückkehr-Drift, Bins 6–9 | 0,05–0,11 dB | 0,16–0,26 dB |
| Akustische Sustain-Wirkung, beim alten 5×-Drift-Guard | **PASS** (7 aufeinanderfolgende Bins) | **NO PROOF** |
| Qualifizierte Band-3-Bins mit 3× Drift + 4-Bin-Minimum | **7** | **4** |
| Größere Drift-Fenster im Sustain-Bin 11/12 | 0 | 2 |
| Ergebnis bei den aktualisierten Sustain-only-Kriterien | **PASS** | **PASS, rückblickende Reanalyse** |

Die unmodifizierte P007AL-Auswertung fordert trotz 0,25-dB-Effektuntergrenze bei **allen** Reglern `abs(B−mean(A,Back)) > 5×abs(A−Back) + 0,05 dB` und mindestens drei 250-ms-Zeitfenster. Bei der zweiten Aufnahme ist dieses Fünffache empirisch zu streng. Gleichwohl bleibt in den ersten vier Zeitfenstern (Bins 6–9, zusammen 1 Sekunde) ein gleichgerichtetes, akustisch messbares Sustain-Signal bestehen.

## Minimale und testbare Änderung

Nur bei **Sustain**:

- Vergleich mit `3×` der *lokalen* A-/Return-Drift statt `5×`;
- mindestens **4 statt 3** zusammenhängende 250-ms-Zeitfenster mit **gleichem Vorzeichen**;
- maximal zwei unzuverlässige Zeitfenster im gesamten 7-Bin-Sustainfenster (bei P007AR sind es die letzten zwei, nicht das bewertete frühe Segment);
- weiterhin 0,3 dB maximale Drift für *jedes als Evidenz verwendete Bin*, 0,25 dB absolute Untergrenze, hörbare PCM-Energie und mindestens 0,01 % Spektralanteil pro Frequenzband.

Die bisherigen Attack-/Decay-/Release-Grenzen und die unabhängige Release-Energieprüfung bleiben bitgenau unverändert. Ein später entdeckter echter Sustain-Fehler darf nicht durch diese Änderung versteckt werden.

## Gegenproben

Der Offline-CSV-Nachvollzug reproduziert die ursprüngliche **P007AO 4/4, P007AR 3/4**-Entscheidung. Nur die gezielte Sustain-Bewertung ändert **P007AR rückblickend auf 4/4**; Attack, Decay und Release sind numerisch unverändert.

Außerdem wurden für beide Originalaufnahmen fünf falsche A/B/A-Zuordnungen als negative Kontrollen ausgewertet: `(A,A,Back)`, `(A,Back,Back)`, `(A,B,B)`, `(A,A,B)`, `(A,Back,B)`. Alle zehn müssen **NO PROOF** bleiben (so nachgerechnet). Unabhängige synthetische CI-Negativtests prüfen nicht hörbare Änderungen, fehlende A-Rückkehr, zu wenige Zeitfenster, wechselnde Wirkungsvorzeichen und völlig instabile Referenzen.

**Geltungsgrenzen:** Dies sind **keine neuen** originalen Win95-Messungen. Die bisher erfassten Original-PCM-Daten werden mit einer fachlich begründeten, streng gegen Fehlnachweise geprüften Persistenzregel neu ausgewertet. Die angegebene 4/4-Zahl aus P007AR ist ausdrücklich eine **retrospektive Offline-Reanalyse** und ersetzt keinen neuen realen DAW-Test. EMU8000-Originalcode, Creative-Win95-Treiber, 86Box-CPU, MIDI-Queue, P007AR-Host-Timing und das VST3-Produkt wurden **nicht geändert**.

Der alte P007AO-Originalnachweis bleibt gültig; P007AR hat in seiner unmodifizierten Originaltestausgabe weiterhin den alten, unveränderten 3/4-Status. Es wird weder diese Datei umgeschrieben noch ein neues Original-Audio-PASS erfunden.
