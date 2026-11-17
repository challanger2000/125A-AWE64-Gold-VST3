# P003 – CPU-Timing NoteOn: originaler P54C-Blockzeitpfad in C++23, verifizierte Grenzen

**17.11.2026 · priorisierte Timing-Forschung, kein Entwickler-PR-Kommentar, keine P005-Änderung.**

## Konkretes Ergebnis dieser Arbeitsrunde

Die bisherige Forschung hatte den exakten **CPU-TSC→44,1-kHz-EMU8000-Frame**-Takt bereits aus dem Original-86Box-Quellcode rekonstruiert (Pentium 133 mit Audiozeitbasis `400000000/3` Hz). **4.366.834/4.366.834** physische Original-Registerwrites aus vier SHA-geprüften Win95-Aufnahmen lassen sich bei gleichem Gast-Epochenursprung exakt auf die Originalframes abbilden. Das ist eine Quell-TSC-Projektion und noch kein autonomer Creative-x86-NoteOn-Scheduler.

Die neue Forschungsimplementation `P003OriginalP54cBlockClock` bildet nun die **Original-CPU-Zyklusabrechnung** einer tatsächlich ausgeführten alten 86Box-x64-Pentium-JIT-Blockfolge nach. Relevante originale, unveränderte 86Box-Quellstellen:

- [`src/codegen/codegen_x86-64.c`, etwa Zeilen 987–1005](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/codegen/codegen_x86-64.c#L987-L1005): `codegen_timing_opcode()` berechnet aus originalen Pentium-Instruktionen die auf `ACCREG_cycles` gebuchten CPU-Zyklus-Debits. P110 erfasst die während der **Übersetzung** entstehenden U/V-/AGI-Zustände.
- [`src/cpu/386_dynarec.c`, `update_tsc()` etwa 301–322](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/cpu/386_dynarec.c#L298-L321): Bei einem echten CPU-Zeitsynchronisationspunkt wird nur die Differenz zwischen verbrauchten und **bereits** in `tsc` angerechneten Takten nachgebucht.
- [`src/cpu/386_dynarec.c`, Blockabschluss etwa 936–954](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/cpu/386_dynarec.c#L936-L954): dieselbe Netto-Zyklusabrechnung nach dem originalen ausgeführten JIT-Block, gefolgt von fälligen Timer-Callbacks.
- [`src/io.c`, `outb/outw/outl`](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/io.c): Der EMU8000-Registerzugriff über einen registrierten Port ruft **nicht von sich aus** `update_tsc()` auf; deshalb können mehrere physische OUT-Callbacks denselben ursprünglichen CPU-TSC besitzen, obwohl das `cycles`-Restbudget zwischen ihnen sinkt.
- `P003OriginalWavetableTimer` verwendet die bereits korrigierte **32:32-Quellperiode** aus `src/pit.c`/`sound.c` und verarbeitet nur die Wave-Sample-Callbacks – **nicht** alle Windows-95-/86Box-Geräteinterrupts.

### Tatsächlich implementierte, heapfreie C++23-Quelle

| Neuer Bestandteil | Funktion |
|---|---|
| [`P003OriginalP54cBlockClock.h`](../src/creative_compat/P003OriginalP54cBlockClock.h) | API für originale CPU-Block-Ein-/Austritte, verifizierte CPU-Debitfolge, EMU-Port-Zeitstempel und explizite `update_tsc()`-Punkte |
| [`P003OriginalP54cBlockClock.cpp`](../src/creative_compat/P003OriginalP54cBlockClock.cpp) | Quellabgeleitete Netto-Zeitabrechnung: CPU-Restbudget darf unter null enden, aber bestätigte Zwischensynchronisation darf nicht nochmals verbucht werden |
| [`tests/p003_original_p54c_block_noteon_clock.cpp`](../tests/p003_original_p54c_block_noteon_clock.cpp) | Vollständig ausgeführte C++23-Gegenproben für 152 synthetische NoteOn-Ports, mehrere gleichzeitige Port-TSC-Gruppen, echte Zeit-Synchronisationen und Sampletimer, TSC-Doppelerfassung, ungültige Ports und Unterläufe |
| [Windows-x64-CI](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44053407757) | **PASS:** nativer Windows-UCRT64-C++23-Compiler + tatsächlicher ausführbarer Test |
| [Linux-Gesamt-CI](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44053043856) | **PASS:** vorhandene P003-PIT-/Allocator-/Autonomie-Tests plus neue CPU-Block-Ledger-Prüfung |

Das Modul **nimmt keine absoluten Original-TSC-Zeitstempel als Zeitplantabelle entgegen** und hat auch keine Tonhöhen-/SF2-/Stimmenerkennungsheuristik. Die CPU-Zykluskosten seiner Eingangsschritte sind jedoch **noch extern einzuspeisen**; die Testwerte sind ausdrücklich synthetisch. Deshalb ist die NoteOn-Zeitsteuerung damit **noch nicht** eigenständig oder bitgenau nachgewiesen.

## Wichtige P110/P111-Integrationsgrenze – aus beiden Original-Aufnahmen erneut gemessen

Der separate Forschungszweig `research/p107-original-x64-dynarec-guest-branch-capture` enthält bereits:

- **P110** originale Pentium-Codeblock-Übersetzung mit tatsächlich originalen U/V-/AGI-/`codegen_block_cycles`-Berechnungen; ist ausdrücklich **Compile Time**, nicht Gastausführung.
- **P111/P112/P113** originaler JIT-Compile-Epoch-/Executed-Block-Abgleich und AGI-Opcode-Epoch-Tags. Auch hier ist die vollständige Original-Win95-P110/P111-P113-Aufzeichnung noch nicht als fertige NoteOn-TSC-Eingabe nachgewiesen.
- Die aktuelle Instrumentierung ist auf **LCOD+0x4023 bis +0x4125** fokussiert: Sie umfasst damit nur einen **Stereo-Allocator-Kontrollfluss-Ausschnitt**, noch nicht sämtliche originalen NoteOn-/NoteOff-/Programmierungs- und Windows-Callback-Pfade.

Diesen begrenzten Scope habe ich **gegen beide SHA-authentifizierten Original-Win95-CPU-Portspuren** geprüft, ausschließlich zur Identifikation der Quellbereiche (nicht als Behauptung über den Prozentsatz ausgeführter x86-Instruktionen):

| Authentische Originalaufnahme | Physische CPU-Portcallbacks insgesamt | Rücksprung-PC innerhalb P110-`LCOD+4023..4125` | Rücksprung-PC außerhalb |
|---|---:|---:|---:|
| 16.11. 18:48 | **1.522.737** | **4.675** | 1.518.062 |
| 17.11. 01:04 | **1.529.747** | **4.563** | 1.525.184 |

Die originale Schreib-/Lesestellenverteilung belegt daneben häufige Original-LCOD-Rücksprungstellen außerhalb P110, darunter `+0x3E0C`, `+0x3F13`, `+0x3F52`, `+0x3F83`, `+0x3FCE`, `+0x4F97`, `+0x5280`. Diese Zahlen zeigen **welche Hardware-Callback-Pfade im P110-Gastadressfenster nicht liegen**. Sie sind **kein Prozentwert der Guest-Instruktionsabdeckung**: JIT-generierte Opcodes und Hardware-Callback-Return-PCs sind verschiedene Datentypen; manche Aufrufe betreffen außerdem Bank-/RAM-Upload statt ein einzelnes NoteOn.

Der [private, SHA-gepinnte Scope-Auditor](../tools/p003_audit_p110_noteon_original_scope_private.py) validiert beide Original-ZIPs und zählt ausschließlich Original-Rücksprungstellen; keine Gastspur-Bytes werden publiziert.

## Nächster direkter Integrationsschritt – keine neue Grundsatzforschung

Die getrennten Resultate können dann **zusammengeführt** werden, sobald **ein vollständiger, authentischer Original-NoteOn-Ablauf** die folgenden zusammengehörigen Eingaben enthält:

1. **Originale wirklich ausgeführte Guest-Instruction-/JIT-Blocksequenz** mit ursprünglicher Codegeneration/Epoch und der tatsächlichen Branch-Entscheidung – nicht allein die statische Disassemblierung.
2. **Echte CPU-Debits aus der ursprünglichen Pentium-P54C-Übersetzung für diese ausgeführten Codegenerationen** (P110-U/V-/AGI in Kombination mit P111/P113-JIT-Epoch, einschließlich bei blockinternen Sprüngen tatsächlich verwendeter `codegen_accumulate`-Buchungen). Eine positive P110-AGI-Meldung allein ist **keine** Zeitstempelanweisung.
3. **Originale `update_tsc()`- / Dynarec-Blockabschlüsse** und CPU-/Chip-Timer-Anfangszustände, damit die neue C++23-Blockuhr dieselbe Ereignisreihenfolge autonom durcharbeitet.
4. **Vollständiges NoteOn-Registerprogramm** des getrennt portierten C++23-Creative-Treibers. Der Taktgeber liest keine Original-Registerzeitpunkte als Eingaben; er versieht ausschließlich die eigenständig erzeugten Registerwerte mit seinen **selbst aus CPU-Kosten errechneten** Framepositionen.
5. Zwei **unabhängige Original-Win95-Holdouts** und ein echtes Windows-VST3-Dry-PCM-Gegentest. Ohne diese komplette Ende-zu-Ende-Prüfung bleibt die Produktfreigabe ausdrücklich **NO-GO**.

**Wichtige Architekturentscheidung:** P110/P111/P113 nicht noch einmal nachbauen. Sie liegen bereits im separaten GitHub-Forschungszweig. Das neue P003-Modul liefert den fehlenden **86Box-CPU-Budget→TSC→Wave-Sample-Verbuchungsweg**. Der jetzige P110-Ausschnitt reicht noch nicht für ein vollständig originales NoteOn; die Erweiterung muss die ursprünglichen Codegen-/Ausführungsblöcke einschließlich Release, Register-Setup, NoteOn-Trigger und tatsächlicher Fremd-Callbacks abdecken.

**Keine Änderungen am Entwickler-PR, an P005, an Creative-VXD-/Win95-Dateien oder am EMU8000-Klangkern. Keine neue Einzelübergabe.**
