# P112 – P54C-Registermasken tatsächlich ausgeführten Original-JIT-Blockgenerationen zuordnen

**Stand: 17. November 2026 · REA-Research, nicht VST3-Produkt oder Original-Win95-Testfreigabe.**

## Warum P111 allein noch nicht reicht

P110 zeichnet den originalen 86Box-Pentium-Zustand **während der Codeblockübersetzung** auf, darunter die vor Beginn des Blocks mitgeführte `regmask_modified`-Registermaske und pro übersetztem Gast-Opcode die realen `check_agi()`-Ergebnisse. P111 kann inzwischen nachweisen, welche Generation eines ursprünglichen 86Box-JIT-Blocks später tatsächlich in `code()` ausgeführt wurde. Allerdings enthalten die einzelnen P110-`S/O`-Datensätze bisher **keine explizite P111-Epochen-ID**; die bloße Gastadresse reicht bei Neugenerierungen derselben Adresse nicht aus.

P112 führt deshalb einen **eng begrenzten, unabhängigen und fail-closed Compiler-Start-Abgleich** ein, ohne Änderungen an EMU8000, P54C-Quellcode, Original-Win95, ROM oder Produkt-VST3.

## Was tatsächlich eindeutig verglichen wird

In der originalen 86Box-Dynarec-Aufrufreihenfolge wird in genau derselben Funktion `codegen_block_start_recompile(block)` zunächst das opt-in P111-`C`-Ereignis geschrieben und danach (vor dem ursprünglichen `codegen_timing_block_start()`) das opt-in P110-`S`-Ereignis. Damit haben beide **Compilation-Start-Marker** dieselbe Gast-PC-Adresse und die eindeutige Reihenfolge der alten 86Box-Übersetzung; P111 führt zusätzlich die eigene generierte Epoch-ID.

`tools/p112_join_original_p110_register_mask_to_executed_p111_block.py`:

1. Prüft **zuerst beide vollständigen privaten Originalspuren** unabhängig über die Originalformat-Auditoren P110 und P111. Syntaktisch defekte, unterbrochene oder vom Ereignislimit abgeschnittene Spuren sind NO-GO.
2. Begrenzt die Verknüpfung auf den **gemeinsamen Original-LCOD-Startbereich `+0x4023..+0x4125`** bei der SHA-verifizierten bekannten Gastbasis `0xC10151E4`. Ein P111-Block außerhalb dieser Startmenge wird **nicht** stillschweigend einem P110-Block zugeordnet.
3. Fordert **vollständige gleiche Anzahl UND identische Gast-PC-Reihenfolge** der gefilterten P111-`C`-Marker und aller P110-`S`-Marker. Damit können selbst **wiederholte Kompilierungen desselben Gast-PCs** in der jeweiligen ursprünglichen Reihenfolge korrekt einer P111-Epoch-ID zugeordnet werden; eine nur anhand des PCs durchgeführte Zusammenführung ist verboten.
4. Prüft über die P111-`R/D`-Ereignisse, ob jede verknüpfte Kompiliergeneration später wirklich als ursprünglicher JIT-Code lief oder lediglich übersetzt und nie JIT-ausgeführt wurde.
5. Gibt **ausschließlich redigierte Zähler/SHA-Werte** aus: übersetzte und später tatsächlich ausgeführte Blockgenerationen mit **nichtnull/Null** ursprünglicher mitgeführter Registermaske sowie übersetzte, aber nie JIT-ausgeführte Generationen mit denselben Eigenschaften.

**Wichtige Grenzen:** Dieser Join beweist **eine konkrete am Blockstart vorhandene originale `regmask_modified`-Maske für eine später JIT-ausgeführte Blockgeneration**. Er beweist **noch nicht**, dass eine konkrete **positive `check_agi()`-Instruktionsprüfung** aus P110-O genau zu jener P111-Generation gehörte, da die P110-O-Einzelereignisse keine Blockepoch enthalten. Schon gar nicht beweist er, dass ein AGI-Stall einen der 91/93/95-CPU-TSC-Abstände verursacht hat. Der ursprüngliche **Interpreter-Erstlauf** bleibt separat über P107 erfasst und gehört nicht in P111-`code()`-Zähler.

## Öffentliche Sicherheits- und Integrationsprüfungen

- P112 umfasst synthetische Gegenproben mit mehrfach gleicher Gast-PC-Adresse, einmal später ausgeführter und einmal nie ausgeführter Generation, Maskenwert 0 und nichtnull. Falsche Reihenfolge, unterschiedliche Anzahl und unvollständige Originalspuren werden zurückgewiesen. [Windows-/Linux-CI #44008131486](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44008131486): **2/2 PASS**.
- Im privaten `tools/p108_private_win95_original_capture.py` ist der neue Forschungsabgleich **nur auf ausdrückliche Option** `--require-p112-block-mask-join` aktiv. Diese erfordert gleichzeitig `--require-p110-translator` und `--require-p111-jit-join` aus **derselben** frischen Original-Windows-95-Sitzung. In der ausschließlich lesenden Auditoption werden gleiche Quellordner geprüft; ein Quellordner allein ersetzt allerdings keinen kryptographischen Sitzungsbeweis. Fehlt ein benötigter Originalbeleg oder gelingt die strikte geordnete Zuordnung nicht, ist die P112-Forschungsabnahme **NO-GO**. Andere P107/P108/P110-Abnahmen bleiben davon getrennt.
- Der originalinstrumentierte 86Box-Fork mit P111 ist **unter Windows x64 und Linux x64 tatsächlich gebaut**: [Actions #44007310897 – 3/3 PASS](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44007310897). Das ist ein Emulator-Quell-/Buildbeleg, **kein neuer Original-Windows-95-Lauf**.

## Nächste echte Originalbeweis-Stufe

Die noch fehlenden Daten sind **keine neuen öffentlichen CI-Tests**, sondern ein echter, privater Start der SHA-gepinnten ursprünglichen Windows-95-/Creative-VM mit dem P107–P112-instrumentierten alten 86Box-x64-Dynarec. Nur dort können die 16 bisherigen unbekannten P107-Jcc-Ausgänge mittels P108 und die ursprünglichen AGI-/Codeblock-/EMU8000-TSC-Bezüge erfasst werden. Für eine **einzelinstruktionsgenaue AGI-Kausalzuordnung** muss nach P112 anschließend noch eine explizite Epoch-ID in die ursprünglichen P110-`O`-Opcode-Messungen eingefügt und an den wirklich ausgeführten Guest-x86-Pfad angebunden werden.

**Keine Product-DSP-Mutation, kein Merge der Diagnosezweige, keine bitgenaue VST3-PCM-/91/93/95-Timing-Freigabe.**
