# P003 unabhängige Originalprüfung – gesammelte Befunde, **noch keine Entwicklerübergabe**

**Stand: 17.11.2026.** Dieses Dokument ist ein **Forschungszwischenstand im separaten GitHub-Forschungszweig**, nicht ein PR-Kommentar, Freigabe oder Arbeitsauftrag an den Entwickler. Die parallele direkte REA-Portierung und P005 bleiben unangetastet. Die echten Originaldateien (Win95-Gast, SBAWE.VXD, ROM, SF2, WAV und komplette Traces) befinden sich **nicht** im öffentlichen Repository.

## Ausgangspunkt

Authentische Original-SBAWE.VXD mit privater SHA-256 `d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`. Das Original-Disassembly wurde erneut geprüft: Für Modus 0 gilt `LCOD+3EA1..3FE7`, für Modus 1 `+3FE8..416B`, für Modus 2 `+416C..42A2`. Die vorherigen echten Original-Win95/86Box-Traces sind separate Nachweise; die hier neu hinzugefügten Registerprovenienz- und Differenzialtests sind **synthetische, aus Original-x86-Kontrollfluss abgeleitete** C++23-Tests, keine erneute Windows-95-Laufzeitmessung.

## Gesammelte, am gepinnten Entwickler-C++23-Stand reproduzierte Abweichungen

Fest gepinnter Dev-Commit für reproduzierbare Negativprüfungen: `940098bd3e35f5b2b2b7c94ec9283b5b45067990` (`develop/p002-creative-register-oracle`). Nicht als Aussage über künftige Developer-Commits interpretieren.

| Bereich | Nachgewiesener Quellvertrag | Befund am gepinnten Entwicklerstand | Forschungsstatus |
|---|---|---|---|
| Modus 1: gesperrte Nachbarstimme | `+403E -> +4111`: obere Nachbarstimme gesperrt -> `previous=0xFFFFF`, kein Score von aktueller Stimme übernehmen | Wählt im isolierten Gegenfall Paar **1+2** statt **0+1** | C++23 Gegenvergleich und Forschungskorrektur PASS |
| Modus 2: hoher Marker, cached end 0 | `+41DE -> +424F -> +4269` **ohne** VTFT/DCYSUSV/CCCA | Vollständigen Chip-Snapshot fälschlich erforderlich; gibt `nullopt` statt später freie Stimme | Gegenvergleich PASS |
| Modus 2: hoher Marker, cached end >0 | `+424F..4267`: **nur** bestätigten CCCA lesen | `complete` verlangt nicht benutzte weitere Chipwerte | Separate Provenienz `ccca_read_verified`, Positiv-/Negativtest PASS |
| Modus 2: frühes Idle / weitere Scoring-Pfade | `+41E0..423F`: VTFT, bedingt DCYSUSV, früher Idle-Return **ohne CCCA** | Überbreite `complete`-Voraussetzung trotz ursprünglichem VTFT-Only-Pfad | Separate `vtft_read_verified` / `dcysusv_read_verified`, Test PASS |
| **Neu: Modus 0**, frühe Verzweigungen | `+3EEF,+3F00,+3F0E,+3F5F,+3F7E,+3FD8`: nur aktuell erreichte Voice-Row auswerten; vorzeitiger Idle-/Sample-End-Return | Pauschaler Vorabcheck aller 30 Stimmen lässt echten frühen Idle-Treffer bei fehlenden Daten *späterer, nie gelesener* Stimmen scheitern | P003-Forschungskorrektur und unveränderter Entwickler-C++23-Gegentest PASS |

Die Modus-2-Fälle stammen aus **derselben strukturellen Schwäche**: Ein einziges `complete`-Bit repräsentiert nicht, welche Register der Originalcode tatsächlich ausgelesen hat. Sie sind nicht automatisch getrennte Fehler in einer bereits produktiv ausgeführten MIDI-Pipeline. Nur die reproduzierten isolierten Auswahlentscheidungen sind hier bewiesen.

## **Neu geprüft: Modus-0-Kontrollfluss und Durchlauflogik**

Echte originale x86-Folgen:
- `LCOD+3EB6`: `[ESI+0x18] > 2` umgeht die gesperrte-Stimme-Prüfung.
- `+3EEF`: bei kleinerem Wert `[EDI+0x0D]` prüfen; gesperrte Stimme wird **ohne Chiplesen** übersprungen.
- `+3F00`: Marker `>=0x2000` überspringt VTFT und DCYSUSV; bei `stored_sample_end=0` wird auch CCCA nicht gelesen.
- `+3F07..3F5F`: VTFT-Wert lesen; bei Marker `0x0FFE` und VTFT highword 0 **sofort** diesen Voice-Slot wählen. Keine Prüfung unbesuchter Stimmen und kein CCCA.
- `+3F71..3F8A`: falls `stored_sample_end>0`, CCCA lesen, auf 24 Bit maskieren, bei erreichtem Endwert sofort wählen.
- `+3F8C/+3FA0/+3FA7`: originale Dreiergruppen-Durchläufe `0,3,6…`, `1,4,7…`, `2,5,8…`; spätere Stimmen werden *nicht* vorzeitig geprüft.

Die neue Forschungsversion des Mono-Scorers verwendet source-ordered Einzelregister-Verifikation und einen vom Auswahlresultat getrennten `missing_source`-Status. Der Modus-0-Dispatcher gibt die Werte korrekt durch, statt pauschal 30 Stimmen vor der ersten Entscheidungsbedingung zu preflighten. Der alte numerische Mono-Test wurde an den echten `+3FD8`-Sofortreturn angepasst und der Vollständigkeitstest prüft weiterhin fehlende **tatsächlich verwendete** Register.

## Unabhängige, deterministische Differenzial-Gegenprüfung

`tests/p003_original_modes_deterministic_differential_15000.cpp` erzeugt **15.000** reproduzierbare komplette synthetische Treiber-/Chip-Zustandsvektoren und vergleicht alle drei P003-Forschungsallocator-Routinen mit getrennt implementierten, von den Original-x86-Kontrollpfaden abgeleiteten Referenzen:

- Absteigende Modus-1-Paare inklusive geblockter Nachbarn, Voice-Row-Score, Penalty und Sentinel.
- Aufsteigende Modus-2-Bereichsauswahl inklusive Idle, Marker, Gleichstand und Sample-End-Kurzschluss.
- Dreigeteilte Modus-0-Suche inklusive Idle, Marker/Status und Sample-End.
- Beide Referenzen verwenden ausschließlich **vollständig vorhandene** syn­thetische Hardwaredaten. Partielle Original-Read-Provenienz wird separat mit eigens konstruierten Negativtests geprüft.
- Die Differenzialprüfung ist **kein** bitgenauer x86-Instruktions-/Zeitplan-, Win95-Treiber-Live- oder PCM-Paritätsnachweis.

**Gesamt-CI: [P003 Research Run #44005095829](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44005095829) – PASS.** Darin: alter Modus-0-Numerik-/Register-I/O-Test, neuer Modus-0-Lazy-Read-Test, Entwickler-Negativfall, volle ursprüngliche Modus-0/1/2-Suite, 15.000 × drei synthetische Originalquellen-Vergleiche, bestehende Modus-1-/Modus-2-/Provenienz-Negativfälle.

## Offene und bewusste Grenzen

1. **Modus-0 30 versus physisch 32 Stimmen**: `LCOD+3ED4` liest `[ESI+0x06]` und begrenzt damit die originale Modus-0-Laufweite dynamisch. Die aktuelle alte C++-Mono-API repräsentiert dagegen nur 30 musikalische Stimmen und der Forschungsdispatcher akzeptiert dafür explizit nur einen verifizierten `mode0_scan_ceiling<=30`. Der bisherige echte Modus-1-Originalgastzustand belegte 32 physische Stimmen. **Zu prüfen:** ob bei realem Modus-0-Originalaufruf tatsächlich 32 (und welche reservierten Stimmen) durchsucht werden. Keine blinde Erweiterung des produktiven Budgetlimits ohne diesen Nachweis.
2. Die sieben zuvor isolierten **+3-CPU-Taktabweichungen** beim Mode-1-Stimme-4→5-Pfad sind weiter offen. Keine erfundenen Zeitkonstanten; sie sind nach funktionalen Allocatorproblemen nachrangig.
3. Die im Forschungszweig verwendeten `vtft_read_verified`/`dcysusv_read_verified`/`ccca_read_verified`-Flags sind **Nachweis-Bedingungen**, keine neue originale Creative-Datenstruktur und nicht automatisch die finale ABI für den Entwickler.
4. Kein zusätzlicher Windows-95-/86Box-Lauf, keine VST3-Integration und keine EMU8000-DSP-Modifikation in dieser Charge. Vollständige autonome P003-VST3-Audioparität bleibt unbewiesen.

## Arbeitsteilung

Die Funde bleiben gesammelt auf `research/p003-v5-voice-row-capture`. **Kein neuer Kommentar im Entwickler-PR, keine einzelne Entwicklerübergabe, kein Unterbrechen der REA-/C++23-Entwicklung.** Eine später sinnvolle Übergabe kann diesen gemeinsam getesteten Bestand als ein konsolidiertes Paket verwenden.
