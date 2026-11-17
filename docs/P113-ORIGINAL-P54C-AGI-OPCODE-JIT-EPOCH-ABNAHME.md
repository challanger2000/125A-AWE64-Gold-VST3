# P113 – Originale P54C-AGI-Einzelergebnisse mit JIT-Blockgeneration verknüpfen

**17.11.2026 – Forschungsstand in Draft-PR #8. Original-AWE64-Gold-VST3, Creative-Treiber, ROM, SF2 und EMU8000-DSP unverändert.**

## Konkrete Fehlerquelle, die P113 beseitigt

P110 protokolliert beim Übersetzen originaler x86-Befehle im Pentium-P54C-86Box-Code die **ursprünglich von `check_agi()` gelieferten** AGI-Ergebnisse und die gebuchten Compiler-Timingwerte. P111 prüft an der ORIGINAL-86Box-Dynarec-Stelle, ob eine bestimmte übersetzte Codeblockgeneration später tatsächlich den `code()`-JIT-Lauf durchlaufen hat. P112 hat außerdem die **mitgeführte Registermaske am Codeblockbeginn** durch vollständigen Reihenfolgenabgleich generationssicher gemacht.

Die bis jetzt **offene Lücke** bestand darin, dass ein einzelner P110-`O`-Datensatz (`Gast-PC, Opcode, AGI-Abfragen, positive AGI-Abfragen`) keine P111-Epochen-ID enthielt. Bei einer wiederholten Übersetzung derselben Gastadresse konnte eine positive AGI-Prüfung fälschlich zur *anderen* Codeblockgeneration gehören. Eine positive AGI-Prüfung eines nur übersetzten, niemals später JIT-ausgeführten Blocks durfte nicht als Ursache eines ausgeführten Blocktimings interpretiert werden.

## Exakte instrumentierte Originalstellen

`tools/p113_tag_original_p110_opcode_agi_with_p111_compile_epoch.py` verlangt eine **bereits P110/P111-instrumentierte Wegwerfkopie** der ursprünglichen 86Box-Dateien. Das exakte ursprüngliche `codegen_timing_pentium.c` muss weiterhin den SHA-gepinnten Originalblob `aa78ee1c96076eef581ce4f1b5a86ae6c47d82a4` aufweisen. Das Original wird nicht verändert.

Es erweitert **nur die existierenden Forschungsbeobachter**:

1. `src/cpu/codegen_timing_pentium.c`: Ein schreibfreier Getter liest die zwei bereits von **Original-P110** gezählten Ergebnisse `p110_translator_agi_calls` und `p110_translator_agi_positive`; keine neue AGI-Formel, kein neuer AGI-Register- oder CPU-Zeitwert.
2. `src/codegen/codegen_x86-64.c`: Nach dem **unveränderten Original** `codegen_timing_opcode(...)`, aber **vor** dem existierenden P110-`opcode_after()`, werden die gerade berechneten Ergebnisse zusätzlich in einen privaten P113-Trace geschrieben.
3. Dabei verwendet P113 die von P111 an demselben ursprünglichen JIT-Blockanfang zugewiesene echte `compile_epoch` aus dem **aktuellen** `block->pnt`. Der Blockslot und die ursprüngliche physische Adresse müssen exakt zur gespeicherten P111-Generation passen. Fehlende oder widersprüchliche Ursprungs-Gültigkeit erzeugt einen expliziten **NO-GO-Marker** im privaten Trace.
4. Die P113-`E`-Zeile enthält nur `originale Übersetzungs-Epoche, originaler Gast-PC, Original-Opcode, ursprüngliche AGI-Abfragen, ursprüngliche positive AGI-Abfragen` sowie eine fortlaufende Sequenznummer. Der alte P110-`S/O`-Trace und der P111-`C/F/R/D`-Trace bleiben unverändert.

**Standardmäßig AUS:** Nur bei gesetzten und überprüften `AWE64_P113_ORIGINAL_AGI_EPOCH_TRACE`, `AWE64_P113_LCOD_BASE` und `AWE64_P113_LCOD_SELECTOR`, zusammen mit aktivierten originalen P110-/P111-Beobachtern, werden Datensätze geschrieben. Der Bereich bleibt das ursprüngliche, SHA-bekannte LCOD-Fenster `+0x4023..+0x4125` und CS `0x28`. Nach einer überschrittenen Sequenzgrenze oder fehlender P111-Epoche wird der Trace als unvollständig gekennzeichnet und **nicht** abgenommen.

## Unabhängiger P113-Auditor

`tools/p113_audit_original_opcode_agi_compilation_epoch_private.py` verlangt **drei vollständig validierte private Originalspuren** aus demselben Messlauf:

- **P110:** `S/O` der tatsächlichen ursprünglichen P54C-Codeübersetzung.
- **P111:** `C/F/R/D` mit eindeutiger Original-Blockgeneration und einem echten ausgeführten Original-`code()`-Aufruf.
- **P113:** `E` der echten Original-`check_agi()`-Abfragen pro übersetztem Opcode und der zugehörigen P111-Generationskennung.

Der Parser verwirft geänderte oder doppelte Epochen, nichtlückenlose Ereignisse, abgeschnittene Daten, falsche Gast-PCs/Opcodes, unmögliche AGI-Zähler, Unstimmigkeiten gegenüber den vollständigen P110-`O`-Datensätzen und P111-Generationen. Zusätzlich wird **jeder P110-`S`-Blockstart mit dem jeweiligen P111-`C`-Start in vollständiger gleicher Reihenfolge** verglichen. Die P113-`E`-Epoche muss dann zu genau *dieser* P110-`S`-Übersetzungsinstanz gehören. Ein bloßer Adressgleichstand reicht nicht. Das prüft auch bewusst mehrfach übersetzte identische Gastadressen.

Redigierte Ausgabe: Anzahl und Summe tatsächlich positiver ursprünglicher AGI-Prüfungen, getrennt nach Opcode-Übersetzung in einer **später tatsächlich JIT-ausgeführten Blockgeneration** oder einer **nur kompilierten, nie später JIT-ausgeführten Blockgeneration**. Ein Lauf mit **null positiven** ursprünglichen AGI-Prüfungen ist ein zulässiges und wissenschaftlich nützliches Ergebnis – sofern die übrigen echten Originalspuren vollständig und konsistent sind. Ausgabedateien enthalten keine privaten vollständigen Original-x86/Port-Samples, Stackregister, ROM- oder VxD-Daten.

## Durchgeführte Prüfungen / echte Originalgrenze

- Die **unveränderten injizierten C-Beobachter** aus P110, P111 und P113 wurden **nativ unter Windows und Linux kompiliert und ausgeführt**, mit zwei absichtlich **identischen fiktiven Gast-PCs**, zwei verschiedenen Epochen und einem tatsächlich fiktiv ausgeführten JIT-Block. Der echte P113-Auditor bestätigte die Zuordnung, und der absichtlich manipulierte `E`-Datensatz mit falscher Epoche wurde erkannt. [CI #44009504274](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44009504274): **2/2 PASS**.
- Die neue Instrumentierung wird außerdem **nach** P107/P108/P110/P111 nur in der Wegwerfkopie des originalen alten 86Box-x64-Forks unter Linux/Windows eingebaut. Der komplette Emulator-Build läuft in [CI #44009603183](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44009603183); dessen Ergebnis ist unabhängig von C-ABI-Tests zu beurteilen.
- `tools/p108_private_win95_original_capture.py` bietet `--require-p113-opcode-agi-epoch`. Diese Option verlangt das gleichzeitige private Mitloggen von P110, P111 und P113 beim **selben** Original-Win95-Gastlauf und ein positives Prüfergebnis für jeden P113-`E`-Datensatz. Ohne Opt-in gibt es keine zusätzliche Datei; vorbestehende Umgebungsvariablen werden entfernt. Beim ausschließlich lesenden Import müssen die Quelldateien zumindest denselben Elternordner aufweisen; das allein ist **kein kryptographischer Sitzungsnachweis**.

**Wichtig:** Selbst eine zukünftig erfolgreich abgeschlossene **echte Original-P113-AGI-Epochenmessung** beweist noch keine originalen P54C-Takte pro ausgeführter x86-Instruktion. Der P110-Zustand stammt aus dem ursprünglichen Übersetzer; P111 beweist nur die anschließende JIT-Blockausführung. Für einen kausalen 91/93/95-Taktbeleg fehlen nach wie vor der direkte Original-Win95-EMU8000-Port-/TSC-Gegentest und eine Abgrenzung von Interpreter-Erstausführungen und übrigen Pipeline-/Speicher-Abhängigkeiten. Ebenso bleibt die autonome bitgenaue AWE64-Gold-VST3-Dry-PCM-Ausgabe separat abzunehmen.

**Original-Win95-P108:** Die letzten 16 bisher nicht erfassten Original-Jcc-Nachfolge-PCs bleiben **UNKNOWN**, bis die private Original-VM wirklich mit der neuen Instrumentierung ausgeführt werden kann. Es gibt **keine** Produkt-VST3-Änderung und **keinen** Research-Merge.
