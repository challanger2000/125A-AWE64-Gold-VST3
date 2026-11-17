# P118 – Tatsächlich betretene x86-JIT-Stelle eindeutig an originale P54C-Timingdaten binden

**17.11.2026 – Nur 125A-AWE64-Forschung, kein Release oder Produkt-P005-Änderung.**

## Das Problem nach P117

P116 hat **40.960/40.960** Original-86Box-C-Tabellenzuordnungen unabhängig in C++23 reproduziert. P117 kennt die originalen Timing-/Registerabhängigkeitswörter jedes **übersetzten** Creative-VXD-x86-Opcode-Standorts und die zugehörige P111-JIT-Compile-Epoche. P107 beobachtet **wirklich betretene** JIT-Gast-x86-Stellen, kennt aber nur PC/EBP/Block-TSC und kann wiederverwendete JIT-Slots beziehungsweise denselben Gast-PC in verschiedenen Compiler-Generationen nicht eindeutig mit P117 verbinden.

**P118 fügt genau diese fehlende Querverbindung in den ORIGINALEN 86Box-OLD-x64-JIT-Code ein.** Es erfindet keine CPU-Kosten und analysiert keine originalen privaten Treiberdateien.

## Technische Umsetzung

- `tools/p118_bind_runtime_jit_to_p117_opcode.py` verlangt den unveränderten SHA-gepinnten Original-OLD-Codegen und die vollständige bestehende P107/P111/P117-Instrumentierung in einer **wegwerfbaren 86Box-Kopie**.
- P117 setzt vor jedem Compile-Opcode den alten Site-Token zurück. Nur ein erfolgreich zugeordneter Opcode mit überprüftem P111-Compile-Block (`slot`, `pc`, `phys`, `epoch`) bekommt seine **monoton eindeutige 1-basierte P117-Site-Nummer**.
- Der bestehende P107-JIT-Callback wird nicht dupliziert: Auf x64 lädt der generierte Hostcode vor derselben einzigen Callback-Stelle einen dritten, **unmittelbaren** Parameter, nämlich die P117-Site-Nummer. Windows x64 verwendet das Original-ABI-Argument `R8D` (`41 B8 imm32`), Linux SysV AMD64 `EDX` (`BA imm32`). Original-Gast-PC und tatsächliches Gast-EBP bleiben die ersten beiden Parameter.
- Der neue Wrapper ruft weiterhin den unveränderten P107-Logger auf; bei `AWE64_P118_EXECUTED_SITE_TRACE` zeichnet er zusätzlich die **exakte P107-Ereignisnummer**, die eingebettete Compiler-Site-ID, Gast-PC und den originalen beobachteten Block-TSC auf. Wenn P107/P111/P117-Provenienz fehlt, ist das Ergebnis ausdrücklich `NO_GO`, niemals ein ersatzweise aus dem PC erratener Compiler-Standort.
- `tools/p117_stage_original_p54c_epoch_probe.py` baut nun P003/P115 und P107–P118 in derselben isolierten 86Box-Originalkopie zusammen. Soundkarten-/EMU8000-Quelldateien bleiben byteidentisch.

## Unabhängige Auswertung

`tools/p118_join_original_runtime_opcode_site_private.py` akzeptiert die kompletten Originalspuren P107/P111/P117/P118 aus **einer** privaten Win95-Gastsitzung und optional zusätzlich P110/P113 zur vollständigen Quellprovenienz. Es verwirft kaputte/abgeschnittene Spuren sowie ungültige Site-IDs, doppelte/falsche Laufzeitfolge, widersprüchliche PC/TSC-Datensätze und Kompilate, deren P111-Generation nie aufgerufen wurde.

Wichtig: P107 enthält auch **Interpreter-Erstlauf-Ereignisse** ohne JIT-Codeaufruf. Diese werden ausdrücklich nicht nachträglich zu JIT-Events erklärt. Gleiche PCs können in verschiedenen Compile-Epochen vorkommen; der tatsächlich in den Host-JIT-Code eingebettete Site-Token verhindert den falschen Join über die PC-Adresse.

Der Auditor gibt ausschließlich aggregierte Zahlen und Datei-SHA-256 aus. Keine Roh-PCs, Creative-ROM-/Treiber- oder VM-Dateien nach GitHub.

## Beweisgrenze und nächster größerer Schritt

**Bei erfolgreicher öffentlicher CI nachgewiesen:** Die neue originale OLD-x64-JIT-Provenienzinstrumentierung lässt sich in unveränderte gepinnte Quellen integrieren; die Tests decken Block-Epochen/Slot-Wiederbelegung/Verlustfälle und die Host-ABI-Auswahl ab.

**Noch nicht nachgewiesen:** Ein privater Original-Windows-95-Gastlauf mit diesem JIT-Callback, vollständiger Opcode-**Abschluss** statt bloßem Eintritt in die JIT-Stelle, genaue x86-Instruktionsdauer, sämtliche Interpreter-/Interrupt- und Timerpfade sowie bitidentischer autonomer NoteOn-PCM-Vergleich.

Nächster Beweis: Bei einer vollständigen Originalmessung die echten P118-Site-IDs mit P117-Timingworten und P115-Live-CPU-Portzuständen zusammenführen; dann die **tatsächlich ausgeführten** Blockkosten unter Original-Bedingungen gegen den bestehenden P005-C++23-Pentium-Rechner prüfen. Keine weitere gleichartige Forschungs-Übergabe an P005 vor einem belastbaren Originalgastvergleich.

## Sicherheits- und Produktgrenzen

Quelle: `turican0/86Box-AWE32` Tag `v6.0-AWE32-0.93`. Original-86Box-/Creative-Lizenzbedingungen bleiben zu prüfen, insbesondere für ein kommerzielles VST3. Kein modifizierter EMU8000-DSP, kein proprietäres Creative-Binary im Forschungs-GitHub, keine Änderung am unabhängigen Entwicklerzweig `develop/p005-original-creative-port-integration`.