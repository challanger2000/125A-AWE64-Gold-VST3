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


## Verbesserung vom 17.11.2026 – Eingangsarchiv und physische Port-TSC-Spur

Der ursprünglich private P108-Runner akzeptiert jetzt optional `--private-original-v4-package`. `tools/p108_resolve_private_original_package.py` sucht **ausschließlich** das originale, bekannte und SHA-256-gepinnte verschachtelte 86Box-Win95-Archiv in der vorhandenen V4-Gesamt-ZIP und verwendet vorhandene, **eindeutig per SHA nachgewiesene** drei-Bank-PE32- und SF2-Daten. Fehlen diese beiden **nichtproprietären Testdateien** im Archiv, erzeugt P108 sie jetzt selbst aus den SHA-verifizierten **öffentlichen** 125A-Quellen: die Original-Stress-EXE über den ursprünglichen MinGW-P54C-Compilerbefehl plus die beiden belegten Original-COFF-Metadatenworte, und die zweite Stereo-Test-SoundFont aus dem öffentlichen Python-Generator. **Beide neu erzeugten Dateien müssen den exakten vorher authentifizierten Original-SHA-256 erreichen; andernfalls NO-GO.** Die originale private Windows-95-/Creative-/ROM-VM wird dadurch ausdrücklich nicht ersetzt oder publiziert. Das vollständige Paket wurde wegen der hier nicht verfügbaren privaten lokalen Laufzeit **nicht** erneut geöffnet; es ist deshalb nicht bewiesen, dass alle drei Quellen darin enthalten sind.

Das Archivmodul prüft auch den **unveränderten ZIP-Eintragsnamen vor der Windows-Pfadnormalisierung** (`ZipInfo.orig_filename`). Eine reale plattformabhängige Testsicherheitslücke wurde damit behoben: Rückwärtsschrägstriche im ursprünglichen Archivnamen würden ansonsten durch Pythons Windows-Normalisierung unbemerkt zu normalen Pfadtrennern. Weitere negative Tests: `../`, doppelte Dateien, Symlinks, ZIP-Bombs, falsche Original-SHAs, fehlende exakte PE32-/SF2-Quellen, fehlende 512-MiB-Gastplatte und Kopieren in ein bestehendes/verbotenes Ziel.

**Wichtige zusätzliche Laufzeit-Abnahmebedingung:** `verify_original_physical_cpu_io` prüft nun auch die echte physische **P003 V4 Stack CPU-EMU8000-I/O-Spur** im *selben* Originalrun. Unmittelbar gefordert: korrekter physischer V4-Header, mehr als 100.000 echte physische Portcallbacks, gültige EMU8000-Registerports (`0x620/0x622/0xA20/0xA22/0xE20/0xE22`), fortlaufende Callbacksequenz und monotoner Gast-TSC. Fehlende/duplizierte Ereignisse oder eine regredierende Uhr ergeben NO-GO. Aus den Daten wird nur SHA-256 und die geprüfte Gesamtanzahl ausgewiesen; Originalregister und x86-Rohspuren bleiben privat. Auch eine physisch korrekte I/O-Spur beweist **noch keine autonome VST3-Zeitparität**.

Die neuen source-only Negativtests laufen als eigene **schnelle**, echte Windows-/Linux-CI (`.github/workflows/p108-private-owner-audit-unit.yml`), ohne für jede Python-Testkorrektur den kompletten 86Box-Emulator neu bauen zu müssen. [GitHub Actions #43995310072](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43995310072): **beide Plattformjobs PASS**. Die realen P108-Emulator-Builds waren bereits im [Lauf #43991715673](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43991715673) erfolgreich. Keine Windows-95- oder Creative-Daten waren in diesen CI-Läufen enthalten.

**Weiterer Originalstatus unverändert:** P107 **3.458/3.474** echte Jcc-Next-PC-Beobachtungen, P108 mit der privaten Original-VM noch nicht neu aufgenommen. Daher **keine** Behauptung von 16/16 echten neuen Original-Zweigentscheidungen, keinen 91/93/95-Zyklen- und keinen bitgenauen trockenen VST3-PCM-Nachweis.


## Zwei ursprüngliche Testdateien jetzt nachweislich bitidentisch rekonstruierbar

**Echter Nachweis aus [GitHub-Actions-Lauf #43996602936](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43996602936): alle drei Jobs PASS** (Ubuntu-Python-Prüfungen, Windows-Python-Prüfungen, echte Ubuntu-22-MinGW-P54C-PE32-Neukompilierung und strenge Original-Artefakt-Gegenprobe).

**Stereo-Vergleichs-SF2:** Die seit P003 verwendete, frei erzeugte Stereo-SF2 aus `generated_original_mode_stereo_sf2()` stimmt unter Windows und Linux vollständig mit der früheren Originaltest-SoundFont überein: SHA-256 `cfeea9897fce687f97160e069f01ae785e7e736798290a0e07ce56af7d8326fb`. Es sind **keine** Creative- oder sonstigen privaten Sampledaten enthalten. Der private Resolver benötigt bei fehlender SF2 keine separate Nutzerdatei mehr.

**Originaler Windows-95-3-Bank-Stress-PE32:** Im originalen öffentlichen Build [P003-Lauf #43787795937](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43787795937) entstand die ursprüngliche **23.866-Byte**-`P002RAM.EXE` SHA-256 `3c9306261ed599073f80d9695a8d73140ea65e2de101dc431169c2e6b3f6bc9f`. Der neue Build verwendet die **bitidentischen historischen Quellblobs** (`a3292292...`, `d664a2...`) und exakt die alten Compilerflags. Ein direkter Download-/Binär-Gegentest im CI ergab **gleiche Datei-Länge, identische Hashes ALLER PE-Sections, aber sechs geänderte Headerbytes in nur zwei Feldern**: COFF-Zeitstempel und PE-OptionalHeader.CheckSum. Der Originalzeitstempel `1794793373` und die Originalprüfsumme `46455` wurden *ausschließlich* aus dem ursprünglichen **öffentlichen** PE-Artifact verifiziert. `tools/p108_rebuild_exact_public_stress_pe.py` restauriert **nur** diese beiden Metadatenworte und verlangt danach den vollen historischen **PE32-SHA-256**. **Der vollständige Original-SHA-Prüfschritt ist im echten Ubuntu-MinGW-CI PASS.** Keine Code-/Daten-Section wird verändert. Der private Runner kann deshalb auch ohne einzelne ursprüngliche Stress-`P002RAM.EXE` diese Datei durch vollständige SHA-Beglaubigung erzeugen, sofern der öffentliche MinGW-i586-Compiler verfügbar ist.

Bei fehlendem Compiler, abweichenden öffentlichen Quellblobs, anderer Sections-Anordnung, falschen Originalmetadaten oder beliebiger Abweichung des resultierenden gesamten PE32 bleibt die Rekonstruktion **NO-GO**. Der neue Test erstellt keinerlei Produkt-VST3-DSP und ersetzt **nicht** die privat unverändert zu bootende Original-Win95-VM.

Diese nachweislich bitidentische öffentliche **Testmaterial-Rekonstruktion** ist klar getrennt vom weiterhin **fehlenden** P108-Original-Win95-x86-Ausführungsgate: die letzten **16** ursprünglich nicht direkt beobachteten Original-Jcc-Ausgänge und die autonome 91/93/95-Takt-Parität sind damit **nicht** automatisch gelöst.
