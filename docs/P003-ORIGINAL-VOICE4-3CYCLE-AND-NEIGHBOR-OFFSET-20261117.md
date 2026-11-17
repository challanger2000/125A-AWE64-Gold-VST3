# P003 – Original-Voice-4: 7 × +3-Takt-Restfälle; korrekter Look-ahead der Nachbarstimmen (17.11.2026)

## **Korrektur der Adressinterpretation**

Die erste Fassung dieser Notiz bezeichnete einen gültigen Look-ahead fälschlich als Recorderfehler. **Dieser Fehler lag in der Analyse und ist zurückgenommen.** Der bereits vor dieser Untersuchung entwickelte V5-Recorder erfasste beide Statusbytes der **nächsten** Stimme korrekt.

Original SHA-geprüfte Creative-SBAWE.VXD (`d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`):

```asm
LCOD+4029: cmp byte ptr [ebx+0Dh],0
LCOD+403E: cmp byte ptr [ebx+2Dh],0
...
LCOD+411A: sub ebx,20h   ; next voice row
```

Der V5-Recorder wird am aktuellen CCCA-Helfer ausgeführt und speichert für die **nächste Schleifeniteration** vorausschauend:
- `next0d` an `[EBX_current-19]` (dezimal) = `[EBX_next+0x0D]`;
- `next2d` an `[EBX_current+13]` (dezimal) = `[EBX_next+0x2D]`.

Denn `EBX_next = EBX_current−32`. Der Vorschlag, `next2d` auf `[EBX_current+45]` umzubauen, wäre **falsch**; er wurde unmittelbar zurückgenommen. Ein neuer kompilierter Negativtest legt verschiedene Werte an `EBX_current+13` und `EBX_current+45` und verlangt ausdrücklich den korrekt vorausschauenden Wert an +13. **Es war keine Reparatur des Original-Recorders erforderlich.**

## Datenquelle und echte Originalbeobachtungen

Privater originaler Owner-Win95-Lauf `P003-20261117T010413Z-9c86c813-ERGEBNIS.zip` (SHA-256 `28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c`). Die private ZIP, Creative-Binärdaten, ROM, SoundFonts und Traces bleiben außerhalb von GitHub.

Alle neun stabilen CCCA→VTFT-Übergänge von physischer Stimme 4 zu Stimme 5 (Original-Scans 17–25) führen exakt um **32 Byte** zur nächsten Voice-Row. Die für Stimme 5 vorausschauend gelesenen beiden Original-Statusbytes `next0d`/`next2d` sind in allen neun Fällen **0/0**. Das Original-Flag `[EBX_voice4+0x2D]` lässt sich zudem unabhängig an der vorherigen Stimme 3 als deren `next2d` derselben Gast-RAM-Adresse identifizieren; auch dort in allen neun Scans **0** (allerdings früher gemessen, nicht gleichzeitig).

| Originalscan | Aktueller Score `[EBP−4]` | Vorheriger Score | Original-Marker `[EBX−0x0A]` | CCCA→VTFT-Takte | Rest zum eng begrenzten Modus-2-Modell |
|---:|---:|---:|---:|---:|---:|
| 17 | 5403 | 5391 | 0x0224 | 110 | +3 |
| 18 | 5442 | 5437 | 0x0224 | 110 | +3 |
| 19 | 5786 | 5726 | 0x0224 | 110 | +3 |
| 20 | 6206 | 6146 | 0x0224 | 110 | +3 |
| 21 | 6662 | 6596 | 0x0224 | 110 | +3 |
| 22 | 7226 | 7056 | 0x0224 | 110 | +3 |
| 23 | 7915 | 7817 | 0x0224 | 110 | +3 |
| 24 | 8582 | 8495 | 0x0224 | 107 | 0 |
| 25 | 9192 | 9105 | 0x0224 | 107 | 0 |

Der aktuelle Score steigt zwischen Scan 23 und 24 über **8192 = 0x2000**, zugleich verschwindet die +3-Differenz. Das ist eine **Korrelation**, kein belegter originaler Score-Schwellwertzweig. Im Original steht:

```asm
LCOD+4048: movzx eax,word ptr [ebx−0Ah]
LCOD+404F: cmp eax,2000h
LCOD+4054: jae LCOD+4111
```

Diese `0x2000`-Prüfung betrifft **den Marker** `[EBX−0x0A]` (hier immer 0x0224), **nicht** den aktuellen Score. Die gezeigte Verzweigung nimmt in allen neun Fällen die gleiche Richtung. Auch die Original-Skip-Flags bleiben gleich und es wird keine zusätzliche Stimme übersprungen.

## Konsequenz für den C++23-Entwickler

1. Die originale 32-Byte-Voice-Row-Schrittweite und beide statusabhängigen Prüfungen `+0x0D` und `+0x2D` korrekt übernehmen. `next0d`/`next2d` der vorhandenen V5-Messung sind dabei **Look-ahead-Werte für den nächsten Schleifendurchgang**, keine falsch adressierten aktuellen Flags.
2. **Keine** neue `if (score >= 8192)`-Regel erfinden; das Original-Disassembly beweist diese Schwellwertprüfung nur für den *Marker*.
3. Die **sieben** verbleibenden +3-Takt-Abweichungen werden durch die bisher nachgewiesenen Score-, Marker-, Skip- und Nachbarstatuszweige nicht erklärt. Ein x86/Dynarec-Blockvergleich kann die noch offene Zeitkostenursache weiter eingrenzen; dies ist kein Anlass für eine zusätzliche willkürliche Taktkonstante.

Die originalen Daten werden nicht verändert; P005/VST3 und der unabhängige REA-Port im anderen Chat bleiben unangetastet.
