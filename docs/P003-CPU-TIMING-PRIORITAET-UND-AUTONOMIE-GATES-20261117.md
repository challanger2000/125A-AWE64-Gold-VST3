# P003 – CPU-Zeitsteuerung jetzt Priorität 1: Integrations-Gates (17.11.2026)

> **WICHTIGE ORIGINALKORREKTUR, 17.11.2026 (ca. 09:00 MEZ):** Der unten noch historisch dokumentierte **99,957-%-Holdout** wurde mit dem **falschen, nur nominalen CPU-Timerwert 133333333 Hz** gerechnet. In der echten 86Box-Funktion `pit_set_clock()` (`src/pit.c`) wird dieser Wert absichtlich auf **400000000/3 Hz** korrigiert, bevor `TIMER_USEC` und der Wavetable-Latch gebildet werden. Eine an der **ersten von vier** Originalaufnahmen festgelegte Phase ergibt damit **4.366.834 von 4.366.834** exakt gleiche CPU→EMU-Frame-Zuordnungen über alle vier SHA-gepinnten Original-Sitzungen. Die damalige Annahme, die 488/517 Restfehler müssten zwingend aus CPU-Block-/Dynarec-Nachlauf entstehen, ist **für diese Framefehler hinfällig**. Die originalen MIDI→TSC-Instruktionskosten sind dadurch nicht automatisch portiert. **Maßgeblich ist jetzt [der verifizierte PIT-Ursachennachweis](P003-CPU-TIMING-ORIGINAL-PIT-CLOCK-FOUR-WIN95-SESSIONS-20261117.md).**


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

## Neuer großer Originalbefund: 2.404.877 Original-CPU→EMU-Schreibzugriffe direkt verbunden (17.11., ca. 03:25 UTC)

**Originalaufnahme A:** `P003-20261116T184835Z-c56deac6-ERGEBNIS.zip`, SHA256 `6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944`.  
**Originalaufnahme B:** `P003-20261117T010413Z-9c86c813-ERGEBNIS.zip`, SHA256 `28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c`.

In beiden **authentifizierten, privaten** Archiven sind komplette `Original-CPU-IO.trace`-Portzeitstempel und zugehörige `Original-Register.trace`-Frames enthalten. CPU- und Chipprotokoll wurden streng **in derselben physikalischen Write-Reihenfolge und mit identischem (Port, Wert) pro Ereignis** verknüpft. Der CPU-Trace enthält Write/Read, das Chipregisterprotokoll enthält Write/Read; für diese Prüfung wurden nur die Original-**Writes** beider Quellen einzeln gepaart. **Keine angenommene Zuordnung per gleicher MIDI-Note, Stimme oder Zeitstempel.**

| Authentischer Lauf | CPU→EMU Port-/Wert-Matches | Falsch verknüpfte Originalwrites |
|---|---:|---:|
| A (16.11.) | **1.201.218** | **0** |
| B (17.11.) | **1.203.659** | **0** |
| Gesamt | **2.404.877** | **0** |

### Strikter Zeittakt-Baseline-Test und unabhängiger Holdout

Der *reine diagnostische* Modellansatz verwendet ausschließlich eine globale ganzzahlige Phasen-/Epochengrenze `P`:

```text
vorhergesagter_emu_frame = floor((original_guest_tsc * 44100 + P) / 133333333)
```

`P` wurde **nur** an Lauf A so gewählt, dass möglichst viele echte Original-Write-Frames getroffen werden (`P=132868932`, reiner auf diesen Originalstart bezogener Epochennumerator; **kein** verifizierter VST3-Host-MIDI-Startoffset, kein x86-Kostenmodell). Der Parameter wurde für Lauf B **nicht nachgestellt**:

| Kontrolle | Exakt auf Original-EMU-Frame | Um genau einen Frame falsch |
|---|---:|---:|
| A, Kalibrierung | **1.200.730/1.201.218** | **488** |
| B, unveränderter A-Parameter (Holdout) | **1.203.142/1.203.659 = 99,957 %** | **517** |
| B, *nur der tatsächlich abweichende zweite Laufabschnitt* | **1.154.488/1.154.901 = 99,964 %** | **413** |

Wichtig: Beide Originalaufnahmen beginnen mit **48.758 identischen CPU-/EMU-Schreibereignissen**, bevor sie divergieren. Deshalb ist der getrennte Holdout des divergierenden Restes ausgewiesen. **Dies ist keine unabhängige andere Gaststartphase und kein C4/D4-Noten-CPU-Kosten-Holdout**. Die beiden Original-V5-Läufe haben vielmehr denselben initialen CPU-/Chip-Epochenbezug und unterscheiden sich später bei der wirklichen Creative-Stimmenmodi-Ausführung.

Im gesamten B-Holdout verteilen sich die 517 Fehler auf **344 verschiedene Gast-TSC-Werte**, also teilweise mehrere zusammengehörige Portwörter pro fehlklassifiziertem Grenzzeitpunkt. Alle Fehlzuordnungen betreffen einen benachbarten Audioframe, nicht verlorene oder inhaltlich andere Registerwrites. Die Abweichungen von einer einheitlichen absoluten Phase an den äußeren Zeitgrenzen entsprechen höchstens wenigen CPU-Zyklen; die Spannweite der gesamten `P`-Konsistenzintervalle überschreitet einen Nominal-EMU8000-Sampletakt von `133333333/44100≈3023,43` CPU-Zyklen lediglich um **rund 9,77 Takte** (ältere Session) beziehungsweise **9,46 Takte** (neuere). Deshalb *kann* keine einzelne konstante `P` alle Original-Writes perfekt erklären.

Der obere Rand und der untere Rand der Fehlzuordnungen kommen häufig an verschiedenen ursprünglichen Caller-Stellen vor (z. B. `0xC117C011`, `0xC11791A4`, `0xC10190F7`). **Das ist eine exakte Lokalisation für künftige Timer-/Dynarec-Instrumentierung**, aber noch kein Nachweis, welcher interne Zeitabgleich diese einzelnen Grenzfälle verursacht.

**Reproduzierbarer Read-only-Auditor:** [SHA-gepinnte Original-CPU/EMU-Frame-Kreuzkorrelation](../tools/p003_audit_original_cpu_chip_phase_holdout_private.py). Er verwirft falsche/private ZIPs, verknüpft ausschließlich exakt passende originale Portdaten, optimiert die ganzzahlige Phase nur auf A und berichtet auf B gesondert die 48.758 gemeinsame Präfixevents sowie den divergenten Holdout. Es gelangen **keine Originaldaten** in das öffentliche Repo.

### Original-86Box-Quelle zeigt die **richtige Zeitarchitektur**

Der originale, unveränderte `turican0/86Box-AWE32`-Fork in Release `v6.0-AWE32-0.93` verarbeitet das Audio **nicht** durch eine einfache direkte Formel aus jedem Port-Ereignis-TSC:

- `src/sound/sound.c`: `wavetable_poll()` führt `timer_advance_u64(&wavetable_poll_timer,wavetable_poll_latch)` aus und erhöht `wavetable_pos_global` um genau ein Sample. `sound_speed_changed()` berechnet die Latch-Dauer aus dem originalen `TIMER_USEC` und `WT_FREQ=44100`.
- `src/include/86box/timer.h`: `timer_advance_u64` akkumuliert die Sampleperiode mit **32 ganzzahligen + 32 fraktionalen CPU-Zyklusbits** und echter Übertragsbehandlung.
- `src/timer.c`: `timer_process()` ruft **alle fälligen** Timercallbacks bei `timer_target <= tsc` auf.
- `src/cpu/386_dynarec.c`: `exec386_dynarec()` gleicht nach CPU-Codeblöcken `cycles` gegen `tsc` ab und verarbeitet anstehenden Timerfortschritt an Ausführungspunkten. Der Portcallback kann **vor** diesem Blockabschluss stattfinden.
- `src/sound/snd_emu8k.c`: `emu8k_update()` holt die Samplezeit bis `wavetable_pos_global` nach; `emu8k_trace_advance(num_samples)` erhöht die Originalchip-Framezahl; die folgenden echten Registerwrites bekommen diesen Frame.

Der relevante kausale **Mechanismus** von Original-Source-Timer + 32:32-Wavetable-Clock + CPU-Blockabgleich ist somit belegt. **Noch nicht bewiesen** ist, dass er genau alle 488/517 beobachteten Grenzfehler verursacht; dafür braucht es einen Originaltrace mit **Wavetable-Timer-Due-/Callback-Zeitpunkt und tatsächlichem CPU-Dynarec-Blockende**, korreliert mit den originalen EMU-Portcallbacks.

### Bereits ausführbarer, von reinen Schätzkosten getrennter Timingbaustein

Der P003-Forschungszweig enthält jetzt [P003OriginalWavetableTimer.h](../src/creative_compat/P003OriginalWavetableTimer.h) und [C++23-Implementierung](../src/creative_compat/P003OriginalWavetableTimer.cpp). Das ist die **originalquellgetreue 32:32-Wavetable-Zeitbasis** als gesonderter, heapfreier C++23-Timer: Initialisierung nur mit verifiziertem Quelle-Initialtimer, Original-Latch, Sampleindex und CPU-TSC; Nachholen aller fälligen Wavetable-Callbacks ausschließlich am angegebenen CPU-Blockabschluss; gleiche Ganzzahl-/Fractional-Carry-Logik wie 86Box; fail closed bei ungültigen Eingaben/Rückwärtszeit.

Der unabhängige [Unit-Test für Übertrag, Block-Boundary und 3.000 Sampleperioden](../tests/p003_original_wavetable_timer.cpp) und die bestehende komplette P003-Suite sind in [GitHub-CI #44011312519](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44011312519) **PASS**. Diese neue Komponente ist **bewusst noch nicht produktiv ins VST3 eingebaut**, denn ihre Voraussetzung – die ursprüngliche CPU-Block-Zeitordnung und der authentifizierte Timer-Anfangszustand – ist im normalen Plugin bislang nicht hergestellt.

### Konsequenz für die Fertigstellung

Dies grenzt die wichtige Integrationslücke jetzt enger ein: Zuerst **originale CPU-Instruktions-/Blockkosten + 32:32-Wavetable-Timer-Semantik** selbstständig nachvollziehen, dann Original-Registerwerte im korrekt getakteten Chip liefern. **Nicht** das bereits hervorragende Portwert-Identitätsergebnis oder einen aus den Win95-Daten gefitteten `P` als Lösung des autonom noch fehlenden Win95-Timingproblems ausgeben. Für die bitgenaue VST3-Endabnahme muss eine definierte Host-MIDI-Phase mit identischem, unabhängig reproduzierbarem CPU-/Chip-Startzustand verwendet werden.

Keine Entwicklerunterbrechung, keine PR-Einzelkommentare, keine Änderungen an P005, Original-Creative-Binärcode oder EMU8000-DSP.

## Rückverfolgbare, öffentliche Quellen

- [P003 autonomer Kandidat mit echten 324-Port-Holdouts](P003-AUTONOMOUS-NATIVE-PORT-TIMING-CANDIDATE.md)
- [Doppelsitzungs-Modusgate und offene +3-Zyklen](P003-ORIGINAL-CREATIVE-STEREO-MODE-GATE-20261117.md)
- [Bisherige TSC-/Dynarec-Phasenanalyse und Instrumentierung](P003-ORIGINAL-DYNAREC-14-CYCLE-PHASE-RESEARCH-20261117.md)
- [Autonomer experimenteller CPU-Scheduler](../src/creative_compat/CreativeAutonomousOutTiming.cpp)
