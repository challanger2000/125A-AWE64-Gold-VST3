# P107 — Original x86-Pfadausführung im alten 86Box-Dynarec beobachten

**16.11.2026 · Research / Draft · KEINE autonome x86-CPU-/VST3-Audiofreigabe.**

## Herkunft und integritätsbewahrter Aufbau

Ausgangspunkt ist der **originale** unveränderte 86Box-AWE32-Fork `turican0/86Box-AWE32`, Release `v6.0-AWE32-0.93`. Der vorhandene Besitzer-Win95-Emulator benutzt das CMake-`regular`-Profil mit **`NEW_DYNAREC=OFF`**, während die Gastmaschine Pentium P54C/133,333,333 Hz/`cpu_use_dynarec=1` verwendet. Daher wird ausdrücklich der alte x64-Codegenerator **`src/codegen/codegen_x86-64.c`** instrumentiert, *nicht* `src/codegen_new/`.

Quellcodeanker des unveränderten Forks: Git-Blob `673141762345c1195458bd840461302493f7d365`. Das Quellpatch-Skript `tools/p107_instrument_old_dynarec.py` lehnt jede Abweichung vom Originalanker und wiederholtes Patchen ab. Instrumentierung betrifft nur eine **temporäre Kopie des Forks**. Der EMU8000-DSP in `src/sound/snd_emu8k.c` und die Original-AWE64-Card-/CT1745-Logik bleiben byteweise unverändert. Zusätzlich bleibt die bereits erprobte `tools/p003_instrument_86box_io.py`-Aufzeichnung der physischen I/O-Callbacks erhalten.

## Was der neue Beobachter tut

Der alte Dynarec emittiert im tatsächlich ausgeführten Host-JIT für einzelne Gast-x86-Befehle einen optionalen Observer-Callback, beschränkt auf einen per Gast-CS und absoluter Original-LCOD-Adresse definierten Ausschnitt (`LCOD +0x4023…+0x4125`). Die Aufzeichnung ist im normalen Emulator **AUS**; sie wird nur in einer privaten 86Box-Ausführung über `AWE64_P107_EXECUTED_X86_TRACE` und eine tatsächlich verifizierte `AWE64_P107_LCOD_BASE` aktiviert. Das Quellpatch-Skript verweigert falsche Opcode-Signaturen an den fünf in P106 identifizierten Jcc-Stellen.

Der Callback darf die Gastdaten **nicht ändern**. Er hält die Reihenfolge der **wirklich ausgeführten** Gast-PCs fest und liest am originalen Jcc `LCOD+0x4027` das entsprechende DWORD `[EBP−0x18]` nur wenn ein gültiger, nicht über Seitengrenzen gehender Gast-RAM-Lesezeiger verfügbar ist. Ein fehlgeschlagener Lesetest erhält explizit den Status **UNKNOWN**, nicht einen erfundenen Wert. Die Aufzeichnung wird bei **2 Millionen** Instruktionen begrenzt. Gast-TSC im Callback ist **nur ein blockbezogener Diagnosewert**: Der alte Dynarec garantiert keinen separat kostenberechneten TSC pro emittiertem Host-Callback.

Die Auswertung `tools/p107_audit_executed_jcc_private.py` ermittelt tatsächlich genommene/nicht genommene Sprünge **aus dem nächsten ausgeführten Gast-PC** und vergleicht bei `+0x4027` die Entscheidung mit dem geprüften lokalen Speicherwert. Falsche PC-Nachfolger, fehlende Instruktionen, falsche CS-Segmente und abgeschnittene Ereigniscaps werden abgewiesen.

## Verifizierter CI-Stand

[GitHub Actions #43928377855 – beide reale 86Box-Builds erfolgreich](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43928377855).

| Prüfung | Resultat |
|---|---|
| Original-Fork-Quell-/Git-Blob-Pin und P003-I/O-Patch | **PASS** |
| P107-Quellmutations-/Negativtests | **8/8 PASS** |
| P107-ausgeführte-PC-/Jcc-Parser-Selbsttests (synthetisch) | **PASS** |
| Kompletter originaler Fork mit instrumentiertem **altem x64-Dynarec** – Linux | **Build PASS** |
| Kompletter originaler Fork mit instrumentiertem **altem x64-Dynarec** – Windows x64 | **Build PASS** |
| Original-EMU8000-/Soundkarten-Quellcode geändert | **NEIN** |
| Berechtigte CPU-Zeit-/PCM-Produktfreigabe | **NEIN** |

Die erweiterte Windows-UCRT64-Linkzeile für das inzwischen von `libcrypto` geforderte `bcrypt` wird ausschließlich im kopierten Emulator-Build ergänzt; am Produkt-VST3 und ursprünglichen 86Box-Submodul ändert sich nichts. In der öffentlichen CI werden **nur der Open-Source-Emulator und synthetische Testdaten** verarbeitet, keine Original-Win95-Dateien, VxD-Binärdaten, ROMs, SF2-Samples oder Audioaufnahmen.

## Authentische private Windows-95-Ausführung – endgültiger Original-Stresstest

**Die private Originalausführung war erfolgreich; kein Originalfile wurde nach GitHub übertragen.** Die ursprüngliche Windows-95-FAT16-Disk und original Creative VxD/ROM wurden ausschließlich in einer wegwerfbaren **512-MiB-Kopie** gestartet. Bei der ersten Ausführung fehlte die zweite Stereo-Bank; das normale 2-Bank-RAM-/MIDI-Testprogramm beendete zwar mit `P002_WIN95_ORIGINAL_CREATIVE_RAM_LOAD_API_SENT_OK`, übte den Stereo-Allocator jedoch nicht aus.

Deshalb wurde separat die originale, speziell dafür gebaute 3-Bank-Stresstest-Gastdatei (SHA-256 `3c9306261ed599073f80d9695a8d73140ea65e2de101dc431169c2e6b3f6bc9f`) zusammen mit der tatsächlich stereo-verlinkten 131.574-Byte-Vergleichsbank (SHA-256 `cfeea9897fce687f97160e069f01ae785e7e736798290a0e07ce56af7d8326fb`) **ausschließlich in eine weitere neue Disk-Kopie** eingesetzt. Private Pfade und Inhalte bleiben dort.

Das originale Win95-Gastprotokoll `P003MODE.LOG` bestätigt:
- `ORIGINAL_BANK_PHASES_COMPLETED=0x3` (**3 von 3 Bänken**),
- `ORIGINAL_MIDI_CALLS_TOTAL=0x1050` (**4.176 Creative-Aufrufe**),
- `ORIGINAL_MIDI_CALL_ERRORS=0x0`,
- Abschlussmarker `P003_ORIGINAL_WIN95_ALLOCATOR_STRESS_COMPLETED`.

**Echtes neues Original-executed-PC-Ergebnis:** Der private, unveröffentlichte Mitschnitt `P107-EXECUTED-X86-STEREO-EXACT.trace` wurde nach dem Gastlauf vollständig gelesen (36.667 Ereignisse, 1.969.058 Byte, SHA-256 `b96eaa2b4cb7537e82cf843b07664d8815e05d68c8d543f439f3451e71cb1a12`, **0** CS-/Adressfensterfehler und **0** CPU-TSC-Regressionsmeldungen). Physischer CPU-I/O-V4-Mitschnitt wurde ebenfalls erstellt (ca. 189 MiB). Beide Dateien verbleiben ausschließlich privat.

| Originaler Original-x86-Jcc-Ausführungspunkt | Genommen (direkt bestätigter nächster PC) | Nicht genommen (direkt bestätigt) | **Ungeklärt wegen übersprungenem Observer-Callback** |
|---|---:|---:|---:|
| `+0x4027 JNE` / lokaler Modusindikator | **739** | 0 | **7** |
| `+0x40CB JA` / CCCA-Samplevergleich | **452** | 0 | **8** |
| `+0x40E4 JE` / Bewertungs-Strafwert | **756** | 0 | **3** |
| `+0x40F1 JAE` / Vergleich mit bisheriger Stimme | **474** | **251** | **40** |
| `+0x4107 JB` / Auswahl bisheriger bester Paar-Kandidaten | **640** | **75** | **22** |
| **Summe** | **3.061** | **326** | **80** |

Von den **746** Original-Ereignissen an `LCOD+0x4027` war der ausschließlich per Gast-RAM-Cache gelesene **tatsächliche lokale Wert `[EBP-0x18]=1`** in allen 746 lesbar. Bei **739** Events war der nächste aufgezeichnete PC exakt der Original-`JNE`-Sprungziel-PC `+0x4048`; die übrigen **7** bleiben **ungeklärt** und dürfen nicht als identisch behauptet werden. Der Originalwert ist ein **lokales Stack-DWORD**, **nicht direkt der Geräte-RAM-Wert `[device+0x18]`**. Die beiden dürfen keinesfalls gleichgesetzt werden.

## Nachweisumfang und präzise Grenze

Der P107-Observer emittiert aktuell den Callback im alten x64-JIT-Generator. Nicht jeder für die P106-Kontrollflussgrafik relevante Zwischenbefehl wird durch diese erste Version zuverlässig protokolliert. **Wenn unmittelbar nach einer beobachteten Jcc-Stelle nicht genau der Original-Ziel-PC oder Fallthrough-PC im Trace folgt, lautet das Ergebnis `UNKNOWN_SPARSE_OBSERVER` – weder `taken` noch `not_taken`.** Genau deshalb wurden die 80 unklaren Fälle gesondert ausgewiesen. Der Quell-Auditor `tools/p107_audit_executed_jcc_private.py` wurde auf diese fail-closed-Behandlung korrigiert; öffentliche CI nutzt nur synthetische Gegenproben.

Die echten 3.387 direkten Jcc-Nachfolger sind ein **positiver Beobachtungsbefund**, jedoch **noch kein originalgetreues vollständiges CPU-Instruktions-/EFLAGS-Protokoll**. JIT-Instrumentierung kann die Hostausführungszeit und unter Umständen Abhängigkeiten/Flags beeinflussen. Der im Callback abgelesene TSC-Wert ist **nicht** automatisch die exakte originale TSC-Zeit jedes x86-Befehls. Daher auch **noch keine** Erklärung der ursprünglichen variablen 91/93/95-Taktabstände und **keine** selbständig aus MIDI erreichte register- bzw. bitgenaue PCM-Parität.

**Weiteres P003-Gate:** vollständige bzw. nachweislich lückenlose x86-Instruktionssequenz im passenden ALTEN Dynarec; dann unabhängige Bewertung von P54C-U/V-Pairing, AGI, tatsächlichem dynamischem x86-State und 44,1-kHz-Subsample-Phase; zuletzt neue Win95-gegen-VST3-PCM-Holdouts. Keine synthetischen TSC-Konstanten oder nachträglich übernommene Original-Zeitstempel in die Engine.

Öffentliche Reproduzierbarkeit: GitHub-Actions Windows/Linux reale Emulator-Builds [#43928377855](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43928377855). **Kein AWE64-Gold-VST3-Produktcode geändert**, kein Merge, keine DAW-Vorabprüfung, **keine Handlung des Nutzers erforderlich**.
