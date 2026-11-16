# P003 — V5-Allocator-Stack: erneute private Originalaufnahme, unabhängiger Vergleich

Stand: 16.11.2026. **Forschungsdiagnostik, keine autonome CPU-/Audio- oder VST3-Freigabe.**

## Eingabe und Provenienz

Private Owner-Eingabe: `P003-20261116T184835Z-c56deac6-ERGEBNIS.zip`. Das Archiv, seine WAV, die beiden privaten SF2-Bänke, die Original-Port- und CPU-Spuren sowie Creative-Originaldateien bleiben **ausschließlich privat** und wurden **nicht** in GitHub aufgenommen.

- **7/7** SHA-256-Belege im `P003-BERICHT.json` unabhängig gegen die realen ZIP-Mitglieder nachgerechnet.
- Original-Windows-95-Gast: **4.176** MIDI-Testaufrufe, **0** API-Fehler; 86Box-Exitcode 0.
- **1.522.737** physische CPU-/EMU8000-I/O-Ereignisse laut 1:1-Join der Session.
- **68.969** `P003_V5_ROW_STATE` / `P003_V5_CALLER_FRAME`-Paare im privaten Original-CPU-IO-Trace.
- Darunter **25** aus den realen Original-Caller-Adressen rekonstruierte Stereo-Allocator-Scans und **481** tatsächliche `LCOD+0x40C4`-CCCA-Lesungen. **481/481** dieser `CALLER_FRAME`-Datensätze haben `valid=FF`, die jeweiligen `ROW_STATE`-Daten `valid=3F`. Diese 481 sind **nicht** mit den 493 CCCA-Lesungen der vorherigen, getrennten Windows-95-Session zu vermischen.
- Alle 481 lesbaren Allocator-Frames haben denselben zum gemessenen Originalstack passenden Parent-EBP und denselben Ausgangs-ESI; drei früher falsch zugeordnete Helfer-Stackwerte werden **nicht** benutzt.
- **4/481** CCCA-Fälle haben `sample_end <= (CCCA & 0x00FFFFFF)` und erfüllen die originale Sample-Endebedingung; **0/481** haben die durch `[EBX+0x10]&1` aktivierte Paar-Strafwertung.

Die originale VM-Konfiguration aus dem tatsächlich verwendeten privaten Gesamtpaket (`VM/86box.cfg`): `cpu_family=pentium_p54c`, `cpu_speed=133333333`, `cpu_use_dynarec=1`. Diese **133,33 MHz** sind die verifizierte Eingabe *dieser* Originalaufnahme, unabhängig von anderen privaten 86Box-Konfigurationen. `cyc_period=cycs/200` im eingesetzten 86Box-Dynarec entspricht hier ungefähr **666** CPU-Takten je 5-µs-Quantum.

## CCCA → unmittelbar folgendes VTFT: echtes x86-Modell

Die originalen durch SHA und Disassembly geprüften Programmstellen `LCOD+0x40C4..0x4120` bilden im vollständigen Wertungspfad die Zustandsübergänge:

1. Nach CCCA ggf. `current = 0`, wenn das Sample-Ende erreicht ist.
2. `candidate = previous + current + 2*max(previous,current) + (penalty_bit ? 0xC00 : 0)`.
3. `best_replaced = candidate <= best_before`, `previous_less_current = previous < current`.
4. Nur für den erwiesenen vollständigen, unmittelbar folgenden VTFT-Pfad ist die frühere **empirisch auf TSC kalibrierte** Diagnoseformel `93 + 2*best_replaced - 2*previous_less_current` zulässig.

Ergebnis, neu aus den **originalen** vollständigen Allocator-Stackwerten und TSCs dieser Session gerechnet; **nur tatsächlich direkt benachbarte CCCA→VTFT-Paare**:

| Fall | Exakte Zeitprognosen | Abweichungen |
|---|---:|---:|
| Erste 16 Original-Allokationen | **159/223** | **64** |
| Letzte 9 Original-Allokationen | **243/243** | **0** |
| Insgesamt | **402/466** | **64** |

Bei den 64 Abweichungen ist die reale gemessene TSC-Differenz stets größer als die beschränkte Modellprognose, um **1 bis 13** Takte. **47** der 223 frühen direkten Intervalle dauern mehr als 95 Takte. Zahlen aus der vorherigen V5-Session (`392/478`, `86` Restfälle) dürfen nicht für diese aktuelle Aufnahme übernommen werden.

### Konkretes Gegenbeispiel für ein bloßes Jcc-Kostenmodell

In **21** aufeinanderfolgenden vollständigen CCCA→VTFT-Übergängen der Originalstimme 30 (Allocator-Durchlauf 5–25) sind die für den betreffenden x86-Wertungspfad nachgewiesenen Sprungentscheidungen gleich:

- Sample-Ende nicht erreicht, Strafbit 0, Paar nicht null.
- `previous < current`, neuer Kandidat ersetzt den bisherigen Bestwert.
- Loop wird fortgesetzt, `[ESI+0x18] > 2` (gleiche Gating-Sprungrichtung), nächster Marker gehört zum normalen VTFT-Pfad.
- Derselbe originale Parent-EBP, derselbe ESI, dieselbe Voice-Row-Adresse.

**Dennoch** reichen die echten `CCCA→VTFT`-Abstände in dieser exakt definierten Vergleichsserie von **93 bis 106 TSC-Takten**. Der neue Caller-Frame schließt die ursprüngliche Unkenntnis von `previous`, `current` und `best` als **alleinige Ursache** dieser Unterschiede aus. Ein künstlicher Konstantenaufschlag pro Jcc-/Voice-Flag kann diese Beobachtung nicht allgemein erklären. Das beweist noch nicht, *welcher* andere 86Box-CPU-Mechanismus dafür verantwortlich ist.

### TSC/Execution-Budget-Sonderfälle

Bei **462/466** unmittelbar benachbarten Übergängen gilt `delta_tsc == (cycles_before - cycles_after) mod 666`; die vier Ausnahmen liegen **alle in Durchlauf 15** und haben zweimal **+14** sowie zweimal **−14** Takte Unterschied zwischen den beiden Counter-Differenzen. Das mod-666-Verfahren setzt explizit das für diese VM beobachtete 5-µs-Ausführungsbudget voraus.

Das ist ein eigener Hinweis, dass TSC-Zeitstempel im laufenden Dynarec nicht stets trivial mit dem momentanen `cycles`-Restbudget identisch sind. Aus dem Befund folgt **nicht**, dass alle übrigen 64 Fehler durch Counter-Synchronisierung entstehen.

## Quellgrundlage der nächsten Ursachenprüfung

Der exakt verwendete 86Box-Fork `turican0/86Box-AWE32` Release `v6.0-AWE32-0.93` implementiert für den Pentium-Dynarec:

- [Pentium-Pairing, AGI- und Pipeline-Modell](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/cpu/codegen_timing_pentium.c#L1199-L1318): Abhängigkeiten, Instruktionspaarung, Decode-Verzögerungen und Blockabschlüsse wirken auf `codegen_block_cycles`.
- [Dynarec-Quantum und TSC-Abgleich](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/cpu/386_dynarec.c#L828-L952): `cycles`, `tsc`, Zwischen-Timer-Verarbeitung und Blockausführung besitzen eigene Aktualisierungspunkte.
- [Pentium-Timing-Modell-Auswahl](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/cpu/cpu.c#L1428-L1435).

**Arbeitshypothese, kein Beweis:** Dynamic-Recompiler-Blockgrenzen, Instruktionspaarung oder Timer-/TSC-Abgleich sind Kandidaten für die Abweichungen trotz identischer beobachteter Original-Verzweigungen. Zur kausalen Isolation benötigt man die Zuordnung der **tatsächlich ausgeführten JIT-/Interpreter-Blöcke und ihrer CPU-Taktkosten**; die Voice-Row- und Allocator-Stackwerte allein reichen jetzt nachweislich nicht mehr.

## Konsequenz

**Keine** Änderung an produktivem P005, keine weitere empirische Zeitkonstante, kein vorzeitiger Merge. Die neue Owner-Aufnahme beweist die Korrektheit des V5-Caller-Frame-Mitschnitts und grenzt das Timingproblem deutlich enger ein. Sie beweist weder vollständige CPU-Zeitrekonstruktion noch Win95↔VST3-PCM-Parität.
