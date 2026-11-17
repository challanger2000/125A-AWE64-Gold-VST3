# P003 – CPU-Timing: Ursache der Sample-Phasenfehler nachgewiesen, 4.366.834 Originalregister-Writes exakt

**17.11.2026 · CPU-Timing priorisiert · Original-X86/86Box-Quellen und vier private Win95-Originalaufnahmen · Forschungszweig, keine Einzelübergabe.**

## Das technische Ergebnis

**Der ursprüngliche Vergleich mit `133333333` CPU-Takten/s verwendete die nominale CPU-Tabellenzahl anstelle der tatsächlich von 86Box berechneten Wavetable-Timer-Zeitbasis.** Der originale Quellcode stellt eine Pentium-133-Frequenz mit einem Drittel-Takt explizit wieder her.

Originalquelle (unveränderter `turican0/86Box-AWE32`-Fork, Tag `v6.0-AWE32-0.93`):

1. [`src/cpu/cpu_table.c`, Pentium P54C 133](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/cpu/cpu_table.c#L3831-L3846) trägt `rspeed=133333333` als **ganzzahlige Nennfrequenz** ein.
2. [`src/86box.c`, `pc_speed_changed()`](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/86box.c#L1369-L1375) ruft tatsächlich `pit_set_clock(cpu_s->rspeed)` auf.
3. [`src/pit.c`, `pit_set_clock()`](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/pit.c#L1150-L1164) behandelt einen `clock % 100000000 == 33333333` **absichtlich gesondert** und ersetzt den abgerundeten Rest durch `100000000.0 / 3.0`.
4. Damit ist die **originale TIMER_USEC-Zeitbasis** `cpuclock = 133333333.333333333... = 400000000 / 3 Hz` – **nicht** die nominale Tabellenspalte `133333333 Hz`.
5. [`src/pit.c` `TIMER_USEC`](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/pit.c#L1242-L1247) rechnet dieses korrigierte `cpuclock` ausdrücklich in das originale 32:32-Timerformat um.
6. [`src/sound/sound.c`, `sound_speed_changed()`](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/sound/sound.c#L737-L744) erstellt daraus bei `WT_FREQ=44100` die Wavetable-Periode.
7. [`src/include/86box/timer.h`, `timer_advance_u64()`](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/include/86box/timer.h#L89-L104) übernimmt die 32:32-Fraktion einschließlich Übertrag zum nächsten Sample.

**Wörtlich aus der Original-Quellrechnung, ohne angepasste Koeffizienten:**

```text
P54C CPU table nominal rspeed             133333333
original pit_set_clock corrected cpuclock  400000000/3 Hz
original 86Box TIMER_USEC 32:32            572662306133  (0x8555555555)
original wavetable_poll_latch 32:32        12985539821609 (0xBCF6E7D0029)
nominal 44.1-kHz sample period             3023.431594858... guest CPU cycles
```

## Exakte echte Win95-Protokollgegenprobe: **vier** SHA-geprüfte Sessions

Die vier echten `P003-ERGEBNIS.zip`-Archive wurden in der Arbeitsumgebung aus dem Nutzerpaket geöffnet. Die ZIP-SHA-256 wurde je Sitzung vollständig geprüft. Jeder originale **CPU-Port-Schreibzugriff** wurde physisch in Write-Reihenfolge und mit exakt gleichem `(Port,Wert)` an das originale **EMU8000-44,1kHz-Registerprotokoll** gebunden. **Keine WAV-, Treiber-, SoundFont- oder Port-Rohdaten sind auf GitHub veröffentlicht.**

Die unabhängige erste Sitzung `P003-20261116T173917Z-bf200af3-ERGEBNIS.zip` bestimmt EINEN auf die originale VM-Start-Epoche bezogenen Wert `P` ohne Wissen aus den drei späteren Aufnahmen. Die drei späteren Sessions sind **vollständige Holdouts**, `P` wird **nicht** nachgestellt.

**Genaue ganzzahlige Quellzeitformel:**

```text
EMU_Frame = floor((original_guest_TSC * (44100 * 3) + P) / 400000000)
                            sample frequency   CPU clock 400MHz/3
P = 400132699, einmalig nur aus Original-Erstaufnahme bestimmt
```

Für eine Originalreferenz mit derselben zuvor gespeicherten Win95-VM-/CPU-/Chip-Epoche ist `P` gültig. **Dies ist ausdrücklich kein allgemein gültiger MIDI-Startphasenwert, der hart in ein VST3 eingebaut werden dürfte.**

| Original-Win95-Sitzung | Original CPU→EMU Port/Wert-Paare | Mit erster Phase exakt korrekt | Fehler |
|---|---:|---:|---:|
| **16.11., 17:39** – einzige Kalibrierung | 760.209 | **760.209** | **0** |
| **16.11., 17:56** – unabhängiger Holdout | 1.201.748 | **1.201.748** | **0** |
| **16.11., 18:48** – unabhängiger Holdout | 1.201.218 | **1.201.218** | **0** |
| **17.11., 01:04** – unabhängiger Holdout | 1.203.659 | **1.203.659** | **0** |
| **Gesamt** | **4.366.834** | **4.366.834** | **0** |

**Original-SHA-256 der vier Quell-ZIP-Archive, in Tabellenreihenfolge:**

- `7def342a694c7e9610dc6dab396e98ddf3cad1ac62f69c895f30b7f4ce34f2c7`
- `cff0c0527cdf13c718621fad6bb23e255188c1f14f3c3ca4f293b9185f250fb7`
- `6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944`
- `28aea68657e48dfaeec8cdd675f01fbab1eee00a771abfa3872a6e97041b543c`

Die **gültigen Phasenintervalle** der einzelnen originalen Sessions sind:

| Original-Session | Untere Grenze für `P` | Obere Grenze für `P` |
|---|---:|---:|
| Erste Kalibrierung | 400132600 | 400132799 |
| Holdout 2 | 400132300 | 400132799 |
| Holdout 3 | 400131800 | 400132799 |
| Holdout 4 | 400132500 | 400132799 |

Der **gemeinsame Schnitt** ist `[400132600,400132799]`. Somit können alle 4.366.834 Ereignisse mit **einem einzigen unveränderten Quell-Zeitursprung** exakt modelliert werden. Nur dieser Quellzeitursprung wurde aus einer Original-Session bestimmt; die **Frequenz selbst stammt aus Original-86Box-Code**, nicht aus Anpassung an Originalframes.

## Eindeutige negative Kontrolle gegen den bisherigen falschen CPU-Takt

Wird anstelle des echten Original-Quellwerts `400000000/3` wieder `133333333` als exakte TIMER-Uhr benutzt und der Phasenwert ebenfalls **nur an der ersten Session** optimal gefittet, bleiben:

| Original-Sitzung | Falsch eingestufte Frames mit `133333333` Hz |
|---|---:|
| Training | **142** |
| Holdout 2 | **501** |
| Holdout 3 | **513** |
| Holdout 4 | **539** |
| Gesamt | **1.695** |

Der vorherige scheinbare Timer-/Dynarec-Grenzfehler von 488 bzw. 517 Writes in den beiden zuletzt untersuchten Archiven war hier also **kein nachgewiesener CPU-Block-Timingfehler**, sondern Folge der ungenauen TIMER-Quellfrequenz. Der Quell-Nachweis widerlegt die frühere Vermutung, man müsse für diese kleinen Framefehler zwingend besondere Dynarec-Blockphasen oder hartkodierte +3/+14-Takte einführen.

**Abgrenzung:** Die sieben eigenständigen CPU-TSC-Abstandsabweichungen bei Modus 1 (Stimme 4) betreffen tatsächlich gemessene **Abstände zwischen x86-Portzeitstempeln** und werden durch diese nachträgliche TSC→Frame-Umrechnung nicht automatisch geklärt.

## Umgesetzte isolierte C++23-Forschungskorrektur

- [`P003OriginalPitClock.h`](../src/creative_compat/P003OriginalPitClock.h) bildet die tatsächliche `pit_set_clock()`-Frequenzreparatur und Original-32:32-Latchberechnung (ohne heuristische Konstanten) ab.
- [`P003OriginalWavetableTimer.cpp`](../src/creative_compat/P003OriginalWavetableTimer.cpp) kann den so ermittelten Latch mit 32:32-Fractional-Carry und echten CPU-Blockabschlusszeitpunkten benutzen. Der ältere Timer-C++-Test wurde von der **gerundeten** auf die **richtige** Quellperiode korrigiert.
- [`CreativeAutonomousOutTiming.cpp`](../src/creative_compat/CreativeAutonomousOutTiming.cpp): **Nur im isolierten Forschungszweig** verwendet die bisher experimentelle, standardmäßig deaktivierte autonome Note-Scheduler-Variante die genaue rationale CPU-/EMU-Frame-Division mit `400000000/3` statt `133333333`. **Die bisher empirischen Pentium-/Creative-OUT-Instruktionskosten bleiben als solche ungeprüft**; es wird kein Timing-MATCH behauptet.
- [`tests/p003_original_pit_fractional_clock.cpp`](../tests/p003_original_pit_fractional_clock.cpp) verifiziert exakte Original-32:32-Werte, unveränderte fraktionale Langzeitperiode und realen Timer-Übertrag. Die alten Timing- und Allocator-Tests laufen weiterhin.
- [Privater, SHA-pinnender Original-Vierfachauditor](../tools/p003_audit_pit_clock_four_win95_sessions_private.py) reproduziert direkt aus allen vier Original-ZIPs Port/Wert-Bindung, Phasenintervall und alle exakten CPU→EMU-Frame-Holdout-Statistiken. Kein User muss etwas für den Entwickler weiterreichen.

## Was damit **erledigt** ist und was **noch fehlt**

**Erledigt:** Originale Zeitbasis des 133-MHz-Pentium-Wavetable-Timers aus dem realen 86Box-Quellcode rekonstruiert und an **4.366.834 echten Originalwrites ohne Frameabweichung** in vier unabhängig ausgeführten Originalaufnahmen bestätigt. Die **früheren Rundungs-/Langzeit-Framefehler sind erklärbar und beseitigt**.

**Noch nicht erledigt:** Für neue MIDI-Events autonom den Original-x86-Programmablauf bis zu jedem Port-OUT mit dem richtigen Gast-TSC zu erzeugen, einschließlich Original-Branch-/Pentium-/Dynarec-Zeitkosten. Außerdem muss die native VST3-Startphase als **eigener deterministischer Anfangszustand** definiert werden; sie darf keinen erfassten Gast-P-Wert wiederverwenden. Die separate REA-Portierung arbeitet daran. Ohne diesen Nachweis ist vollständige autonome Win95/VST3-PCM-Parität weiterhin **NO-GO**.

**Kein Entwickler-PR-Kommentar, keine Einzelübergabe, kein Eingriff in P005 und kein Upload privater Gastdaten.**
