# P108 – Vollautomatische private Original-Windows-95-Gegenprobe

**Stand: 17. November 2026.** Die P108-CPU-Zweigaufzeichnung samt echtem Linux-/Windows-86Box-Build ist im Research-Zweig vorhanden. Der **neue** P108-Original-Windows-95-Gastlauf ist **noch nicht durchgeführt**. Die Originalmesstraces aus P107 bleiben weiterhin der letzte tatsächlich geprüfte Guest-Jcc-Befund (3.458/3.474 Next-PC-Jcc, 16 unbekannt).

## Ausführungsgate, private isolierte Umgebung

`tools/p108_private_win95_original_capture.py` ist ein **reproduzierbarer Originaltest-Runner**, der ausschließlich in der privaten Umgebung mit den bereits vorhandenen Originalen eingesetzt wird; kein Nutzer muss die Daten in die öffentliche GitHub CI übertragen. Das Programm verlangt:

- Die ursprüngliche private 512 MiB FAT16-Win95-VM mit originalem Creative-Windows-95-Treiber und Original-ROM. SHA-256-Check der Original-ROM, der originalen `SBAWE.VXD`, `AWEMAN32.DLL` und des ursprünglichen ersten RAM-SoundFonts.
- Genau das originale Windows-95-Drei-Bank-Test-PE32, SHA256 `3c9306261ed599073f80d9695a8d73140ea65e2de101dc431169c2e6b3f6bc9f`, das vorher im echten Gast **4.176 Creative MIDI API Calls / 0 Fehler** produzierte.
- Die originale zweite Vergleichs-SF2, SHA256 `cfeea9897fce687f97160e069f01ae785e7e736798290a0e07ce56af7d8326fb`.
- Den **echten neu gebauten P108-86Box-x64-Emulator** mit SHA-256-Provenienz aus dem erfolgreichen Linux-Build, altem x64-Dynarec `NEW_DYNAREC=OFF`, JIT + Interpreter + direkte CF/ZF-Beobachtung.

Der Runner kopiert das Ausgangssystem zunächst in einen **neuen Wegwerf-VM-Ordner** und bestätigt die Bitidentität des kopierten Datenträgers vor der einzigen FAT16-Gaststaging-Operation. Er überschreibt weder die ursprüngliche VM noch deren Treiber, ROM oder SoundFont. Nach Ausführung der drei Originalbanken wartet er auf den **vollständig validierten** originalen `P003MODE.LOG`, fordert einen regulären 86Box-X11-Window-Close und bewertet ausschließlich nach sauberem Prozessende. Ein Timeout, Crash, unvollständiger Gast-MIDI-Lauf, fehlende GUI-Close-Möglichkeit oder unvollständige/inkonsistente Originalspuren ist ausdrücklich **NO-GO**.

## Unabhängige Originalspuren

Die neu erzeugte private Sitzung muss **zwei zusammengehörige** echte originale CPU-x86-Messungen liefern:

- `P108-PRIVATE-EXECUTED-I.trace`: P107 Instrumentation, tatsächlich ausgeführte Gast-x86-Instruktionen und deren Sequenznummern (JIT und Interpreter).
- `P108-PRIVATE-DIRECT-JCC-B.trace`: Neue P108 Instrumentation, unmittelbare Gast-Jcc-Entscheidung aus den originalen 86Box-ZF-/CF-Lazy-Flags, mit exakt derselben P107-Sequenznummer.

Der strikt implementierte Vergleich in `tools/p108_verify_direct_jcc.py` fordert **jeden ausgeführten** originalen P107-Jcc mit genau **einem** neuen P108-Flag-Datensatz, bestätigt sämtliche tatsächlich sichtbaren direkten Next-PC-Paare und weist fehlende/duplizierte/falsche Beobachtungen zurück. Nur die zuvor wegen einer fehlenden Nachfolge-Instruktion unentschiedenen Fälle dürfen künftig als **direkt per CF/ZF beobachtet** markiert werden. Das allein ergibt **kein** korrektes P54C-U/V-/AGI-Instruktionszeitmodell und **keine** bitidentische autonome AWE64-VST3-PCM-Ausgabe.

## Wirklich ausgeführte Tests bis jetzt

Die **neun** *synthetischen* Positiv-/Negativtests des neuen Private-VM-Runners sind unter `tests/test_p108_private_win95_original_capture.py` implementiert. **Tatsächlich überprüft:** [GitHub-Actions-Lauf #43991715673](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43991715673) **3/3 PASS**: Quell- und Negativtests (einschließlich 9/9 Runner-Gates), voll instrumentierter Linux-x64-86Box-Build und voll instrumentierter Windows-2022-x64-86Box-Build. Die beiden nativen CI-Builds verwenden ausschließlich öffentliches, SHA-gepinntes 86Box-Quellmaterial; **keinen Original-Win95-Gast**. Die öffentliche CI enthält **keinen** Microsoft-/Creative-Binärcode, keine privaten VM-/SF2-/ROM-/WAV-Daten und keinen echten Original-Gastausführungstest.

**Bisherige belegte Originalaufnahme:** P107 mit 36.784 Guest-I-Ereignissen und 3.458/3.474 Jcc-Next-PC-Befunden; P108 hat für die 16 noch keine **private Originalergebnisdatei**. Deshalb status `P108_PRIVATE_ORIGINAL_EXECUTION_PENDING`, nicht P108 Original PASS.

Auch nach möglichem P108-FLAGS-GO bleibt separat der Gast-CPU-Zeitbeweis für **91/93/95 Takte** und das bitidentische dry VST3-PCM offen.

**Keine Änderungen am Produkt-VST3, kein Merge, keine Studio-One-Frühabnahme. Keine Originaldaten in GitHub.**
