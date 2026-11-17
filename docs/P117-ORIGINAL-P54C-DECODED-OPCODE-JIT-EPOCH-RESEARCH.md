# P117 – Original-Pentium-P54C-Decoderwort mit tatsächlicher JIT-Übersetzungsepoche verbinden

**17.11.2026 · Original-REA-Forschungszweig · Windows-VST3/Creative/EMU8000 unverändert**

## Wozu P117 dient

P116 hat unter Windows und Linux für **40.960** originale 86Box-Opcode/Präfix/ModR/M-Kombinationen die identische C++23-Timing-/Abhängigkeitstabellenauswahl nachgewiesen. P005 besitzt bereits einen C++23-U/V-/AGI-Rechner. Bislang war aber nicht nachvollziehbar, welche **originalen** Timing- und Registerabhängigkeitswörter bei einer konkreten 86Box-Übersetzung tatsächlich ausgewählt wurden. P110/P113 erfassten AGI und Compile-Epochen, nicht diese beiden 64-Bit-Tabellenwerte plus ModR/M und Präfix.

P117 beseitigt die Lücke auf Ebene der **Originalübersetzung**, nicht durch Schätzung eines festen OUT-Abstands.

## Originalquellen-Instrumentierung

`tools/p117_tag_original_p54c_selected_opcode_epoch.py` verlangt die exakte Git-Blob-SHA des 86Box-Pentium-P54C-Timingmoduls (`aa78ee1c96076eef581ce4f1b5a86ae6c47d82a4`) und vollständige P110/P111/P113-Forschungsinstrumentierungen im **wegwerfbaren** alten x64-86Box-Kopierzweig.

In `src/cpu/codegen_timing_pentium.c` werden **nach** dem unveränderten Original-`switch(last_prefix)` die folgenden, bereits vom ursprünglichen Decoder ausgewählten Werte ausschließlich lesend abgegriffen:

- Eingangsopcode und tatsächlich vorliegende letzte Präfixkennung;
- vollständiges `fetchdat` (ModR/M/SIB-Kontext) und `op_32`;
- Original-Switch-Ergebnis `opcode`, `timings[opcode]`, `deps[opcode]`.

Die Codegen-Kopie holt genau diese Werte **nach** dem Original-`codegen_timing_opcode()` und vor dem existierenden P110-Rücksetzpunkt ab. P113 protokolliert weiterhin unabhängig die bereits original berechneten AGI-Zähler. P117 prüft `block->pnt` gegen die von P111 vergebene eindeutige `compile_epoch` und gegen originalen PC/physische Blockadresse. Die Ausgabedatei ist standardmäßig AUS; nur mit `AWE64_P117_ORIGINAL_SELECTED_TRACE`, `AWE64_P117_LCOD_BASE` und `AWE64_P117_LCOD_SELECTOR=0x28` aktiv. Ein falscher/fehlender Zustand erzeugt NO-GO, keine erfundenen Aufzeichnungen.

`tools/p117_stage_original_p54c_epoch_probe.py` baut P107→P108→P110→P111→P113→P117 ausschließlich in einer neuen 86Box-Quellkopie auf. Die originalen Soundkarten-/EMU8000-Quelldateien werden byteweise auf Unverändertheit geprüft.

`tools/p117_verify_selected_opcode_epoch_private.py` validiert die vollständigen **vier Originalspuren** P110/P111/P113/P117, die Ereignisreihenfolge und das 1:1-Verhältnis jedes P117-Decodereintrags zur P113-Epoche; gemeldete Ergebnisse sind nur redigierte Summen/Hashes. Er unterscheidet „übersetzt, später JIT-Generation ausgeführt“ und „nur übersetzt“. Aus einer ausgeführten **Blockgeneration** wird ausdrücklich **nicht** geschlossen, dass jede enthaltene Opcode-Auswahl wirklich ausgeführt wurde.

## Abnahme

- Originalquellen-SHA, Quellanker, Double-Patch-/Fehlpfad-Negativtests und synthetische Decoder-/Epochenprüfungen.
- Windows-2022-/Linux-Quellprüfung und vollständiger originaler Linux-86Box-OLD-Dynarec-Build mit P117 in der Wegwerfkopie. Nur abgeschlossene CI-Jobs dürfen als PASS bezeichnet werden.
- Private Gastmessung mit echter Win95/Creative-VXD und neuem P117-Trace **steht aus**; sie findet niemals in öffentlicher CI statt.

## Noch offen

Die P116/P117-Nachweise sind wichtig für die präzise Parametrierung des vorhandenen C++23-Rechners, aber noch kein autonomer Original-Treiber. Für die letzten 91/93/95-CPU-Zyklen und die bitgenaue DRY-PCM-Wiedergabe fehlen die vollständige originale **tatsächlich ausgeführte** x86-Instruktionsfolge mit Zweigentscheidungen, die korrekte Instruktionskosten-Zuordnung zur Laufzeit, die externe 86Box-Timer-/Sample-Phase und eine unabhängige Original-/VST3-Holdout-Abnahme. Keine Übergabe an P005 ohne diese Grenzen, kein Studio-One-Vorabtest.

**Originale Creative-VXD-Binaries, ROM, SF2, VM, CPU-Rohspuren und WAVs dürfen nicht nach GitHub.**