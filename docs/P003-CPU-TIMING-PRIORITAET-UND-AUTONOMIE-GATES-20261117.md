# P003 – CPU-Zeitsteuerung jetzt Priorität 1: Integrations-Gates (17.11.2026)

**Ziel:** Das native Windows-x64-VST3 muss vom MIDI-Ereignis aus die originalen Creative-EMU8000-Registerwerte **in der richtigen Reihenfolge zum richtigen Zeitpunkt selbstständig** erzeugen – ohne archivierte Windows-95-Timestamp-Lookups. Diese Aufgabe hat Vorrang vor weiterer Stimmenallocator-Detailforschung. Dies ist ein internes Forschungsdokument, **kein Entwickler-PR-Kommentar und kein eigenständiger zweiter Treiberport**.

## Ausgangsbeweise (Originalbelege nicht veröffentlicht)

1. Quelle `docs/P003-AUTONOMOUS-NATIVE-PORT-TIMING-CANDIDATE.md`: Im alten Source-Clocked-Referenzmodus waren mit originalen Zeitpunkten **160.000/160.000** trockene PCM16-Werte identisch. Das beweist den Register-/Chip-Vergleich bei zugeführter Zeitbasis, **nicht** die Eigenständigkeit des Zeitplans.
2. Zwei echte Win95-Gastläufe vom 15.11. mit demselben C4-/D4-MIDI-Programm erzeugten dieselben **relativen CPU-TSC-Abstände pro OUT** (C4 NoteOn 9.500 CPU-Zyklen; D4 NoteOn 10.058; NoteOff 331/325), aber unterschiedliche 44,1-kHz-Framebelegungen der Portwrites. **Daraus folgen zwei separate Probleme:** (A) relative CPU-Arbeit und (B) die sub-sample CPU-/Chip-Phase bei einem MIDI-Ereignis.
3. Der derzeitige `CreativeAutonomousOutTiming.cpp` verwendet ausdrücklich **experimentelle** feste Werte für Select/Write/Voice-Search und als VST3-Standard einen **unverifizierten Halbsample-Phasenplatzhalter**. Er ist bei gleichen Noten nicht universell originalidentisch: Quelle `docs/P003-AUTONOMOUS-NATIVE-PORT-TIMING-CANDIDATE.md`, zwei Win95-Session-Vergleiche bei tatsächlicher Gastphase **322/324** und **319/324**; mit festem Halbsample-Fallback nur **267/324** bzw. **272/324**.
4. Die zwei privaten, SHA-gepinnten V5-Stereo-Aufnahmen (16. und 17.11.) zeigen einen echten zusätzlichen Modus-/Nachbarzweig für `[ESI+0x18]<=2` und sieben übrig gebliebene jeweils **+3 Gastzyklen** bei physischer Stimme 4. Sie sind ein möglicher CPU-Timing-Mikrofehler, **nicht** die Hauptursache fehlender allgemeiner MIDI-/Chip-Phasengleichheit. Das eigene Forschungsmodell ist empirisch, darf nicht in P005 als Originalformel übernommen werden.
5. Konfiguration des originalen 86Box-Gasts: **133.333.333 CPU-Zyklen/s**, EMU8000 **44.100 Samples/s**; nominell etwa **3.023,4316 CPU-Zyklen pro Sample**. +3 Takte entsprechen nur ~0,000992 eines Samples, können an einer Original-Samplegrenze aber immer noch einen Framewechsel auslösen. **Priorität sind zuerst ganze NoteOn/NoteOff-/Controller-Portfolgen und die reale Phase**, nicht das blinde Fitting dieser sieben 3-Takt-Fälle.
6. Die separate P110-Forschung zur Pentium-Pipeline und Registermasken kann relevante Erkenntnisse liefern, aber nicht einfach ohne unabhängige Zuordnung zum ausgeführten Creative-Block in `CreativeAutonomousOutTiming.cpp` übernommen werden.

## Neuer Forschungs-Fokus: vier überprüfbare Gates

**Gate A – echte Original-CPU-Basis und Instruction-Sites**  
Für komplette ursprüngliche MIDI-Sequenzen die exakten Gast-TSC- und Portzeitpunkte, source `CS:EIP`, den CPU-Timingmodus, 86Box-Blockausführungsart und Rechenphasen sammeln. Der bereits quellgepinnt instrumentierte 86Box-Dynarec-Build unter `tools/instrument_p003_86box_dynarec_clock.py` liefert optional die fehlende Blockspur. **Kein neuer Original-VM-Lauf ohne konkreten Erkenntnisgewinn**; zuerst vorhandene Originalspuren ausschöpfen.

**Gate B – native relative Portabstände (OHNE Original-Timestamps als Eingabe)**  
Direkte, quellbelegte x86-Kosten je tatsächlich ausgeführtem Originalbranch und originalem Status lesen; die vollständigen originalen Portabstände mindestens an zwei unabhängigen Win95-Sitzungen vergleichen. Erfolg verlangt **unabhängige Holdout-Übereinstimmung aller CPU-OUT-Abstände**, nicht nur Trainingsdaten. Nur Quellkosten, keine Tonhöhen-/Preset-/Noten-ID-Lookups und keine pauschalen +14- oder +3-Takt-Hacks.

**Gate C – Host-MIDI-Zeit zu Chipphase (ein eigenständiges Problem)**  
Das VST3 erhält MIDI mit Sample-Offset, aber nicht dieselbe virtuelle CPU-Subsample-Phase wie die zufällig gestartete Win95-VM. Deswegen zunächst eine **explizite deterministische Phase** für den VST3-EMU8000-Chip festlegen und die originale CPU-/Chip-Umrechnung als rationale/integer-basierte Rechnung ohne Drift implementieren. Den Win95-Referenzvergleich mit **bekannter, vergleichbarer Phase** durchführen. *Bitidentität zu jeder zeitlich anders gestarteten originalen Win95-Session ist nicht sinnvoll forderbar,* wenn die originale Port-zu-Sample-Phase selbst von Sitzung zu Sitzung variiert. Nur für identische Eingangsvoraussetzungen/Phase kann ein 1:1-PCM-Nachweis geführt werden.

**Gate D – natives VST3 und trockener PCM-Gegenbeweis**  
Die native VST3 **muss ihren Zeitplan autonom erzeugen** (ohne Quelloffset-Playback), dann im echten Windows-VST3-Host gegen mindestens eine authentifizierte Win95-Referenz bei kontrollierter Startphase verglichen werden. Registerfolge und Wert, Frameposition jedes OUT, ADC-/DSP-Reset, dry-only (Reverb/Chorus-Sends 0) und kompletter PCM-Block sind separat zu auditieren. Ein reiner C++-Unit-Test oder eine source-clocked Wiedergabe ist **keine** solche Freigabe.

## Immediate Task nach Prioritätswechsel

**Entscheidende Lücke statt neue Micro-Tests:** Den bestehenden `CreativeAutonomousOutTiming.cpp`-Phasenplatzhalter und die unverifizierten strukturellen Kosten **nicht** als originale Zeitbasis einstufen. Der aktuelle P003-Status bleibt **AUTONOMOUS CLOCK = NO-GO**. Die CPU-Forschung arbeitet zuerst an Gate B/C und nutzt vorhandene Source Clock Trace/Original-VXD-Disassembly. Der separate REA-Entwickler kann währenddessen ungestört fertig portieren.

**Keine neue Entwicklerübergabe für jeden kleineren Befund.** Erst nach einem vollständig geprüften, zusammenhängenden Timingpaket ein gebündelter Abschlussbericht, wenn die Abnahme ihn rechtfertigt.

## Rückverfolgbare, öffentliche Quellen

- [P003 autonomer Kandidat mit echten 324-Port-Holdouts](P003-AUTONOMOUS-NATIVE-PORT-TIMING-CANDIDATE.md)
- [Doppelsitzungs-Modusgate und offene +3-Zyklen](P003-ORIGINAL-CREATIVE-STEREO-MODE-GATE-20261117.md)
- [Bisherige TSC-/Dynarec-Phasenanalyse und Instrumentierung](P003-ORIGINAL-DYNAREC-14-CYCLE-PHASE-RESEARCH-20261117.md)
- [Autonomer experimenteller CPU-Scheduler](../src/creative_compat/CreativeAutonomousOutTiming.cpp)
