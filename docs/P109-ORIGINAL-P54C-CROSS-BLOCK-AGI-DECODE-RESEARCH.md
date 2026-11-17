# P109 – Originaler 86Box-P54C: Codeblock-Grenzen, AGI und Jcc-Decode

**Status: source-verifizierte Forschung, Windows-/Linux-C++23-Regression bestanden. KEINE Original-Win95-Zeitabnahme.** Erkenntnisse aus dem unveränderten ursprünglichen 86Box-Fork `turican0/86Box-AWE32 v6.0-AWE32-0.93`, nicht aus geschätzten 91/93/95-Werten oder nachgebauten Creative-Treibern. Keine Änderung an Produkt-VST3, originalem EMU8000-DSP, Windows-95-Treiber oder proprietären Medien.

## Neu identifizierter, bislang nicht modellierter 86Box-Mechanismus

In der Originalquelle [`src/cpu/codegen_timing_pentium.c`, Zeilen 958–961](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/cpu/codegen_timing_pentium.c#L958-L961) lautet der vollständige P54C-Codeblock-Reset:

```c
u_pipe_full = decode_delay = decode_delay_offset = 0;
```

**Auffällig:** Das separat definierte statische `regmask_modified` (Zeile 106) wird **nicht** zurückgesetzt. [`check_agi`, 1000–1011](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/cpu/codegen_timing_pentium.c#L1000-L1011) vergleicht aber die Adressregister der nächsten Instruktion **gerade mit diesem Maskenwert** und gibt bei einer ungedeckten AGI-Abhängigkeit einen zusätzlichen Ausführungs-Zyklus vor. Die echten Maskenupdates stehen in Zeilen 1222, 1262, 1274, 1305. Besonders relevant: `codegen_timing_pentium_block_end` (1309–1319) leert möglicherweise die vorgemerkte U-Pipe-Instruktion **ohne** `regmask_modified` neu zu setzen.

Damit bleibt in der *originalen Emulator-Timing-Implementierung* ein Anteil des Registerabhängigkeitszustands **auch über Übersetzungs-Codeblockgrenzen hinweg** erhalten. Das muss beim Erklären der ursprünglichen 91/93/95-Takt-Differenzen berücksichtigt und **erst im realen 86Box-Originalgast** gemessen werden. Die bloße Quellbeobachtung beweist weder, dass dieser Zustand in unseren konkreten acht variablen CCCA→VTFT-Intervallen wirksam war, noch dass eine echte Pentium-P54C-Hardware denselben emulatorinternen Zustand hätte. Eine Quellbesonderheit der Original-86Box-Übersetzungsreihenfolge darf nicht ungeprüft als Creative-Treiber- oder Hardwaregesetz übernommen werden.

### Getrennt zu prüfende Originaleffekte

- **AGI-Adressen:** `get_addr_regmask()` arbeitet mit ModR/M/SIB (Original `codegen_timing_common.h` 119–176); `check_agi()` verwendet den Übergang zwischen letztem `regmask_modified` und dieser neuen Adressquellenmaske. Das implizite ESP wird durch `REGMASK_IMPL_ESP = 1<<8` ausgenommen, mit einer Ausnahme bei zuvor *explizit* verändertem ESP.
- **U/V-Pairing:** Bei Paarbildung wird `regmask_modified = get_dstdep_mask(...) | u_pipe_regmask` gesetzt (1262). Ein nur als `bool AGI` oder statischer fixer Branchpreis modellierter Scheduler verliert diese Abhängigkeiten.
- **Prefix-Decode:** Für Nicht-MMX-Pentium ist ein normales Präfix nur eventuell durch vorige Ausführung überschattbar (971–997), aber `0F` vor `80–8F` (nahe Jcc) kostet in der Original-86Box explizit **keinen** Präfix-Decode-Zyklus. Eine pauschale `+1 je 0F`-Konstante wäre falsch.
- **Branch/Pending-Cycles:** `src/codegen/codegen_x86-64.c` 985–1005 wendet die durch den P54C-Decoder aufgebauten `codegen_block_cycles` beim JIT-Generieren an und hat einen speziellen Taken/Not-Taken-Flushing-Pfad. Block-TSC ist damit **nicht automatisch** Instruktions-TSC.

## Fertig implementierte Forschung und unabhängige Tests

- `research/p109_original_p54c_agi_block_carry.hpp`: kompakter C++23-**Forschungsbeobachter**, der nur die tatsächlich aus dem Original belegt vorhandenen Masken-/Prefix-Regeln abbildet. Keine Produkt-Echtzeit-Engine, keine fiktiven PCM-/TSC-Werte.
- `tests/p109_original_p54c_agi_block_carry.cpp`: testet `regmask_modified` bei Codeblock-Reset, bei Pending-U-Pipe-Flush, beim Ersetzen und beim Vereinigen von U/V-Zielregistern, bei explizitem/implizitem ESP, bei **allen 16** `0F 80–8F`-Jcc-Sekundäropcodes und mit **25.000** deterministischen Blockübergängen.
- [Actions #44001932246](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44001932246): native Ubuntu **GCC + Clang C++23 PASS**, native Windows 2022 **Clang C++23 PASS**.

**Nicht bewiesen / deshalb NO-GO:** welche konkreten Original-P003-LCOD-Instruktionen an der realen Win95-Gast-Codeblockgrenze in dem aktuellen `regmask_modified`-Zustand standen, wie häufig die entsprechende AGI-Bedingung tatsächlich erfüllt wurde und ob diese Unterschiede einen Teil der acht ursprünglichen 91/93/95-Taktmuster erklären. Die 16 P107-Next-PC-Lücken bleiben bis zum P108-Originalgastlauf weiterhin offen. Die isolierte C++23-Quelle gibt `autonomous_original_guest_scheduler_ready=false` zurück und darf nicht in das normale Produkt-Timing übernommen werden.

## Konkretes nächstes Originalexperiment

Auf derselben privaten SHA-verifizierten Win95-VM muss ein originalgetreuer P54C-86Box-Lauf nicht nur P107-x86-I, P108-CF/ZF-Jcc-B und die originale physische EMU8000-Portspur sammeln, sondern bei **der Erstellung des ursprünglichen 86Box-JIT-Codeblocks** den Start-Gast-PC, das unmittelbar vor `codegen_timing_pentium_block_start()` vorhandene `regmask_modified`, den u-pipe-/AGI-Zustand und den gebuchten `codegen_block_cycles` aufzeichnen. **Die Übersetzungszeit der Codeblöcke ist von deren späterer tatsächlicher Ausführung zu unterscheiden.** Erst eine Zuordnung der wirklich ausgeführten LCOD-Pfade zu den translatorischen P54C-Zeitwerten kann entscheiden, ob der Persistenz-Zustand ursächlich beteiligt war.

Der Entwickler von P005 soll bis dahin keinen Reset von `regmask_modified` annehmen und keine festen 91/93/95-Takt-Lookup-Werte einbauen. Ein vollständiger authentischer Treiberport ist damit **nicht** schon abgenommen.
