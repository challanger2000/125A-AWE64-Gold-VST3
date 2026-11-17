# P111 – Originaler 86Box-JIT: Übersetzung ist nicht Ausführung

**Stand 17. November 2026, Forschungszweig PR #8, Windows-VST3-Produkt unverändert.**

## Originale technische Grundlage

Im unveränderten ursprünglichen 86Box-Fork `turican0/86Box-AWE32 v6.0-AWE32-0.93` trennt `src/cpu/386_dynarec.c` zwei Originalabläufe:

- **Tatsächlicher JIT-Block-Aufruf:** Nach dem Finden eines bereits kompilierten Blocks und dessen Statusprüfung wird bei Zeilen 595–616 ein Funktionszeiger `code = &block->data[BLOCK_START]` gebildet und **tatsächlich mit `code()` ausgeführt**. Erst dieses Ereignis ist eine echte spätere JIT-Blockausführung.
- **Originale Erst-Interpretation und Neuübersetzung:** Bei Zeilen 617–718 wird `codegen_block_start_recompile(block)` aufgerufen, während der erste ursprüngliche x86-Opcode-Weg über `codegen_generate_call(...)` und danach `x86_opcodes[...](fetchdat)` im Original-**Interpreter** ausgeführt wird. Diese erste Ausführung ist bereits P107-I-beobachtbar, aber **nicht** eine spätere `code()`-Ausführung.
- **Ursprüngliche P54C-Zeitberechnung** innerhalb `src/cpu/codegen_timing_pentium.c` sowie `codegen_timing_opcode` in `src/codegen/codegen_x86-64.c`: Die 91/93/95-Abstände kann man nicht einfach aus der Summe der **übersetzten** Blockzyklen ableiten, solange nicht nachgewiesen ist, welche Codeblockgenerationen wirklich laufen.

Der neue Forschungsstand P110 misst die P54C-Übersetzungszeiten und AGI-Abhängigkeiten, hatte aber keine sichere eigene Verbindung zwischen einem übersetzten Block und dessen späterer Ausführung. **Das ist die konkret geschlossene technische Lücke von P111.**

## Implementierte Originalquellen-Instrumentierung

`tools/p111_join_original_p54c_jit_compile_execute.py` verlangt eine **SHA-gepinnte Originaldatei** `src/cpu/386_dynarec.c` (Git-Blob `e9765a423b5d675337aab33fd060204e62dc20d7`) und den bereits vollständig P107/P108/P110-instrumentierten alten x64-Codegenerator; die vom Original unterschiedenen JIT- und Interpreter-Pfade sind weiterhin beide vorhanden.

Nur an einer **disponiblen Kopie der originalen 86Box-Quellen** werden vier Ereignisse beobachtet:

| Marker | Originaler Ereignisort | Bedeutung |
|---|---|---|
| `C` | `codegen_block_start_recompile(block)` | Originaler JIT-Block beginnt seine Übersetzung |
| `F` | nach `codegen_timing_block_end()` und vor der ursprünglichen Zyklenbuchung | Die Original-Übersetzung wurde abgeschlossen, mit originaler gebuchter Blockzyklensumme |
| `R` | unmittelbar vor der echten originalen `code()`-Anweisung | Bereits kompilierte Blockgeneration wird **tatsächlich ausgeführt** |
| `D` | unmittelbar nach Rückkehr aus `code()` | Dieselbe Blockgeneration hat ausgeführt; originaler **Blockgrenzen**-TSC wird erfasst |

**Generationssicherheit:** Der originale `block->pnt`-Slot ist **kein unveränderlicher Identifikator**. 86Box kann einen Slot löschen oder neu verwenden. P111 weist deshalb jeder Übersetzung eine eigene monoton steigende `compile_epoch`-ID zu und hält die Original-Blockidentität als Kombination von `generation, block->pnt, block->pc, block->phys` fest. Erst nach einem echten Abschluss `F` darf ein späterer Originalaufruf `code()` ein `R/D` derselben Generation schreiben. Eine Blockgeneration mit `C/F`, aber ohne `R/D`, wird ausdrücklich als **übersetzt, aber nicht ausgeführt** gezählt. Alte Slots oder veränderte physische Adressen werden nicht fälschlich übernommen.

**Standard AUS:** `AWE64_P111_JIT_BLOCK_JOIN_TRACE`, `AWE64_P111_LCOD_BASE`, `AWE64_P111_LCOD_SELECTOR` sind alle erforderlich. Nur Original-LCOD-nahe Codeblockanfänge (Bereich relativ zur verifizierten Basis `+0x3F00..+0x4200`, Selector `0x28`) werden erfasst. Fehlt eine Blockübersetzung außerhalb dieses Bereichs, gilt **keine Vollständigkeitsgarantie** für alle P107-Gast-x86-Instruktionen.

## Tatsächliche Prüfung statt nur Quellcodeerstellung

`tests/test_p111_original_p54c_jit_block_join.py` testet echte Quellanker und Negativmanipulationen; `tests/test_p111_actual_c_compiled_block_callbacks.py` übersetzt den **unveränderten P111-C-Instrumentierungscode** unter Windows und Linux und **führt ihn mit ausschließlich künstlichen Codeblöcken** aus. Damit wird ein korrekter `C/F/R/D`-Originalformat-Trace geschrieben, einschließlich niemals ausgeführter Blockgeneration und Wiederverwendung eines Slots mit neuer physischer Adresse. Der echte, separat entwickelte `tools/p111_audit_original_p54c_jit_block_join_private.py` muss die Datei einlesen und Manipulationen (Sequenzlücken, fehlende Übersetzung, vorzeitige Ausführung, fehlende Rückkehr, falsch wiederverwendete Generation, TSC-Regress, Ereigniscap, falscher SHA) zurückweisen.

[Windows/Linux-P111-C-Laufzeit-Gegenprüfungen #44007046414](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44007046414) **2/2 PASS**. [Einbindung in den echten ursprünglichen Linux-/Windows-86Box-OLD-Dynarec #44007310897](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44007310897): Ergebnis siehe aktuelles CI, nicht aus der Quellenprüfung ableiten.

## Private echte Originalabnahme

`tools/p108_private_win95_original_capture.py` nimmt über `--require-p111-jit-join` optional die neue **fünfte** Originalspur im selben privaten Windows-95-Test auf, zusätzlich zu den P107-Instruktionen, P108-direkten CF/ZF-Jcc, P003-physischen EMU8000-I/O und optional P110-AGI-Übersetzungen. Wird sie angefordert und fehlt oder ist inkonsistent, ist das **NO-GO**. Der originale Creative-Treiber, ROM, SF2, Win95-Disk, reale IO-/x86-Spuren und alle Gastregisterdaten bleiben privat.

**Wichtig:** Die Quelle von P111 unterscheidet nur erfolgreich übersetzte und tatsächlich später ausgeführte **JIT-Blöcke**. Die erste Opcode-Ausführung durch den Original-Interpreter gehört zum separat weiterhin aktiven P107-I-Beobachter und darf nicht als P111-`code()` gezählt werden. Der `R..D`-TSC-Abstand ist nur der **Blockgrenzenwert** des ursprünglichen Emulators, nicht eine Zeitmessung für jede x86-Instruktion.

Eine neue tatsächliche Original-Win95-Aufnahme ist unverzichtbar. **Zusätzlich ist bei mehrfacher Übersetzung derselben Gastadresse noch eine explizite P111-Generationskennung für die einzelnen P110-S/O-AGI-Ereignisse nötig**: Die jetzigen P110-Ereignisse enthalten Gast-PC, aber keine P111-Generation. Deshalb darf P111 vor dieser Zuordnung nicht behaupten, eine bestimmte positive P110-AGI-Prüfung sei exakt in jener später ausgeführten Blockgeneration aufgetreten. Die jetzige P111-Spur belegt zunächst nur, **welche übersetzten Blockgenerationen später tatsächlich als JIT-Code aufgerufen wurden**. Erst der generationssichere P110→P111-Abgleich in einer echten Original-Win95-Aufnahme kann prüfen, ob diese Kosten Teil der ursprünglichen 91/93/95-Abweichungen sind. Bis dahin bleiben die 16 alten P107-Jcc-Next-PC-Lücken sowie die autonome bitgenaue VST3-PCM-Parität **offen**. Keine Produkt-DSP-Änderung, kein Merge der Research-Hooks, kein früher Studio-One-Test.
