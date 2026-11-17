# P116 – Originale P54C-Opcode-Zeit- und Abhängigkeitstabellen in C++23 zuordnen

**17.11.2026, ausschließlich REA-Forschung. Keine Integration in das verkaufbare VST3.**

## Ausgangslage

Der Entwicklerzweig `develop/p005-original-creative-port-integration` hat bereits `P005OriginalPentiumIntegerTiming.hpp` und `P005OriginalPentiumCpuAudioTimeline.hpp`: Der P54C-U/V-, AGI- und Präfix-Zeitrechner erwartet explizit die **originalen Timing- und Registerabhängigkeitswörter** pro *tatsächlich ausgeführter* Gast-x86-Instruktion, aber **hat keinen vollständigen Original-Opcode-Decoder/Branch-Replay**. Ein zweiter Zeitrechner wäre kontraproduktiv.

Der ursprüngliche, hier nach Git-Blob gesicherte `turican0/86Box-AWE32`-Fork `v6.0-AWE32-0.93` besitzt die nötigen primären Tabelleneinträge und die Auswahlsemantik:

- `src/cpu/codegen_timing_pentium.c`, Git-Blob `aa78ee1c96076eef581ce4f1b5a86ae6c47d82a4`, 32 Zeitwerttabellen und ORIGINAL `codegen_timing_pentium_opcode()`-Auswahl.
- `src/cpu/codegen_timing_common.c`, Git-Blob `0c538fc4e87ef6d4dd26ad8b74c2b556c94a09d8`, 36 Tabellen mit Registerabhängigkeiten.
- `src/cpu/codegen_timing_common.h`, Git-Blob `3d28a89deb2c10ee011d5c490f01cab971744b9d`, originale Bitmaske-Makros und ABI-Bedeutungen.

## Tatsächliche Umsetzung

`tools/p116_original_p54c_opcode_table_generator.py` akzeptiert **nur die drei exakt SHA-gepinnten öffentlichen Originalquellen**, prüft alle Tabellengrößen und verweigert jede Ausführung bei veränderten/fehlenden Tabellen oder Originalschaltern. Er kompiliert die originalen C-Tabellen und exakt den aus dem Originaltext extrahierten `switch(last_prefix)`-Block in einem temporären nativen C-Oracle. Er schreibt numerische, maschinell evaluierte C++23-Konstantentabellen in einen **nur zur Buildzeit generierten** Header sowie öffentlich reproduzierbare Eingabe/Erwartungswertpaare.

`research/P116OriginalP54cOpcodeLookup.hpp` ist eine eigenständige constexpr-C++23-Nachbildung der Tabellenauswahl. Sie berücksichtigt die originale 0F-Präfixfamilie, ModR/M-Registerform, x87-D8–DF-Sondertabellen, 80/81/82/83-Gruppen, C0–D3-Shifts, F6/F7 und FF-Gruppen. Der Nachweis liest **nicht** angenommene NoteOn-TSC-Abstände und verwendet keine Creative-Treiberbytes.

`tests/p116_original_p54c_lookup_differential.cpp` vergleicht alle Original-C-Ergebnisse mit der unabhängig geschriebenen C++23-Auswahl für **10 Präfixklassen × 256 Opcodes × 16 ModR/M-Eingaben = 40.960 Fälle**. Getestet werden 64-Bit-Timingwort, Registerabhängigkeitswort und der tatsächlich ausgewählte Unteropcode. Zusätzlich klassifiziert der Helper unsichere FPU/MMX-Eingaben als nicht für den bereits vorhandenen P005-Integer-Zeitrechner freigegeben. `tests/test_p116_original_p54c_generator.py` enthält Mutations- und Quellintegritätskontrollen.

Die numerischen Tabellen werden nicht als Produkt-Quellcode übernommen. Die ursprünglichen 86Box-Quellen unterliegen ihren jeweiligen Lizenzen; eine mögliche Übernahme in ein kommerzielles Plugin erfordert gesonderte Klärung. P116 ist zunächst nur ein Forschungsoracle für die originalgetreue Parametrierung.

## Fortschrittsgrenze

**Bewiesen, falls CI grün:** die genaue Lookup-Zuordnung der ursprünglichen P54C-Zeit- und Registerabhängigkeitsparameter anhand von öffentlichen Originalquellen. Damit können später unabhängige C++23-Decoder gegen das Original geprüft werden.

**Nicht bewiesen:** die vollständige Decodierung der zur Laufzeit tatsächlich ausgeführten Creative-VXD-x86-Bytes; welche Codeblöcke bei NoteOn tatsächlich genommen werden; PC/Flag/RAM-abhängige Verzweigungen; genaue TSC-Zeit an jedem Hardwarecallback; schwankende Windows-95-Scheduler-Phase oder bitidentische autonome DRY-PCM-Ausgabe.

**Nächster großer Schritt:** Die x86-Befehle des echten ausgeführten Pfads samt ModR/M, SIB, Präfix-/Längen- und Branchentscheidungen in einer reproduzierbaren Decode-Laufzeitfolge liefern und gegen Original-86Box-P110/P111/P113 und P115-TSC prüfen. Ein bloßer Tabellen-Differentialtest ersetzt diese Aufgabe ausdrücklich nicht.

**Betriebsgrenze:** Keine Änderung an P005, PR #6, dem originalen EMU8000-DSP, Windows-95-Gast, Creative-ROM oder Benutzerdateien.