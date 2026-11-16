# P106 — Originaler SBAWE.VXD x86-Kontrollfluss: CCCA → VTFT

**Forschungsbeleg 16.11.2026.** Der ursprüngliche Windows-95-Creative-Treiber bleibt unverändert. Die Untersuchung ist ausschließlich statische Rekonstruktion möglicher x86-Wege, **kein** fertiger CPU-Zeitgenerator oder PCM-Nachweis.

## Originaleingaben, nur in privater Umgebung

- Original Creative `SBAWE.VXD`, SHA-256 `d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`.
- Aus Original-LE extrahiertes, korrekt relocatiertes `LCOD`, **31.200 Bytes**, SHA-256 `bea69aa3492aeb6fc40400fca1cf584d553cbaa973d9011784db25867beed906`.
- Originale CPU-V4-Messung `P003-20261116T014751Z-3594ea53-ERGEBNIS.zip`, SHA-256 `2b507624a159f4f8ed9c69e22b3a300bbbe8937aecb0d6d3b254ae641ca2f3c6`.
- `tools/p106_original_vxd_stereo_cfg_private.py` liest einen privaten LCOD-Blob und optional die private V4-Aufzeichnung; beide Eingaben müssen SHA-256-geprüft sein. Originalbytes und Original-I/O-Werte werden weder in den Prüfer geschrieben noch nach GitHub hochgeladen.

## Tatsächlich ausgeführte Privatprüfung

Das SHA-geprüfte 32-Bit-x86-Codeobjekt wurde mit **GNU objdump** als i386 dekodiert. Ein ausdrücklich begrenzter, x86-Jcc/JMP-bewusster Kontrollflussgraph beginnt **unmittelbar nach der originalen CCCA-Hardwarelese-Funktion bei LCOD+0x40C4**. Er endet beim ersten nächsten **VTFT-Helferaufruf LCOD+0x4060**, oder separat bei einem anderen Hardware-Helfer. Jeder Pfad darf jede x86-Adresse höchstens einmal besuchen (bewusst **kein** unbeschränktes Wiederholen des Voice-Scans); undefinierte Zieladressen werden nicht frei geschätzt. Branch-Offsets `40CB/40DA/40E4/40F1/4107/4120` werden anhand tatsächlicher Originalopcode-Mnemonics verifiziert.

**Originalresultat:**

| Originalkontrolle | Resultat |
|---|---:|
| Ohne Laufzeitzustand *syntaktisch* mögliche Pfade zu nächstem VTFT | **48** |
| Beschränkung auf direkt folgende **nächste Stimme im vollständigen Modus-1-Scan** ohne unmittelbare Freigabe / übersprungene Stimme | **16** |
| Instruktionen pro verbleibendem Weg (im relevanten Bereich) | **40 bis 45** |
| Noch ungeklärte bedingte Original-x86-Sprungstellen | **4** |
| Durch Original-V4-Messung verifizierte Stereoallokationen | **25** |
| Vollständige, gleiche Original-Helferabläufe | **9** |
| Unterschiedliche Original-CCCA→VTFT-Taktintervalle | **8 Stellen**, Klassen **91/93/95** |

Die **vier** verbleibenden Entscheidungen lauten:

| LCOD-Offest | Original-x86-Befehl | Bedingung |
|---|---|---|
| `+0x40CB` | `JA` | Sample-End-Vergleich nach CCCA-Hardwarelesung |
| `+0x40E4` | `JE` | Prüfwert für Paar-Strafgewicht |
| `+0x40F1` | `JAE` | Vorherige gegenüber aktueller Stimmenbewertung |
| `+0x4107` | `JB` | Beste bisherige Stimmenkombination beibehalten oder ersetzen |

Aus den neun **echten** originalen Hardwarefolgen folgen für die übrigen sieben Sprungstellen Einschränkungen: der Scan wird fortgesetzt, die nächste Stimme wird weder übersprungen noch wegen fehlendem Paar sofort freigegeben. Diese Einschränkungen begründen das Reduzieren von 48 auf 16 **mögliche**, nicht bewiesenermaßen *ausgeführte* Pfade.

**Entscheidend:** Ein Original-Hardwarecallback enthält dessen CPU-TSC/CS:EIP/Call-Return und Registerwort, aber **nicht das x86-EFLAGS-Protokoll sämtlicher Zwischeninstruktionen**. Der Original-Gast kann bei unveränderter Hardwareaufruffolge mehrere dieser 16 Kontrollflusspfade wählen. Damit sind die exakten **91/93/95**-Taktunterschiede weiterhin **nicht kausal** einer bestimmten Sprungentscheidung zugewiesen. Auch 40–45 Instruktionen sind *keine* Umrechnung in 91/93/95 CPU-Takte – 86Box-P54C-Dynarec berücksichtigt U/V-Pairing, Registerabhängigkeiten, AGI, andere I/O- und Speicheroperationen.

## Reproduzierbare Tests

- Privat in der bereits vorhandenen isolierten Umgebung erfolgreich ausgeführt: originaler LCOD- und V4-Trace-Abgleich, **48 → 16**, 4 Branch-Stellen, 25/9/8, verifizierte Original-SHA.
- Im öffentlichen GitHub-Zweig werden nur der Quellprüfer und **vollständig erfundene** CFG-Testbefehle gespeichert.
- `.github/workflows/p105-pentium-dynarec-pairing.yml` prüft `--selftest` mit ungültigem Sprungziel und synthetischer Verzweigung **unter Ubuntu und Windows 2022**. P105-CPU-Pairing-C++23-Tests bleiben aktiviert.
- Ein **privater** Originaltest: `python tools/p106_original_vxd_stereo_cfg_private.py --private-lcod /privat/LCOD.bin --private-v4-trace /privat/Original-V4.zip`. Keine private Datei wird dafür in GitHub Actions benötigt.

## Konkrete noch offene Ursache

Benötigt werden die dynamischen **Jcc genommen/nicht genommen**-Entscheidungen einschließlich x86-EFLAGS / vorausgehender Register- und Speicherzustände zwischen `0x40C4` und dem jeweils nächsten `0x4060`. Diese wurden in der originalen 86Box-Aufnahme **nicht** protokolliert. Die Rekonstruktion sollte an exakt diesen vier Stellen weitergehen, nicht an den 445 originalen E8-Zieladressen oder mit Lookup-Konstanten. Erst danach lassen sich die originalen P54C-CPU-Zyklen pro belegtem Pfad gegen mehrere unabhängige Win95-Aufnahmen berechnen und eine autonome VST3-Zeitleiste testen.

**Keine Änderung an AWE64-Gold-Produkt-VST3 oder originalem EMU8000-DSP. Keine Studio-One-Vorabprüfung. Keine Handlung des Nutzers notwendig.**
