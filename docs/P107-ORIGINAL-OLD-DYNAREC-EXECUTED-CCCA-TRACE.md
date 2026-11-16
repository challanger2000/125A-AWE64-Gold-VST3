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

## Tatsächliche private Original-Windows-95-Ausführung

Eine getrennte private **512-MiB-Kopie** der Besitzer-Win95-FAT16-VM wurde in der Linux-Prüfumgebung bis zum Windows-95-Desktop gestartet. Die physische `P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK`-I/O-Spur wurde dabei tatsächlich erzeugt. Das ursprüngliche Guest-Programm `P002RAM.EXE` wurde in der Gast-VM ausgeführt: Sein originaler Creative-API-Abschlussmarker lautete `P002_WIN95_ORIGINAL_CREATIVE_RAM_LOAD_API_SENT_OK`; Original-Windows-95-Ladevorgang von Bank 1/Bank 2 und MIDI-Notenmeldungen waren im vom **Originaltreiber selbst** geschriebenen Gastprotokoll erfolgreich. Für den ursprünglich fehlenden zweiten Vergleichsbankinhalt wurde eine neue, **nur private** FAT16-VM-Kopie erstellt.

Zum Vergleich mit den 25 ursprünglichen Stereo-Allocator-Fällen ist zwingend genau der **separate Stimmenmodus-Stresstest** erforderlich. Die relevante, bereits vorliegende Gast-EXE trägt SHA-256 `3c9306261ed599073f80d9695a8d73140ea65e2de101dc431169c2e6b3f6bc9f`; die originale, vom Benutzer aufgezeichnete **stereo-verkettete** zweite Bank ist `131574` Bytes und SHA-256 `cfeea9897fce687f97160e069f01ae785e7e736798290a0e07ce56af7d8326fb`. Beide wurden ausschließlich auf eine neue **private** Gast-Kopie vorbereitet; das ursprüngliche VM-Disk-Image blieb unverändert.

**Wichtige Beweisgrenze:** Der erfolgreiche Gastboot, das erfolgreiche 2-Bank-Original-API-Protokoll, der native Build und die synthetischen Jcc-Gegentests bedeuten noch **keinen** beobachteten P107-Sprungausgang. Solange die private P107-Ausführung keinen vollständigen ausgeführten-PC-Mitschnitt mit den fünf erwarteten Jcc-Standorten liefert, bleiben die fünf offenen P106-Entscheidungen **nicht gemessen**. Ein leeres Mitschnitt-File ist ausdrücklich ein **NO-GO** für einen Original-Branch-Beweis. Ebenso darf zusätzlicher JIT-Logger-Hostaufwand die ursprünglichen Programm-/Emulator-Timings verändern; jede positive Branchevidenz muss separat gegen die ursprüngliche uninstrumentierte Hardware-/PCM-Spur gegengeprüft werden.

**Ergebnisziel nach erfolgreichem Gastmitschnitt:** Die tatsächlichen fünf x86-Jcc-Pfade mit Original-`[EBP−0x18]` ermitteln und anschließend den quellbasierten Pentium-P54C-U/V/AGI-Zyklusrechner und die 44,1-kHz-Subsample-Phase validieren. Die fünf Jcc sind keine pauschal paartaktsicheren Instruktionen.

Produktbranch, originaler EMU8000-Kern und P003-VST3-Laufzeitpfad bleiben unberührt; keine vorzeitige DAW-/Studio-One-Abnahme, kein Merge. **Keine Benutzerhandlung erforderlich.**
