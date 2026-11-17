# P003 – Original Creative Modus 2: Nur CCCA erforderlich, kein vollständiger Chip-Snapshot

Datum: 17.11.2026. **Unabhängiger C++23-Quellvertrag und Negativtest, kein zweiter Treiberport, kein VST3-Release.** Der auf der Entwicklerseite laufende unabhängige REA-Weg bleibt unangetastet.

## Originalquelle, aus privat extrahiertem SBAWE.VXD LCOD

```asm
+41D2: movzx eax,word ptr [ebx-0Ah]  ; Originaltreiber-Marker
+41D9: cmp eax,2000h
+41DE: jae +424F                      ; für normales Scoring unzulässig
+41E0: ...                            ; nur darunter: VTFT lesen
+421F: ...                            ; evtl. DCYSUSV lesen
+424F: cmp dword ptr [ebx],0          ; CACHED sample_end im Treiber-RAM
+4252: je +4269                       ; wenn null: KEIN CCCA nötig
+4254: ... call +3D44                 ; sonst NUR CCCA aus dem EMU8000
+4260: and eax,00FFFFFFh
+4265: cmp [ebx],eax
+4267: jbe +42A2                      ; sample end erreicht, sofort wählen
+4269: add ebx,20h                    ; sonst nächste Stimme
```

Der relevante Fall ist `marker>=0x2000` und `cached sample_end>0`. **Das Original benötigt hier nur den CCCA-Wert, nicht VTFT oder DCYSUSV.** Die Treiber-RAM-Werte Marker und Endadresse müssen bereits aus authentifizierter Originalquelle vorliegen. Ist die CCCA-Lesung **nicht** nachgewiesen, muss die C++23-Routine dagegen weiterhin fehlschlagen.

## Gegenvergleich mit tatsächlich kompiliertem Entwicklercode

Version `940098bd3e35f5b2b2b7c94ec9283b5b45067990`, Entwicklerzweig `develop/p002-creative-register-oracle`. `OriginalWin95AllocatorVoiceSnapshot.complete` steht dort für einen **vollständigen** Chip-Snapshot und kann die lediglich benötigte CCCA-Lesung nicht separat ausweisen. Das ursprüngliche `mode2RangeWithSourceStatus` prüft `if(!v.complete) return std::nullopt` und verwirft deshalb auch gültige Quellzustände.

Test mit Modus 2, Range [0,2), `mode_count=32`:

- Voice 0: `marker=0x2000`, `stored_sample_end=100`, **CCCA ausdrücklich separat verifiziert**, kein vollständiger VTFT/DCYSUSV-Snapshot.
- Voice 1: `marker=0x0FFE`, vollständig gemessener VTFT highword 0, genuine freie Stimme.
- Fall **CCCA=100**: Original beendet die Suche sofort mit Voice **0**, Grund `SampleEndReached`.
- Fall **CCCA=99**: Original sucht weiter und wählt Voice **1**, Grund `CompletelyIdle`.
- Fall **CCCA-Verifikation fehlt**: Korrekte Routine muss **fehlschlagen**, nicht einen unbekannten Hardwarewert erfinden.

**Kompilierter unabhängiger Nachweis:**
- [C++23-Regressionsprüfung](https://github.com/challanger2000/125A-AWE64-Gold-VST3/blob/research/p003-v5-voice-row-capture/tests/p003_original_mode2_ccca_only_validated.cpp)
- [GitHub Actions – Forschungskorrektur PASS, echte P002-Revision FAIL (Fehler reproduziert)](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44001687844)

Log-Ausgaben: `P003_MODE2_CCCA_ONLY_SOURCE_CONTRACT_PASS` und `P003_P002_HEAD_MISSING_CCCA_ONLY_PARTIAL_SNAPSHOT_REPRODUCED` mit P002-Commit-Hash.

## Eng begrenzte Forschungskorrektur

Auf `research/p003-v5-voice-row-capture` wurde ein **eigenes Verifikationsbit `ccca_read_verified`** in die Forschungssnapshotstruktur eingeführt. Bei `marker>=0x2000` und `stored_sample_end>0` akzeptiert der Forschungsallocator **nur** einen vollständigen Chip-Snapshot oder dieses explizite CCCA-Beweisbit. Er prüft genau das reale Sample-Ende und kann andernfalls zur nächsten Stimme übergehen. Die übrigen Originalpfade benötigen weiterhin die entsprechenden originalen Hardwarewerte.

**Keine Empfehlung**, CCCA allein aufgrund eines Zahlenfelds als gemessen anzunehmen. Bei einem richtigen Treiberport ist eine pro Register getrennte Provenienz (VTFT / DCYSUSV / CCCA) sinnvoll, weil der Originaltreiber je nach Marker/Status nur bestimmte Werte ausliest. Es reicht nicht, den booleschen `complete`-Wert ohne eine beweisbare Original-Lesung zu umgehen.

## Abgrenzung

Die vorherigen zwei P003-Abweichungen (gesperrter Modus-1-Nachbar, Modus-2-Marker>=0x2000 und Cached-Ende null) sind **separat** nachgewiesen und werden im selben CI-Workflow unverändert erneut geprüft. Die aktuelle neue Abweichung ist eine **zu grobe Validierungstiefe der Forschungs-Snapshot-API**; sie beweist noch nicht, dass eine produktive VST3-Instanz genau diesen Zustand bereits erreicht, und **ersetzt keine** Original-Win95-gegen-VST3-PCM-Abnahme. Keine privaten Creative-Binärdaten, ROM, SF2 oder Gastspuren in GitHub. Keine Änderung an P005.
