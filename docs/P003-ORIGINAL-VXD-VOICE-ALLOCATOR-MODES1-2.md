# P003 — Originale SBAWE.VXD-Stimmenallokation: Modus 1 und 2

Stand: 16. November 2026, auf `develop/p002-creative-register-oracle`.

Die unabhängige statische Originalcodekontrolle
[PR #4, issuecomment-6904224030](https://github.com/challanger2000/125A-AWE64-Gold-VST3/pull/4#issuecomment-6904224030)
und eine eigene Gegenprüfung des privaten SHA-gepinnten Original-LCOD
(`SBAWE.VXD`) bestätigen den dreiarmigen Dispatcher bei LCOD+0x3D6F.
**Originalbytes/ROM/Bankdaten wurden nicht eingecheckt.**

| Original LCOD | Verhalten | Native Implementierung |
|---|---|---|
| +0x3EA1, Modus 0 | Drei ineinandergeschachtelte Einzelstimmengruppen | Bereits `CreativeVxdMonoVoiceRanking` (separate, bisherige Tests) |
| +0x3FE8, Modus 1 | Absteigende, benachbarte **Stimmenpaare**; individuelle Status-/VTFT-/DCYSUSV-/CCCA-Wertung | Neu `CreativeVxdAdditionalAllocatorModes`: `candidateOriginalWin95Mode1Pair` |
| +0x416C, Modus 2 | Aufsteigende Bereichssuche; Start `packed & 0xFF`, Ende `min(packed>>8, device+0x06)` | Neu `candidateOriginalWin95Mode2Range` |

### Modus-1-Wertung

Der private originale x86-Code berechnet aus dem vorherigen und aktuellen
individuellen Voice-Ranking:

`score = previous + current + 2*max(previous,current)`

und addiert `0x0C00`, falls das originale Statusbit
`[device+0x4C+voice*0x20+0x10] & 1` gesetzt ist.
Die umgekehrte Schleifenreihenfolge und die **unsigned-tie-Auswahl**
sind im C++-Modell erhalten. Wenn zwei benachbarte Einzelwerte 0
sind, beendet der Originalcode die Suche frühzeitig.

Die native, typisierte Teilroutine erzeugt außerdem die vier
beobachteten Hardware-Portwörter der paarweisen Freigabe über
den originalen 16-Bit-I/O-Writer an LCOD+0x414A/+0x4158:
`E22:0xA0|v (8-bit), A20:0x00FF (16-bit)` und danach
`E22:0xA0|(v+1), A20:0x00FF`.

### Modus-2-Wertung

Der Bereich wird exakt als **untere inklusive und obere exklusive**
Grenze behandelt, letztere durch `device+0x06` begrenzt. Die native
Teilroutine erhält die ursprünglichen Marker- und Statuszuschläge
`0x1200/0x0200/0x0300/0x1300`, den sofortigen Leerlauf-Treffer
(`marker==0x0FFE` bei VTFT-Highword 0), das zusätzliche
Sample-End-Kriterium auch bei für das normale Scoring unzulässigen
Markern sowie die ursprüngliche **aufsteigende Gleichstandsregel**.
Ein Treffer führt zum originalen einzelnen Writer-Muster
`E22/A20`.

### 30 versus 32 Stimmen

Der Originaltreiber liest `device+0x06` als physische Suchgrenze
(ursprünglicher Gastzustand: 32). Einige bislang geprüfte
Native-Music-Allocator-Module begrenzen unabhängig davon auf
30 musikalische Stimmen. **Die neue Implementierung erzwingt
keine Gleichsetzung.** Sie akzeptiert explizit die vollständig
verifizierte Gerätestimmenkapazität im Bereich 1–32. Bei
unvollständigen Zustands-/EMU8000-Reads wird fehlgeschlagen, ohne
eine vermeintlich freie Stimme zu erfinden.

### Tests und Abnahmegrenze

`tests/p003_original_vxd_additional_allocator_modes.cpp` prüft
u. a. Modus-1-Paarwertung, Penalty, umgekehrte Reihenfolge,
Tie-Break, Sample-Ende, fehlende Kandidaten, 30/32-Kapazität,
Port-Reihenfolge und -Breite; Modus-2-Bereichsgrenzen, Status-Gates,
Gleichstände, vorherige Live-Read-Abhängigkeit und ungültige
Zustände. Test über `ctest` in den regulären CI-Jobs.

**Wichtig:** Das ist eine aus Original-x86-Kontrollfluss und
synthetischen Eingaben abgeleitete, separat kompilierte
**Auswahl-/Portplan-Kandidatenimplementierung**. Sie ist noch
**nicht** mit der produktiven VST3-MIDI-Pipeline verbunden, denn
dort fehlen die unabhängig bestätigte Original-Descriptor-Moduswahl,
vollständige Live-VXD-/Chip-Zustände und die genuine CPU-Zeitplanung.
Kein eigenständiger Windows-95-gegen-VST3-PCM-Beweis, kein PR-Merge.
Der fest gepinnte EMU8000-Kern und der bisher bitidentische
source-clocked Referenzmodus bleiben unverändert.

**Nächster Nachweis:** Die neue V4-Gastmessung mit echten
Original-Descriptorflags, paarweiser Überlappung, Voice-Stealing
und Bereichsallokation analysieren; daraus verifizierte
Original-Live-Zustände und die tatsächliche Register-/Release-
Reihenfolge für den native VST3-Scheduler ableiten. Bis dahin
P003 autonome Treiber-/PCM-Parität weiterhin **NO-GO**.
