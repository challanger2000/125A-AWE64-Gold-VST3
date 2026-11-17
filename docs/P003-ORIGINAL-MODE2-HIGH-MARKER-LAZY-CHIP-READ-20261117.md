# P003 – Original-SBAWE.VXD Modus 2: unzulässiger Marker erfordert keine Live-EMU8000-Lesung

**Stand 17.11.2026.** Unabhängige technische Zuarbeit zum Creative-C++23-Allocator. Kein vollständiger Treiberport, keine Änderung an P005/EMU8000 oder der parallelen REA-Portierung.

## Original-x86-Kontrollfluss: LCOD +41D9 / +424F / +4269

Die originalen, privat SHA-geprüften `SBAWE.VXD`-LCOD-Bytes (Original-SHA256 `d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`) enthalten:

```asm
+41C2: cmp dword ptr [ebp-18h],0
+41C6: jne +41D2
+41C8: cmp byte ptr [ebx+0Dh],0 ; optionales blocked gate
+41CC: jne +4269
+41D2: movzx eax,word ptr [ebx-0Ah] ; driver marker
+41D9: cmp eax,2000h
+41DE: jae +424F                   ; unzulässig für normales Scoring
+41E0: ... VTFT-Live-Read
+421F: ... DCYSUSV-Live-Read
+424F: cmp dword ptr [ebx],0       ; gespeichertes Sample-Ende aus DRIVER RAM
+4252: je +4269                    ; wenn Null: KEIN CCCA-Live-Read
+4254: ... CCCA-Live-Read
+4269: add ebx,20h                 ; nächste physische Stimme
```

Daraus folgt ein enger, **ohne Gast-Laufzeitbehauptung quellbelegter** Satz: Ist die Stimme nicht anderweitig blockiert, `marker >= 0x2000` und `stored_sample_end == 0`, führt Creative **keine VTFT/DCYSUSV/CCCA-Live-Lesung** an der aktuellen Stimme aus und sucht weiter. Fehlt nur der Live-Chip-Snapshot dieser Stimme, darf die Implementierung deswegen **nicht** abbrechen. Die Quelle ihrer beiden bereits verwendeten Driver-RAM-Felder (Marker, gespeichertes Sample-Ende) muss selbstverständlich unabhängig verifiziert sein.

## Negativtest gegen reale Entwicklerrevision

**Unverändert kompilierter Stand**: `develop/p002-creative-register-oracle` Commit `940098bd3e35f5b2b2b7c94ec9283b5b45067990`, `src/creative_compat/CreativeVxdAdditionalAllocatorModes.cpp`, Funktion `mode2RangeWithSourceStatus`.

Der Kandidat prüft `if(!v.complete) return std::nullopt`, noch bevor er den für marker >= 0x2000 und gespeichertes Sample-Ende 0 belegten Original-Skip durchführt. Der Fehler ist mit diesem C++23-Testfall reproduziert:

| Stimme | Marker | `stored_sample_end` | Vollständiger Chip-Snapshot | Originalverhalten |
|---:|---:|---:|---|---|
| 0 | 0x2000 | 0 | nein – **nicht benötigt** | ohne Chip-Portlesung überspringen |
| 1 | 0x0FFE | 0 | ja; VTFT highword 0 | sofortige Leerlauf-Auswahl: **Stimme 1** |

`capacity=2`, `packed_range=(2<<8)|0`, `mode_count=32`, kein blocked gate.

- **Original-Quellvertrag / eng begrenzte Forschungskorrektur:** Stimme 1, Reason `CompletelyIdle`.
- **Echter aktuell eingecheckter Entwicklerstand:** `std::nullopt`, also fälschlich **kein Ergebnis**.
- [Kompletter C++23-Regressionstest](https://github.com/challanger2000/125A-AWE64-Gold-VST3/blob/research/p003-v5-voice-row-capture/tests/p003_original_mode2_high_marker_lazy_chip_read.cpp)
- [GitHub Actions, beide Kandidaten tatsächlich kompiliert – PASS als Fehlerdetektor](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43999680970). Logs: `P003_MODE2_MARKER_SKIP_SOURCE_CONTRACT_PASS` für Forschungskorrektur, `P003_MODE2_INELIGIBLE_MARKER_CAUSES_FALSE_NO_RESULT` für echten P002-Stand.

**Eng begrenzter korrigierter Code auf dem P003-Forschungszweig:**

```cpp
if (forbiddenByDeviceMode(v,driver_mode_count)) continue;
// Original +41DE jumps directly to +424F. No CCCA call if saved end=0:
if (v.internal_key_marker>=0x2000u && v.stored_sample_end==0u) continue;
// Other paths still require independently complete chip snapshot:
if (!v.complete) return std::nullopt;
```

Die Korrektur darf **nicht** auf Marker unter 0x2000 oder gespeichertes Sample-Ende ungleich null ausgeweitet werden, weil dort ursprüngliche Hardwarelesungen benötigt werden. Die Snapshot-Struktur bildet verschiedene Teilvollständigkeiten nur eingeschränkt ab; dies ist kein Nachweis einer allgemeinen, fehlerfreien Modus-2-Portierung.

## Abnahmegrenze

Dieser durch Original-Disassembly und gegen realen Entwickler-C++23-Stand belegte Bug ist **unabhängig** vom bereits nachgewiesenen Modus-1-Fehler bei blockiertem oberem Nachbarn. Beide sind im gemeinsamen [CI-Vergleich](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43999680970) reproduziert. Keine Quelle enthält private Treiber-, ROM-, SoundFont- oder Gasttrace-Bytes. Noch keine neue Windows-95-Gegenprobe zu genau diesem synthetischen Zustandsfall; keine autonome VST3-PCM-Parität behauptet.
