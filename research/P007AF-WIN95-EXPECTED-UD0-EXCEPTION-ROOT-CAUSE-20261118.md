# P007AF — Ursache von `[0117:0000B991] Illegal instruction 00008B55 (FF)` geklärt

**Datum:** 2026-11-18. **Einstufung:** Erwarteter Windows-95-CPU-Ausnahme-Selbsttest (`UD0`, Exception `#UD/INT 6`); **kein Fehler der 86Box-CPU-Emulation oder des Creative-Treibers**. Original 86Box-Quellcode und EMU8000-Kern bleiben unverändert.

## 1. Exakte Quelle dieser Protokollmeldung

Unveränderte, release-gepinnte 86Box-AWE32-Quellen:
`turican0/86Box-AWE32`, SHA **`fee1e81977824fd2d5eed4d239589dc30d251dd7`**.

- `src/cpu/386_ops.h:385-394` (16 bit) bzw. `:430-471`: `op0F_*` liest das **zweite Opcode-Byte** aus der Doppelbyte-Sequenz `0F XX`, speichert `fopcode = opcode`, erhöht den Befehlszeiger und ruft `x86_*_opcodes_0f[opcode]` auf. Bei `(FF)` ist **`0F FF`** der fehlerauslösende Befehl, **nicht** der gültige Einzelbyte-Opcode `FF`.
- `src/cpu/386_ops.h:1314-1394`: Die **Pentium**-0F-Opcodetabelle bildet `0F FF` in allen vier Operand-/Adress-Breiten auf `ILLEGAL` ab. Alle anderen 0F-Tabellen im gepinnten Fork ebenfalls.
- `src/cpu/386_ops.h:156-165`: Die exakte Formatvorlage der Eigentümerzeile:
  ```c
  pclog("[%04X:%08X] Illegal instruction %08X (%02X)\n",
        CS, cpu_state.pc, fetchdat, fopcode);
  cpu_state.pc = cpu_state.oldpc;
  x86illegal();
  ```
- `src/cpu/386_common.c:1754-1758`: `x86illegal()` ruft `x86_int(6)` auf. **Dies ist der korrekte x86-Ausnahmevektor** für den ausdrücklich undefinierten `UD0 = 0F FF` (auch Intel-SDM `UD - Undefined Instruction`).

**Bedeutung der konkreten Felder:** `0117` = CS-Segmentselektor, `0000B991` = emulierter Befehlszeiger **nach dem Lesen des 0F/FF-Doppelbyte-Opcodes** (kein Windows-11-Prozessabsturz), `00008B55` = vierbyte `fetchdat` *nach* dem Opcode (kein zweiter Fehler), `(FF)` = **zweites** Opcode-Byte `0F FF`. Die CPU setzt vor INT 6 `oldpc` zurück.

## 2. Warum Windows 95 das absichtlich ausführt

Unabhängige, spezifische Nachweise:
1. **PCjs/PCx86-Entwickler Jeff Parsons am 17.07.2015**, aus echter Windows-95-Setup-Analyse: Windows-95-Kernelinitialisierung führt gezielt die Opcodefolge **0F FF** aus und erwartet, dass der DPMI-Ausnahmehandler **Exception 6** abfängt. Bei einer Emulatorvariante, die das Opcode nicht als invalid erkennt, scheitert die Windows-95-Initialisierung. Originalbeschreibung: https://www.pcjs.org/blog/2015/07/17/ (Abschnitt „Next, I ran into a couple of Windows 95 oddities“).
2. **Intel/AMD x86-Dokumentation**: `0F FF` ist `UD0`, eine undefinierte Instruktion, die absichtlich `#UD` auslöst: https://www.felixcloutier.com/x86/ud
3. **86Box-Fehlerbericht #4872, 2024** eines vom 125A-Projekt unabhängigen Windows-98-SE/86Box 4.2.1: dieselbe Folge `Illegal instruction 00008B55 (FF)`, sogar in derselben unteren Speicherregion (`[0147:0000B9BD]`): https://github.com/86Box/86Box/issues/4872 . Das Issue enthält einen eigenen späteren Host-"buffer overflow"-Absturz, dessen Ursache **nicht** mit dem INT-6-Eintrag gleichzusetzen ist; daraus darf kein Schadensbeweis für den 125A-Gast abgeleitet werden.

## 3. Abgleich mit originalen Eigentümer-Win95-Klangtests

- P007AD: `[0117:0000B991] Illegal instruction 00008B55 (FF)` während Boot; DANACH `P007AD_ORIGINAL_WIN95_WINMM_READY`, sechs NoteOn-Aufnahmen, kein Crash. Filter- und Attack-NRPN damals noch unwirksam, später 16550-MIDI-Pacing-Fehler gefunden.
- P007AE: derselbe Logeintrag; DANACH `P007AD_ORIGINAL_WIN95_WINMM_READY`, Acht-Noten-PCM-A/B/A und `stable_original=1 cc7_mute=1 cc7_restored=1 filter_nrpn=1 amp_attack_nrpn=1 original_win95_creative=1 synthetic=0`, Test **PASS**.
- Auf Basis des CPU-Quellcodes und des unabhängigen Windows-95-Nachweises ist dies die **erwartete absichtliche CPU-Exception #UD**. Das Bestimmen des exakten Windows-Moduls/Namens am CS:IP bedürfte zusätzlicher Gastsymbole; der ausgeführte Befehlsfamilie, CPU-Exception und Sinn des bekannten Win95-Tests sind ohne diese Symbole bereits geklärt.

## 4. Korrekte Engineering-Entscheidung

**Keine Änderung an Original-CPU, keinem Opcode-Decoder, keiner Interrupt-Behandlung, keiner VM-Disk und keinem Creative-Treiber.** Insbesondere NICHT die `ILLEGAL`-Behandlung in `NOP` verwandeln, `x86_int(6)` unterdrücken oder die 86Box-Logzeile pauschal ausblenden. Jede solche „Reparatur“ würde das von Windows 95 gewünschte Verhalten zerstören.

Die Meldung `Illegal instruction` bleibt ein technisch akkurater Emulator-Debuglog, ist aber **bei einmaligem Auftreten während des Bootens, anschließendem WinMM READY und funktionierendem Original-PCM kein Produkt-Stabilitätsfehler und kein Release-Blocker**. Für andere Invalid-Opcode-Stellen, wiederholte Ausnahmen nach READY oder tatsächliche Abstürze gilt diese Ausnahme ausdrücklich **nicht**.

**P007AF Abnahme:** Ursachenanalyse abgeschlossen, bisher fälschlich offener „Stabilitätspunkt“ geschlossen. Restliche 125A-AWE64-Release-Gates (weitere NRPN-Regler, FX-Typ-SysEx, SF2/SBK, Studio Pro) bleiben vollständig bestehen.
