# P003 – Original-Voice-4-Restabweichung: 7 × +3 Takte, echter Nachbar-Byte-Offset (17.11.2026)

## Quelle und Beweismethode

Ausschließlich die privat aufgenommene, SHA-256-verifizierte Owner-Win95-Sitzung `P003-20261117T010413Z-9c86c813-ERGEBNIS.zip` (SHA-256: `28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c`) sowie `SBAWE.VXD` (SHA-256: `d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`). Die privaten ZIPs, Treiberbytes und Traces wurden nicht nach GitHub übertragen.

Die Analyse beschränkt sich auf jeweils den im Original-Caller verifizierten physischen **CCCA→VTFT**-Übergang der Stimme 4 in den vollständigen Scans 17–25. Die sieben offenen +3-Takt-Abweichungen betreffen genau Scans 17–23; Scans 24–25 zeigen sie nicht.

## Gefundener Recorder-Adressenfehler

Originales `SBAWE.VXD`-Disassembly:

```asm
LCOD+4029: cmp byte ptr [ebx+0Dh], 0
LCOD+403E: cmp byte ptr [ebx+2Dh], 0
```

Die bisherige `P003_V5_CALLER_FRAME`-Instrumentierung las den als `next2d` bezeichneten zweiten Wert **fälschlich an `[EBX+13]` statt `[EBX+45]`** (dezimal). Das las das Byte der aktuellen Voice-Row, nicht das Original-Nachbarstimmen-Byte. Der source-pinned Beobachter ist auf `+45` korrigiert; `tests/test_p003_v5_caller_frame_runtime.py` legt nun **unterschiedliche** Testbytes an beide Adressen und verlangt explizit das originale `+45`-Byte. Der originale EMU8000-Kern und das Win95-Gastsystem bleiben unverändert.

Die älteren `next2d`-Felder sind **nicht gültige direkte Messungen des Original-Offsets +0x2D**. Allerdings enthält der ältere V5-Trace an der **zuvor beobachteten** physischen Stimme 3 den früheren Wert `[EBX_v3+0x0D]`. Weil in allen neun Fullscans `EBX_v3=EBX_v4+32`, ist dies dieselbe **lineare Gastadresse** wie `[EBX_v4+0x2D]`. Die zeitlich vorherige Beobachtung lautet in allen neun Fällen **0**. Dies ist eine adressgleiche vorherige RAM-Beobachtung, kein Beweis einer unmittelbar gleichzeitig lesbaren Flag-Momentaufnahme und darf nicht mit einer neuen direkten Aufnahme verwechselt werden.

## Originalmesswerte – keine neue Taktkonstante

Alle neun vergleichbaren Original-Stimme-4-CCCA-Aufrufe verwenden `mode=[ESI+0x18]=2`, `[EBX-0x0A]`-Marker **548 = 0x0224**, keine aktive Paar-Strafwertung, gültige 32-Byte-Row-Adresse und dieselben bereits berücksichtigten x86-Score-Entscheidungen (Bestwert ersetzt, `previous_score < current_score`).

| Original-Scan | Current score `[EBP-4]` | Previous `[EBP-0x0C]` | Ehem. `next2d` indirekt am korrekten Ort | Echter CCCA→VTFT | Rest gegenüber eng begrenztem Modus-2-Modell |
|---:|---:|---:|---:|---:|---:|
| 17 | 5403 | 5391 | 0 | 110 | +3 |
| 18 | 5442 | 5437 | 0 | 110 | +3 |
| 19 | 5786 | 5726 | 0 | 110 | +3 |
| 20 | 6206 | 6146 | 0 | 110 | +3 |
| 21 | 6662 | 6596 | 0 | 110 | +3 |
| 22 | 7226 | 7056 | 0 | 110 | +3 |
| 23 | 7915 | 7817 | 0 | 110 | +3 |
| 24 | 8582 | 8495 | 0 | 107 | 0 |
| 25 | 9192 | 9105 | 0 | 107 | 0 |

Der Current-Score überschreitet **0x2000 (=8192)** zwischen Scan 23 und 24. Dies korreliert hier exakt mit dem Verschwinden der 3 zusätzlichen Takte, **beweist aber keinen Schwellwert-Sprung**. Wichtiger Original-x86-Gegenbeleg:

```asm
LCOD+4048: movzx eax,word ptr [ebx-0Ah]
LCOD+404C: mov [ebp-10h],eax
LCOD+404F: cmp eax,2000h
LCOD+4054: jae LCOD+4111
```

Der **tatsächlich** bei `LCOD+404F` gegen `0x2000` geprüfte Wert ist der 16-Bit-**Marker** aus `[EBX-0x0A]`, nicht `[EBP-4]` Current-Score. Der Marker bleibt **0x0224** in allen neun Scans, die dortige Sprungrichtung ist somit unverändert. Es wäre ein gravierender Fehler, dem C++23-Entwickler einen vermeintlich originalen `if (score >= 8192)`-Zweig vorzuschreiben.

## Entwicklerkonsequenz und offene Frage

- Beide echten Original-Byte-Offsets `[EBX+0x0D]` und `[EBX+0x2D]` exakt unterscheiden; bei `EBX`-Schritten von −32 bewegt sich die Nachbarreferenz über die Voice-Row-Grenze.
- Kein +3-Fix durch eine willkürliche Score-Schwelle; der original belegbare `cmp 0x2000` betrifft **ein anderes Feld**.
- Nur die **neun** konkret belegten Stimme-4-Fälle sind untersucht; sieben bleiben um +3 gegenüber dem engeren empirischen Modus-2-Timingmodell abweichend.
- Erst der Abgleich der tatsächlich ausgeführten CPU-Blöcke oder ein zusätzlicher originalgetreuer Instruktionsnachweis darf deren kausalen Ursprung festlegen. `P003_V5_CALLER_FRAME` nach Fix wurde unter Win95 **noch nicht** erneut ausgeführt; Quell-/Windows-CI-Test ist getrennt zu bewerten.

**Keine Änderung an P005, kein automatisches Übernehmen dieser Werte als native CPU-Zeitsteuerung.**
