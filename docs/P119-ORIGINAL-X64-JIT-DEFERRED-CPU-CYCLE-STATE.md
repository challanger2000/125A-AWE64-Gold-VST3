# P119 – Originale, im x64-JIT noch nicht zurückgeschriebene Pentium-Zyklen messen

**17.11.2026 · nur REA-Forschung · Produkt-VST3 und P005 unverändert**

## Entscheidende bisher fehlende Originalquelle

Im originalen `turican0/86Box-AWE32`-Fork `v6.0-AWE32-0.93` werden die x86-Gastzyklen nicht bei jedem übersetzten Befehl direkt in `cpu_state._cycles` geschrieben. Stattdessen akkumuliert `src/codegen/codegen_accumulate_x86-64.c` die Änderung in `static acc_regs[ACCREG_cycles].count`. Erst `codegen_accumulate_flush()` erzeugt später den Maschinenbefehl zum Rückschreiben in den ursprünglichen CPU-Zähler.

Damit gilt: Der an einer tatsächlich betretenen JIT-Stelle sichtbare **Speicher-CPU-Zähler allein** ist nicht zwingend der gesamte bis dorthin bereits vom Originalcodegen berechnete Zeitaufwand. Eine präzise Untersuchung benötigt **beide** Werte – den originalen Laufzeit-Speicherzähler und den noch ausstehenden, kompilierten Delta-Wert.

## Implementierung ohne Änderung der Zeitsteuerung

`tools/p119_capture_original_pending_jit_cycles.py` akzeptiert nur zwei originale, Git-Blob-gepinnte Module:

- `src/codegen/codegen_accumulate_x86-64.c`, SHA `6e88487badb8abb5b73258b2bb2a6cfcd97d65c3`;
- `src/codegen/codegen_x86-64.c`, SHA `673141762345c1195458bd840461302493f7d365`.

In einer **wegwerfbaren Original-86Box-Kopie** wird ein ausschließlich lesender Getter für `acc_regs[ACCREG_cycles].count` ergänzt. In den P118-JIT-Hook wird der Getter **während der Originalübersetzung** an der Stelle aufgerufen, an der die originale Zyklenschätzung für die betreffende Instruktionsstelle bereits akkumuliert, aber noch nicht durch einen erzwungenen Probe-Flush verändert wurde. Der erhaltene signierte Wert wird als **vierter sofortiger Parameter** in die schon vorhandene P107/P118-Maschinencode-Callbackstelle codiert.

Die nativen x64-ABIs sind explizit:

- Windows x64: `R9D` (`41 B9 imm32`).
- Linux SysV AMD64: `ECX` (`B9 imm32`).

Der Original-P107/P118-Callback bleibt genau einmal ausgeführt; der P119-Wrapper zeichnet bei `AWE64_P119_PENDING_JIT_CYCLES_TRACE` zusätzlich die originale P107-Ereignisnummer, P117-Site-ID, den Gast-PC, die originale TSC, den CPU-Speicherzähler, den kompilierten ausstehenden Delta-Wert, optional `acycs` und die JIT-Laufzeitkennung `inrecomp` auf.

**Es gibt keinen Aufruf von `codegen_accumulate_flush()`, `update_tsc()` oder `timer_process()` durch den Beobachter.** Weder Originalzykluszählung noch 86Box-Timer, Creative-VxD, EMU8000-DSP oder SoundFont werden bearbeitet. Das bei `USE_ACYCS` parallel gebuchte `acycs` ist ein eigener Kanal; blindes Addieren aller Werte würde ggf. doppelt zählen und ist ausdrücklich verboten.

## Automatische QA und Originalitätsgrenzen

`tests/test_p119_original_pending_jit_cycles.py`: originale Git-Blob-SHAs, unveränderte Original-Zyklenbuchungen, Fehler bei fehlenden/verdoppelten Source-Ankern und Quellen. `tests/p119_native_pending_jit_callback_abi.cpp`: *echte native Ausführung* des selbstgenerierten x64-Hostmaschinencodes, 1.000 Callback-Aufrufe pro ABI mit vier verschiedenen Parametern einschließlich signierter Delta-Bitmuster.

`tools/p119_validate_original_deferred_jit_cycles_private.py`: fail-closed Auditor verknüpft vollständige private P118- und P119-Laufzeitspuren 1:1 mit dem Original-P107-Site-Token und prüft dabei die P107/P111/P117-Originalprovenienz. Er gibt aggregierte Abweichungs-/Delta-Statistiken und SHA-256 aus, keine Original-PCs.

Die vollständige P003-/P115-CPU-Portmessung und P107–P119 sind über `tools/p117_stage_original_p54c_epoch_probe.py` in **derselben** isolierten 86Box-Kopie kombinierbar. Github-CI prüft Windows-/Linux-Source-Negativtests und echte x64-Host-JIT-Callback-Aufrufe; ein vollständiger ORIGINAL-86Box-OLD-x64-Dynarec-Build belegt zusätzlich die Integration. Nur abgeschlossene CI-Jobs dürfen als PASS bewertet werden.

## Beweisgrenze

P119 kann bei einem künftigen echten Gastlauf erstmals **den bereits zurückgeschriebenen und den noch aufgeschobenen Original-Zyklenanteil für eine eindeutig identifizierte JIT-Opcode-Stelle** nebeneinander messen. Das ist näher an der tatsächlichen Zeitbasis als eine bloße feste Pro-OUT-Schätzung.

Es ist **noch kein abgeschlossener autonomer C++23-Treiber**. Die originale U/V-Pipeline kann Kosten bis zur nächsten Instruktion verzögern; JIT-Probeaufrufe liegen am Eintritt der Stelle, nicht zwingend hinter dem abgeschlossenen Opcode; Interrupts, Interpreter, Verzweigungen, `acycs`-Doppelerfassungen und die echten Audio-Timer müssen noch unter Original-Win95 überprüft werden. Die private Originalgastmessung mit P119 und der unabhängige native DRY-PCM-Vergleich fehlen.

**Keine weiteren Entwickler-Kleinstübergaben vor einem belastbaren Originalgastlauf bzw. einer nachgewiesenen vollständigen CPU-Kostenfolge.**