# P110 – Originale Pentium-P54C-Codeblockübersetzung: U/V, AGI und Zeitbuchungen

**Forschungszweig, 17.11.2026.** P110 ist ein neues, ausdrücklich **privat einzuschaltendes Messmodul** für die Originalquelle `turican0/86Box-AWE32 v6.0-AWE32-0.93`. Das native Plugin, die unveränderte EMU8000-Engine, der Windows-95-Creative-Treiber, ROM und proprietäre Sample-Bänke bleiben unangetastet.

## Warum P107/P108 allein nicht reicht

Die echte P107-Messung vom 16.11.2026 zeigte im Originalgast 36.784 tatsächlich ausgeführte LCOD-Guest-PC-Ereignisse und 3.474 Jcc, davon 3.458 per unmittelbarem Next-PC belegt und 16 wegen Aufnahme-Lücken unbekannt. P108 ist für eine zusätzliche unmittelbare CF/ZF-Messung der fünf Original-Jcc vorbereitet, wurde aber noch **nicht** mit einer neuen privaten Win95-VM ausgeführt.

P107/P108 protokolliert **Gast-Ausführung**. Dagegen berechnet die echte 86Box-P54C-Quelle `src/cpu/codegen_timing_pentium.c` die U/V-Paarung, Decode-Shadowing, AGI-Masken und `codegen_block_cycles` während des **Übersetzens** eines x86-Codeblocks. Die Entscheidung über dessen Übertrag auf das Gast-CPU-Zyklenkonto geschieht anschließend in `src/codegen/codegen_x86-64.c`. Translationsergebnisse darf man deshalb **nicht** als unmittelbar pro ausgeführtem x86-Befehl beobachtete TSC-Differenzen verkaufen.

P109 hat anhand des unveränderten Quellcodes nachgewiesen, dass `codegen_timing_pentium_block_start()` die `regmask_modified`-Maske **nicht** zurücksetzt, während `check_agi()` sie weiterhin verwendet. Dadurch kann die ursprüngliche 86Box-Codeübersetzungsreihenfolge einen alten Registermaskenwert zur nächsten Übersetzung tragen. **Ob dieser Effekt unsere acht Original-Stereo-Intervalle von 91/93/95 Takten kausal erklärt, ist bislang nicht gemessen.**

## Tatsächliche P110-Instrumentierung

`tools/p110_instrument_original_p54c_compile_timing.py` prüft zuerst den **exakten originalen Git-Blob-SHA** von `src/cpu/codegen_timing_pentium.c` (`aa78ee1c96076eef581ce4f1b5a86ae6c47d82a4`) und verlangt eine **bereits durch P107/P108** instrumentierte Wegwerfkopie des originalen alten x64-Codegenerators. Erst danach verändert er **nur diese beiden Dateien in der Kopie**. Die ursprünglichen Soundkarten- und Chipdateien werden ausdrücklich unverändert gegengeprüft.

Das Modul hängt sich unmittelbar an die originalen P54C-Codegen-Funktionsaufrufe:

- `codegen_timing_block_start()`: Vor dem Originalaufruf wird der vorherige `regmask_modified`-Wert zusammen mit `u_pipe_full`, `decode_delay`, `decode_delay_offset` und `codegen_block_cycles` notiert, ohne den Zustand zu ändern.
- `codegen_timing_opcode()`: Vor und nach der Originalroutine werden die tatsächliche LCOD-Gastadresse, x86-Opcode, alte/neue Registermaske, U-Pipe-, Decode- und gebuchte Zyklusstände notiert.
- `check_agi()`: Die **unveränderte ursprüngliche Bitmasken-Operation** wird einmal ausgewertet und ihr Ergebnis ohne zusätzliche Änderung an den 86Box-Zuständen zurückgegeben. Ein separater, privater Zähler unterscheidet die Anzahl der AGI-Abfragen von den positiv festgestellten AGI-Abhängigkeiten.

**Standardmäßig vollständig deaktiviert.** Nur in einer explizit privaten Original-VM-Umgebung mit `AWE64_P110_TIMING_TRANSLATION_TRACE`, `AWE64_P110_LCOD_BASE` und `AWE64_P110_LCOD_SELECTOR` wird ein Rohtrace für **LCOD+0x4023 bis +0x4125, CS 0x28** geschrieben. Die Datei trägt den besonderen Header `P110_ORIGINAL_P54C_TRANSLATION_TIMING_V1 COMPILE_TIME_NOT_GUEST_EXECUTION`. `S` markiert die ursprüngliche Übersetzung eines Blockanfangs, `O` ein während der Übersetzung verarbeitetes Opcode-Timing; **keine** dieser Zeilen ist eine neue ausgeführte x86-Instruktion.

## Sichere Auswertung

`tools/p110_audit_original_p54c_translation_private.py` akzeptiert ausschließlich die private Original-Trace-Datei im bekannten Format. Der Prüfer verwirft unvollständige, doppelte, übersprungene oder überzählige Sequenzen, fremde Gast-PCs, falsche bekannte Original-Jcc-Opcodes, widersprüchliche AGI-Zähler, manipulierte Hashes und die Ereignisobergrenze. Ausgabe sind nur der SHA-256-Hash und die **aggregierten** Zahlen der Übersetzungen, blockübergreifend vorhandenen Masken, tatsächlich positiven AGI-Tests sowie der angetroffenen Jcc-Offsets. Weder EBP-/RAM-Werte, vollständige x86-Rohspuren noch Creative-VxD-Inhalte werden öffentlich gemacht.

Die existierende private P108-Testkette kann über `--require-p110-translator` auch den P110-Trace verlangen. Wird diese Option gesetzt, ist ein fehlender P110-Trace **NO-GO**. Ohne die Option bleibt die ältere P108-Originalprüfung davon unberührt. Das ist eine **separate Forschungsabnahme**, keine vorzeitige Freigabe des VST3-Timings.

## Unabhängige CI-Gates und offene Messung

1. `tests/test_p110_original_p54c_compile_timing.py` schützt die exakten originalen Quellen, die JIT/Interpreter-Voraussetzungen, die Quellanker und den standardmäßig deaktivierten Beobachter mit fiktiven Negativdaten.
2. `.github/workflows/p107-original-old-dynarec-branch-capture.yml` baut tatsächlich unter Ubuntu und Windows den **SHA-gepinnten originalen alten x64-86Box-Dynarec** nach P107/P108 **plus P110-Original-Pentium-Codeinstrumentierung**, ohne private Win95/ROM/SF2-Daten. [Lauf #44004309787](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44004309787).
3. `.github/workflows/p110-original-p54c-translation-audit.yml` führt fiktive Quellspuren- und Mutationsprüfungen unter Windows/Linux aus. [Lauf #44004528547](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44004528547): **beide Jobs erfolgreich**.
4. Die private V4-Gesamtpaket-Abnahme soll künftig dieselben drei Originalbelege aus dem laufenden Win95-Gast verbinden: **P107 ausgeführte x86-I-Spur**, **P108 direkte x86-Jcc-CF/ZF-B-Spur**, **P003 echte physische CPU/EMU8000-Port-TSC-Spur**; optional zusätzlich **P110 ursprüngliche Codegen-Übersetzungsmaske/-Zyklen**.

**Kritische Grenze:** Eine Quell- oder Build-PASS-Meldung bedeutet **nicht**, dass ein neuer privater Original-Gastlauf stattfand. Solange dieser Lauf fehlt, gelten weiterhin 16 P107-Jcc als UNKNOWN und die Original-91/93/95-Taktursache bleibt ungeklärt. Auch eine künftige positive P110-AGI-Zählung beweist **nicht** ohne Korrelation mit den ausgeführten 8 variablen Stereo-Folgen die autonome Gast-CPU-Taktparität. Keine Produktintegration, keine Änderung am Original-Chip, kein Merge, keine frühe Studio-One-/ASIO-Abnahme.
