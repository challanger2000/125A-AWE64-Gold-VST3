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

## Neuer belastbarer Originalfund (17.11., 02:55 UTC)

**Authentischer Live-Nachweis statt nur synthetischer Quellhypothese:** Das originale Windows-95-SBAWE.VXD verwendet in **Modus 0 physische Stimmen 30 und 31**. Die erste private Aufnahme zeigt 2.686/2.649 originale VTFT-Leseselektoren und 101/117 originale DCYSUSV-Freigabewörter für Voices 30/31, die zweite 2.688/2.625 bzw. 108/122. Der Originalcode selbst prüft `device+0x06` als dynamische vollständige Voice-Grenze. Der korrigierte Forschungsdispatcher (bis 32), originale Port-Wort-Helfer und die auf vollständige 32 Voice erweiterte deterministische Dreimodus-Differenzialprüfung bestehen zusammen in [GitHub CI #44006536172](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44006536172). Der unveränderte ältere Entwicklercode ist mit dem originären Voice-30/31-Gegenbeispiel unabhängig als zu eng nachgewiesen. Vollständiger [Originalbefund](P003-ORIGINAL-WIN95-MODE0-32-PHYSICAL-VOICES-EVIDENCE-20261117.md).

**Keine Einzelübergabe und kein PR-Kommentar.** Die Ergebnisse werden im Forschungszweig konsolidiert.

## Weiterer echter Originalnachweis: 30/31 sind nicht nur erreichbar, sondern rangbasiert auswählbar

Die erneute Auswertung beider **authentischer** Win95-SBAWE.VXD-Portspuren belegt **448 echte Auswahl-/Freigabevorgänge** für physische Stimmen 30 und 31 (Sitzung A: 101 + 117, Sitzung B: 108 + 122). **179 davon** enden nach einer unmittelbar vorangegangenen echten VTFT-Lesung für die letzte Stimme der dritten Modus-0-Lane (**29**). Das ist ein eigenständiger Originalnachweis einer **rangbasierten** Auswahl der früher besuchten Stimmen 30/31 nach dem Dreiergruppen-Scan, nicht nur der sofortigen Idle-/Sample-End-Auswahl.

Auch die *Folgeoperation* wurde vollständig geprüft: **448/448** originale Modus-0-Auswahlen der Stimmen 30/31 gehen unmittelbar in `DCYSUSV=0x00FF` und einen originalen `VTFT=0x0000FFFF`-Initialisierungsschreibzugriff auf **dieselbe physische Stimme** über. Die Original-I/O-Folge ist exakt `A20=0x00FF`, `E22=0x7E/0x7F`, `620=0xFFFF`, `622=0x0000`, ohne einen einzigen abweichenden Fall. Es geht um reale Registerprogrammierung im Originaltreiber; die native VST3-Audioidentität ist damit nicht bewiesen.

Ein [privater, SHA-gepinnter Original-Auditor](../tools/p003_audit_original_mode0_highvoice_select_start_private.py) und [zusätzliche C++23-Rang-/Gleichstands-/Sample-End-Regressionsfälle](../tests/p003_original_mode0_highvoice_selection_ties.cpp) sind im Forschungszweig hinterlegt. Die [CI mit voller vorheriger Suite ist PASS](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44008082076). Details siehe [Original-Nachweis mit 32 Stimmen](P003-ORIGINAL-WIN95-MODE0-32-PHYSICAL-VOICES-EVIDENCE-20261117.md).

**Keine neue Entwicklerübergabe, kein PR-Kommentar und keine Änderung an P005.**

## Originales Modus-0-Stimmenfreigabe-Protokoll jetzt über **alle 32 Stimmen** geprüft

Über den oben dokumentierten ursprünglichen Voice-30/31-Nachweis hinaus wurden **beide vollständigen, SHA-256-authentifizierten Win95-Creative-CPU-I/O-Spuren** erneut auf **jede** ausgeführte Original-Freigabe bei `LCOD+3FCE` und die unmittelbar folgenden Porttransaktionen bei `LCOD+3E0C` geprüft.

| Tatsächlich ausgeführte Originalereignisse | Sitzung A | Sitzung B | Beide |
|---|---:|---:|---:|
| Modus-0-Stimmenfreigaben (`E22 = 0xA0 | voice`) | 5.878 | 5.878 | **11.756** |
| Verifizierte unmittelbar folgende Port-Writes (`A20`, `E22`, `620`, `622`) | 23.512 | 23.512 | **47.024** |
| Falsche Stimme / falscher Caller / falscher Hilfs-Program Counter / falscher Wert / falsche Reihenfolge | 0 | 0 | **0** |
| Unterbrochene oder unvollständige Freigabesequenzen | 0 | 0 | **0** |
| Tatsächlich ausgewählte physische Stimmennummern | 0–31 | 0–31 | **alle 32** |

**Originale Ereignisfolge (pro Auswahl, unabhängig von der Voice-ID):** `E22 (8-Bit) = 0xA0|voice` bei Original `+3FCE` → `A20 (16-Bit) = 0x00FF` am selben Original-Caller → `E22 (8-Bit) = 0x60|SAME_voice` bei Original `+3E0C` → `620 (16-Bit) = 0xFFFF` → `622 (16-Bit) = 0x0000`. Der letzte doppelte Port-Write ist das physische Original `VTFT=0x0000FFFF` als DATA0-32-Bit-Wort. Der Nachweis umfasst **11.756/11.756** vollständige Originalauswahlen, nicht nur 448 High-Voice-Fälle.

**Timing-Negativbeweis gegen eine universelle Offsetkonstante:** Der gemessene Original-CPU-TSC-Abstand von der Freigabe-E22-Ausgabe bis zum nachfolgenden VTFT-E22-Selektor schwankt in Sitzung A von **91 bis 4.062** Gast-TSC-Takten (häufigster Wert 92, 2.527 Vorkommen), in Sitzung B von **91 bis 701** Takten (häufigster Wert 94, 2.204 Vorkommen). Das ist ein **Verteilungs-/Originalzustandsbefund**, kein isolierter Beweis einer CPU-Zeitformel oder der Ursache einzelner Ausreißer. Eine pauschale harte Zeitkonstante wäre aus diesen zwei Originalmessungen nicht gerechtfertigt.

**Nachweis und maschinelle Verifikation im getrennten Forschungszweig:**
- [Strikter, SHA-gepinnter Original-Win95-Auditor](../tools/p003_audit_original_mode0_complete_release_private.py): prüft Original-Quelladresse, reale x86-Helfer-Program Counter, vollständige physische I/O-Reihenfolge und 32-Voice-Abdeckung; gibt nur aggregierte Daten aus. Private ROM-/SF2-/Win95-Daten sind NICHT veröffentlicht.
- [Sieben unabhängige synthetische Negativ-/Positivtests des Auditors](../tests/test_p003_original_mode0_complete_release_audit.py): falsche Voice, falsches A20-Wort, falscher PC, fehlende Folgeoperation, ersetzter Read und unzulässiger TSC-Rücksprung müssen als Fehler erkannt werden.
- [C++23-Originalausgabe für ALLE 32 Stimmen und drei Freigabegründe](../tests/p003_original_all32_release_vtft_words.cpp): kontrastiert die vorhandenen Writer mit der gesicherten Original-Registerfolge und verbietet Stimme 32/255.
- [GitHub-CI mit vollständiger vorheriger Suite und neuen Prüfungen](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44009319217). Dieser Lauf muss PASS sein; bis dahin darf keine neue Testfreigabe behauptet werden.

**Abnahmegrenze:** Dies bestätigt den Original-Register-/Port-Ablauf für die beobachteten 11.756 Freigaben und die Übereinstimmung der isolierten C++23-Ausgabehelfer mit diesem Format. **Nicht** getestet ist damit die produktive VST3-Audioausgabe, die vollständige Creative-Note-On-Registerfolge ab dem VST3-Einstieg oder autonom exakt reproduziertes Pentium-Timing.

**Kein GitHub-Kommentar am Entwickler-PR, keine Einzelübergabe und kein Eingriff in P005 oder den separaten REA-Port.**

## Offene und bewusste Grenzen

1. **Modus 0: die originale physische Grenze 32 ist inzwischen BEWIESEN.** Zwei unabhängig SHA-gepinnte echte Windows-95-Portaufnahmen zeigen `LCOD+3F13`-VTFT-Lesungen **und** `LCOD+3FCE`-DCYSUSV-Freigabeschreibzugriffe für Stimmen 30 und 31. Der Original-x86-Loop bei `+3ED4` verwendet direkt `[ESI+0x06]` (tatsächlich 32), **keine** generelle 30-Stimmen-Grenze. Der isolierte Forschungsdispatcher und die Port-Wort-Helfer sind auf die Quelle korrigiert; die alte bewusst enger definierte 30-Music-Voice-Hilfs-API bleibt getrennt. Details, genaue Originalanzahlen und Gegenprüfung: [Mode-0-32-Originalbeweis](P003-ORIGINAL-WIN95-MODE0-32-PHYSICAL-VOICES-EVIDENCE-20261117.md). **Noch offen** sind die restlichen Volltreiber-/VST3-Integrationsschritte, keine Zahlengrenze mehr.
2. Die sieben zuvor isolierten **+3-CPU-Taktabweichungen** beim Mode-1-Stimme-4→5-Pfad sind weiter offen. Keine erfundenen Zeitkonstanten; sie sind nach funktionalen Allocatorproblemen nachrangig.
3. Die im Forschungszweig verwendeten `vtft_read_verified`/`dcysusv_read_verified`/`ccca_read_verified`-Flags sind **Nachweis-Bedingungen**, keine neue originale Creative-Datenstruktur und nicht automatisch die finale ABI für den Entwickler.
4. Kein zusätzlicher Windows-95-/86Box-Lauf, keine VST3-Integration und keine EMU8000-DSP-Modifikation in dieser Charge. Vollständige autonome P003-VST3-Audioparität bleibt unbewiesen.

## Arbeitsteilung

Die Funde bleiben gesammelt auf `research/p003-v5-voice-row-capture`. **Kein neuer Kommentar im Entwickler-PR, keine einzelne Entwicklerübergabe, kein Unterbrechen der REA-/C++23-Entwicklung.** Eine später sinnvolle Übergabe kann diesen gemeinsam getesteten Bestand als ein konsolidiertes Paket verwenden.
