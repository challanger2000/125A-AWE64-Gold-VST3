# P003 – CPU-TSC-Phasen und Dynarec-Blockverfolgung (17.11.2026)

> **WICHTIGE KORREKTUR / NEUER ORIGINALNACHWEIS (17.11.2026):**
> Die nachstehend dokumentierte 14-Takt-Differenz ist als Messung richtig, war
> aber **nicht als Ursache eines 86Box-Dynarec-Fehlers nachgewiesen**. Ein
> zweiter Original-Win95-Lauf (`P003-20261117T010413Z-9c86c813`) belegt,
> dass das echte Creative-Schwellfeld `[ESI+0x18]` im älteren stabilen Scan **4**
> und im neuen stabilen Scan **2** beträgt. Bei `<=2` durchläuft die **originale
> SBAWE.VXD** nachweislich zusätzliche Nachbarstimmen-Prüfungen bei
> `LCOD+4029..4042`. Diese Original-Kontrollflussdifferenz ist der aktuelle
> Erklärungsansatz für +14-Takte und für die neun Fälle mit zwei übersprungenen
> Stimmen (+50 Takte). Der daraus abgeleitete, klar begrenzte Vergleich trifft
> **479/486** stabile Originalintervalle aus beiden Sessions; sieben neue
> +3-Takt-Restabweichungen bleiben offen. Siehe
> [revidierte Originalanalyse](P003-ORIGINAL-CREATIVE-STEREO-MODE-GATE-20261117.md).
> **Bitte keinen session-spezifischen 14-Takt-Offset in produktives C++23 oder
> P005 übernehmen.** Die Dynarec-Blockbeobachtung bleibt optionales Diagnosemittel.


## Authentische Eingangsdaten und Grenzen

Private Original-Win95-Aufnahme des Eigentümers: `P003-20261116T184835Z-c56deac6-ERGEBNIS.zip`, SHA-256 `6ea6b3a28339a262926bc083c99170428676eb983cfe6410b8d28a32675c9944`. Ihre privaten Creative-Treiber-, Registerspur-, WAV- und SoundFont-Daten **werden nicht im öffentlichen Repository gespeichert**.

Die vorhergehende unabhängige P003-V5-Auswertung hat 25 echte Stereo-Allokationen, 481 originale CCCA-Lesungen mit vollständigen korrigierten Caller-Frames und 466 unmittelbar aufeinanderfolgende CCCA→VTFT-Zeitintervalle bestätigt. Im begrenzten, für neun stabile Vollscans kalibrierten Forschungsmodell sind **402/466** Intervalle exakt, **64** weichen ab (alle länger). Dies ist weder eine CPU-Instruktions-Timing-Implementierung noch eine Freigabe für P005.

## Neue unabhängige Prüfung: 14-Takt-Zustandswechsel

Der originalen, SHA-geprüften `Original-CPU-IO.trace` entstammen **1.762** physische Original-Allocator-Helferaufrufe (jeweils **das erste echte E22-Portevent pro Helfer**, nicht jeder der 1,5 Mio Portcallbacks):

| Gast-CPU-Zustand `(tsc + cycles_remaining) mod 666` | Anzahl |
|---|---:|
| 635 | **1.697** |
| 649 | **65** |

**Es gibt in diesen 1.762 Helferstarts keinen dritten Restwert.** Die beide CPU-Zähler verbindenden Phasen liegen exakt 14 Takte auseinander. In den letzten neun vollständigen Original-Allocator-Durchläufen liegt kein einziger Helferstart in Phase 649.

Das mod-666-Verfahren ist auf die private VM-Konfiguration (`pentium_p54c`, 133.333.333 Hz, Dynarec eingeschaltet) begrenzt. Es ist keine Behauptung über reale Creative-AWE64-Hardwarezyklen.

### Präzise D→C-Korrelation

Vor `CCCA` erfolgt im Original im betrachteten Pfad normalerweise ein `DCYSUSV`-Read. Der originale I/O-Wert am Helfer-Rücksprung `LCOD+0x40A7` beträgt bei allen **461 von 461** D-Lesungen **`0x7F7F`**. Der Opcode `LCOD+0x40A7: test ah,0x80` nimmt folglich in diesen Fällen dieselbe Sprungrichtung. Die längeren D→C-Abstände lassen sich **nicht** durch diesen einzelnen Creative-Statuszweig erklären.

| Direkter D→C-TSC-Abstand vor CCCA→VTFT | Fälle | Nachfolgende Abweichungen vom engen Modell | TSC-/Restbudget-14-Takt-Differenz |
|---|---:|---:|---:|
| 70 | **417** | **26** | **1** |
| 84 | **27** | **25** | **25** |
| 75 bzw. 79 | 2 | 2 | 2 |

Die 25 auffälligen 84-Takt-Vorläufe zeigen gegenüber dem Restbudget eine Abweichung von **−14 Takten** nach Normierung um den VM-Budget-Nachschub von 666 Takten. Ein Teil der auffälligen TSC-Intervalle betrifft also den **Abgleich zweier CPU-Zeitvariablen**. Aus den vorhandenen Zählern allein folgt **nicht**, welche genaue Dynarec-Anweisung den Unterschied verursacht.

Auch unter identischen sichtbaren Originalbedingungen (gleiche Stimme, gleiche beobachteten CCCA-Best-/Prioritätszweige und gleiche Nachbarstatusbytes) bleibt der Zusammenhang erhalten: sechs passende Vergleichsgruppen, **16/17** Fehler bei 84-Takt-Vorläufen gegenüber **12/91** bei 70-Takt-Vorläufen. Dies ist eine **beobachtete Assoziation, kein Kausalnachweis**.

In allen **466** direkt aufeinanderfolgenden CCCA→VTFT-Fällen sinkt der echte Voice-Row-EBX-Zeiger genau um **32 Byte**. Unsichtbar übersprungene Stimmen sind dafür ausgeschlossen.

## Originale 86Box-CPU-Mechanik

Der tatsächlich verwendete `turican0/86Box-AWE32`-Fork `v6.0-AWE32-0.93` führt CPU-Blöcke in `src/cpu/386_dynarec.c` aus und gleicht anschließend `cycles` und `tsc` an. Die Abrechnungsstellen `exec386_dynarec`, `update_tsc` und die Pentium-Instruktionspaarung sind in diesen Originalquellen nachzulesen:

- [Original CPU-Block-/TSC-Abrechnung](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/cpu/386_dynarec.c)
- [Original Pentium-Pairing-/AGI-Modell](https://github.com/turican0/86Box-AWE32/blob/v6.0-AWE32-0.93/src/cpu/codegen_timing_pentium.c)

Das Originalmodell kann bereits **während** der Blockausführung den Zeitstempel ändern, und es korrigiert den `tsc` beim Rücksprung auf Blockebene erneut. Der Portcallback kann zwischen diesen Punkten liegen. Die 14-Takt-Phasen sind deswegen ein konkreter Ansatz für eine **Blockausführungsspur**, nicht für zusätzliche feste Kostenkonstanten.

## Neue Forschungsinstrumentierung (noch keine Win95-Messung)

`tools/instrument_p003_86box_dynarec_clock.py`:

1. Prüft den Git-Blob-Hash **`e9765a423b5d675337aab33fd060204e62dc20d7`** des unveränderten Original-CPU-Quelltexts. Jede unbekannte Revision scheitert geschlossen.
2. Patcht nur eine **Kopie** der Datei `src/cpu/386_dynarec.c`. Original-EMU8000-Kern, Creative-Win95-Gasttreiber, VST3, VM-Image und DSP bleiben unberührt.
3. Pro betroffenem CPU-Block werden der Eintritts-PC, Austritts-PC, CPU-Budget vor/nach Ausführung, TSC vor/nach Ausführung und ein zusätzlicher TSC-Abgleich-Zeitpunkt aufgezeichnet.
4. Der Blockpfad unterscheidet **neu übersetzt / rekompiliert / Cache-Ausführung / Interpreter**. Die Erfassung erfolgt nur auf LCOD-Allocator- und I/O-Helfer-Pfaden, wenn **beide** Umgebungsvariablen `AWE64_P003_DYNAREC_BLOCK_TRACE` und `AWE64_P003_DYNAREC_LCOD_BASE` gesetzt sind.
5. Ein eigener Windows-GitHub-Actions-Build überprüft die quellgebundene CPU-Instrumentierung zusammen mit dem bereits getesteten V5-Caller-Frame und dem unveränderten originalen EMU8000-Chipkern.

**Nächste unabhängige technische Prüfung:** Den Unterschied zwischen TSC- und CPU-Budgetphase auf den tatsächlich ausgeführten **86Box-Dynarec-Blockpfad** (Cache/Neuübersetzung) und seine originale Taktbelastung zurückführen. Erst eine tatsächliche Guest-Ausführung mit diesem zusätzlichen Log kann diese Kausalfrage klären. Weder alte Timestamps noch 14 künstliche Zyklen werden in das native P005-VST3 übernommen.
