# P003 – Original-Creative-Modus 2: registerselektive Live-Lesungen (VTFT / DCYSUSV / CCCA)

**17. November 2026 · unabhängige Zuarbeit für den C++23-Entwickler, keine zweite Treiberportierung.** Quelle: Original `SBAWE.VXD` aus der privaten Win95-VM, originales LCOD-Disassembly (Treiber-SHA256 `d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`). Keine Creative-Binärdatei, ROM, SF2 oder Original-Trace in GitHub abgelegt.

## Belegter Original-Kontrollfluss

```asm
LCOD+41D2: movzx eax,word ptr [ebx-0Ah]     ; driver marker
LCOD+41D9: cmp eax,2000h
LCOD+41DE: jae +424F                        ; ineligibel -> ggf. NUR CCCA
LCOD+41E0..+41E6: call +3D44               ; VTFT lesen
LCOD+41EB: shr eax,10h                     ; VTFT highword
LCOD+41EE: cmp [ebp-10h],0FFEh
LCOD+41F8: je +423B                         ; marker 0FFE: KEIN DCYSUSV
LCOD+41FA: test marker highbyte,10h
LCOD+4210: cmp marker lowbyte,0FFh
LCOD+4214: jne +421F
LCOD+4216: add score,300h                  ; low FF: KEIN DCYSUSV
LCOD+421F..+4228: call +3CE6               ; sonst DCYSUSV lesen
LCOD+423B: cmp score,0
LCOD+423F: je +429D                        ; 0FFE/VTFT=0: SOFORT frei,
                                          ; keine CCCA-Lesung mehr
LCOD+4241..+424C: Kandidatenwertung
LCOD+424F: cmp dword ptr [ebx],0           ; cached sample_end
LCOD+4252: je +4269                        ; Ende 0: KEIN CCCA
LCOD+4254..+425B: call +3D44               ; sonst CCCA lesen
LCOD+4260: and eax,00FFFFFFh
LCOD+4265..+4267: cmp cached_end,CCCA; jbe +42A2
LCOD+4269: add ebx,20h                     ; nächste Stimme
```

Damit gibt es **keine allgemeine Voraussetzung**, alle drei EMU8000-Register vor jeder Stimmenwertung gelesen zu haben. Das Original benötigt die folgenden **jeweils tatsächlich gemessenen** Register, abhängig vom Quellzustand:

| Originalzustand | Benötigte EMU8000-Lesungen |
|---|---|
| `marker>=0x2000`, `cached_end=0` | keine |
| `marker>=0x2000`, `cached_end!=0` | nur CCCA |
| `marker=0x0FFE`, `VTFT_hi16=0` | nur VTFT; sofortiger Idle-Return |
| `marker=0x0FFE`, `VTFT_hi16!=0`, `cached_end=0` | nur VTFT |
| `marker!=0x0FFE`, low byte `0xFF`, `cached_end=0` | nur VTFT; der originale Status-Read entfällt |
| `marker!=0x0FFE`, low byte `!=0xFF`, `cached_end=0` | VTFT und DCYSUSV |
| Nicht vorzeitig beendeter, berechtigter Scoring-Pfad mit `cached_end!=0` | vorgenannte Lesungen sowie CCCA |

Unbekannte Live-Werte müssen weiterhin **fail closed** sein, wenn der Originalpfad sie verwendet. Der Treiber-RAM-Zustand (Marker, cached sample end, blocked mode) muss ebenfalls unabhängig verifiziert sein.

## Implementierte enge, unabhängige Forschungsreferenz

In `src/creative_compat/CreativeVxdAdditionalAllocatorModes.h` gibt es **zusätzlich** zu `complete` drei explizite, standardmäßig falsche Provenienzbits `vtft_read_verified`, `dcysusv_read_verified` und `ccca_read_verified`. Für Modus 2 überprüft der Forschungsallocator jetzt genau die wirklich benötigten Einzelregister; bei `complete=true` bleibt die bereits vorhandene Vollständigkeitssemantik erhalten. Keine neuen Zeitkonstanten, keine EMU8000-Änderung, keine implizit erfundenen Read-Ergebnisse. Der Modus-1-Code wurde durch diese Erweiterung nicht verändert.

[Direkt ausführbarer C++23-Regressionstest](https://github.com/challanger2000/125A-AWE64-Gold-VST3/blob/research/p003-v5-voice-row-capture/tests/p003_original_mode2_vtft_dcysusv_provenance.cpp) prüft:
- `marker=0FFE, VTFT_hi16=0`, cached end auch ungleich 0: sofort `CompletelyIdle`, **nur VTFT**.
- `marker=0FFE, VTFT_hi16=5`, cached end null: Scoring mit **nur VTFT**.
- `marker=01FF`, VTFT hi16=5, cached end null: Score **5 + 0x200 + 0x300 = 1285**, kein DCYSUSV.
- `marker=0100`: ohne verifizierte DCYSUSV-Lesung ausdrücklich **NO-GO**; mit dieser Lesung Score **517**.
- cached end ungleich null nach nicht-idlem Score: ohne verifiziertes CCCA ausdrücklich **NO-GO**, mit CCCA Sample-End-Kurzschluss.
- VTFT-Verifikation danach entfernen: ausdrücklich **NO-GO**.

**GitHub-CI vollständig PASS:** [Run #44003976058](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44003976058) kompiliert die komplette frühere Modus-0/1/2-Regressionssuite sowie alle bisherigen P003-Original-Negativfälle und die neue registerselektive Prüfung. Log: `P003_MODE2_VTFT_DCYSUSV_CCCA_SOURCE_PROVENANCE_PASS`.

## Gegenprobe gegen *echten*, unveränderten Entwickler-C++23-Code

Gepinnter Entwicklungs-Commit `940098bd3e35f5b2b2b7c94ec9283b5b45067990` – Quellfunktion `mode2RangeWithSourceStatus` in `src/creative_compat/CreativeVxdAdditionalAllocatorModes.cpp`. Er setzt `!v.complete` pauschal mit „fehlende erforderliche Daten“ gleich. Die **erste** Prüfung (nur VTFT, Original-Marker `0FFE`, VTFT highword 0) liefert `std::nullopt` statt `CompletelyIdle`. Exakter CI-Nachweis: `P003_DEV_INCOMPLETE_VTFT_ONLY_SOURCE_SNAPSHOT_REPRODUCED`.

Dies ist eine weitere **Teilzustands-/API-Schwäche** derselben Designklasse wie die vorangegangene CCCA-Einzelregisterprüfung, **kein unabhängig nachgewiesener vierter produktiver Audiofehler**. Bisher **kein** neuer originaler Windows-95-Aufnahmelauf mit genau dem synthetischen Entwickler-Testzustand. Keine P005-Änderung und kein Merge in produktiven VST3-Code.

## Konkrete Entwicklerempfehlung

Im C++23-Treiberadapter die Provenienz pro Original-Hardwarelesung bzw. den tatsächlich ausgeführten Quellpfad abbilden. Nicht aus der bloßen Existenz eines numerischen Registerfelds eine erfolgreiche Lesung ableiten, nicht alle Register auf Verdacht voraussetzen und insbesondere **vor einem Idle-Return keine CCCA-Anforderung auslösen**. Der direkte REA-Chat kann denselben Quellkontrollfluss unabhängig übernehmen.
