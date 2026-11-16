# P005 – Original-Windows-95-Gastspeicher tatsächlich aufzeichnen

**Stand: 16.11.2026 · Windows x64 · Original Creative/86Box · NUR PRIVATE DATEN**

## Wozu dieses neue Werkzeug dient

Bisher waren aus der realen 86Box-/Windows-95-Sitzung die originalen
EMU8000-Portereignisse, CPU-TSC, zwei SoundFonts und die DRY-WAV
vorhanden. **Nicht** vorhanden waren die tatsächlichen
Windows-95-VXD-Gastspeicherbereiche, auf die die 288 Bankzeiger bei
`device+0x6CA` zeigen.

`tools/p005_capture_original_win95_vxd_guest_bank.cpp` löst genau
diese Lücke durch eine **read-only Aufnahme des laufenden
86Box-Prozesses**, ohne Patches des Creative-Treibers, RAM-Schreibzugriff,
erfundene SF2-Programme oder Modifikation des originalen EMU8000.

**Die Aufnahme liest nur das Gerät und die referenzierten Listen:**
1. Den echten **0x1592-Byte-Creative-VXD-Gerätezustand**.
2. Seine **288 originalen 32-Bit-Zeigerwerte** ab Offset `0x6CA`.
3. Für jeden nichtnull und eindeutig referenzierten Originalzeiger
   genau die originalen **0x1A = 26 Byte langen VXD-Presetzeilen**
   bis einschließlich des ersten `0xFFFF`-Abschlussdatensatzes.
4. Die tatsächlich verwendeten Win95-Gast-Geräteadresse und
   physische Seitentabellenbasis (CR3).

Die Werkzeuge suchen den VXD-Gerätezustand über die **tatsächlich
vorhandenen x86-Gastseitenmappings** der RAM-Aufnahme und lesen den
Gastspeicher mit der originalen Win95-Seitentabelle. Keine Adresse
einer vorherigen VM-Sitzung wird vorausgesetzt; Seiten müssen
nachweisbar present und innerhalb der echten 64-MiB-86Box-RAM-
Konfiguration liegen. Originalzeiger-Aliase bleiben erhalten.

Zwei zeitlich getrennte Leseaufnahmen müssen **dieselben
Geräte- und Bankdaten** liefern. Bei unstabilen oder mehreren
verschiedenen Quellzuständen endet die Aufnahme mit NO-GO.

## Privates Windows-Gesamtpaket

Das CI-Paket `P005-Original-Win95-Guest-Bank-Capture-WinX64`
enthält ausschließlich unseren eigenen Quellcode und die gebaute
Windows-x64-EXE, **keine Creative-VXD-, ROM-, SF2- oder Win95-Bytes**.

Es enthält auch `START-P005-ORIGINAL-GASTSPEICHER-AUFNEHMEN.cmd`.
Nach dem Entpacken kann der Besitzer diese CMD starten, während
**genau eine** originale 86Box-Win95-VM mit bereits geladener
Creative-SoundFont läuft. Der Starter erkennt die Prozess-ID selbst,
benötigt keine Administratorrechte, sofern Windows den lesenden
Prozesszugriff erlaubt, und schreibt eine neue Datei unter:

`%LOCALAPPDATA%\125A\PrivateOriginalWin95Captures\`

Er überschreibt keine vorhandene Datei und schreibt **nichts**
in den Gast-RAM, die Festplatte oder den Creative-Treiber. Für
Bank1 und Bank2 werden getrennte Aufnahmen im jeweils tatsächlich
geladenen Originalzustand benötigt. Das Werkzeug besitzt keine
automatische Sessionzuordnung; der Anwender und der Original-
Win95-Lauf müssen den Zeitpunkt der jeweiligen Bank eindeutig
belegen, bevor daraus ein Produktivnachweis wird.

## Format der *privaten* Ausgabedatei `.125a-private-vxd`

Little Endian, ohne C++-Struct-Padding:

| Teil | Bytes | Inhalt |
| --- | ---: | --- |
| Magic | 8 | ASCII `125AVXD1` |
| Version | 4 | `1` |
| Guest-VXD-Geräteadresse | 4 | Echte Win95-Virtualadresse |
| CR3 | 4 | Tatsächliche physische Gast-Seitentabellenbasis |
| Geräteabbildgröße | 4 | `0x1592` |
| VXD-Slots | 4 | `288` |
| Anzahl eindeutiger Listen | 4 | Originalzeiger ohne Duplikate |
| Anzahl nichtnull Slots | 4 | Einschließlich Zeiger-Aliase |
| Vollständiges Geräteabbild | `0x1592` | Originale Bytes |
| Pro eindeutiger Liste | 8 + N | `u32 Gastzeiger, u32 Bytezahl, N Originalbytes` |

Alle Listen enthalten echte, vollständige 26-Byte-VXD-Zeilen und
eine Original-`FFFF`-Schlusszeile. Es werden keine Listen aus
SoundFont-PHDR-Zeilen erzeugt.

Der private Strukturprüfer
`tools/p005_verify_original_vxd_guest_bank_capture.py` untersucht
Zeiger, tatsächliche Listen, Sentinel, fehlende/trunkierte Bytes
und Quell-Hashsummen. Beispiel für die **private** Besitzerseite:

```powershell
python tools/p005_verify_original_vxd_guest_bank_capture.py --capture "C:\MeinPrivaterOrdner\P005-Original-VXD.125a-private-vxd" --original-vxd "C:\MeinPrivaterOrdner\SBAWE.VXD"
```

Auch ein passendes Original-VXD-SHA256 beweist **noch nicht**, dass
gerade diese VM zu diesem Zeitpunkt genau diesen Originaltreiber
benutzte. Der Prüfer meldet deshalb ausdrücklich
`STRUCTURE_VALID_SOURCE_PROVENANCE_STILL_REQUIRED`.

## Abnahme und offene Sicherheitsgrenzen

- `awe64_p005_original_win95_guest_bank_capture --self-test`
  prüft synthetische Original-VM-Seitentabellen, abweichende
  physische Seiten, **nicht feste Geräteadresse**, Original-
  Zeiger-Alias, Seitengrenze, fehlende Gastseiten und fehlende
  Tabellen-Endmarkierung.
- `tests/test_p005_verify_original_vxd_guest_bank_capture.py`
  prüft dasselbe private Dateiformat mit 18 Negativ-/Positivproben.
- Erst nach weiterer ORIGINAL-Win95-SHA-/Sessionbindung darf
  `deviceAndGuestMemoryIndependentlyAuthenticated` im
  `P005OriginalCreativeVxdDirectoryImporter` auf true gesetzt
  werden. Die hier gelieferte Datei ist **kein** automatisches
  Freigabesignal und keine vollständig spielbare Bank.
- Die originalen x86-Allocator-/PCM-Zeitpunkte bleiben beim
  Kontrollchat; der EMU8000-/CT1745-Kern wurde nicht geändert.
- Die **echten Originalaufnahmen niemals in ein öffentliches
  GitHub-Repository** übernehmen. Der Windows-CI-Runner prüft
  nur synthetische Speicherabbilder und unseren eigenen Code.

Der neue Exporter beweist seine eigene **technische Lesefunktion**
über den Selftest. Ein echter Win95-Bank-Capture ist erst
erfolgreich, wenn die private Besitzer-VM diesen Lauf ausgeführt
hat. PR #6 bleibt DRAFT/NO-MERGE.


## Originalaufnahme vom 16.11.2026 – tatsächliche indirekte Bankstruktur

Die erste private Besitzeraufnahme (`125AVXD1`) belegte nur einen von 288
Device-Slots. Dieser Slot zeigt auf Root-VA `0xC11B891C`, deren erste
sieben 32-Bit-Wörter **interne Gastzeiger** sind. Der erste zeigt auf
`0xC11B8968` (Root + 76), wo 129 durchgehend 26-Byte-breite
Creative-Presets und danach ein ausgerichteter `FFFF`-Abschluss liegen:
128 melodische Patches, danach `Standard` als Percussion-Preset.
Die erste alte Capture-Revision las hingegen Root+0 als Zeile 0 und fand
erst viel später eine zufällige `FFFF`-Übereinstimmung bei falscher
26-Byte-Ausrichtung.

Korrektur in PR #6: der Live-Exporter erfasst bei erkannten
indirekten Bankköpfen nur die originalen Root-Bytes bis zum ausgerichteten
Kindlisten-Abschluss. Offline-Verifier, Besitzerdateileser und
P005-Importer folgen dem echten Kindzeiger, behalten aber den originalen
Root-Zeiger in der 288-Slot-Geräteabbildung. Frühere Besitzerdateien,
die hinter dem echten Kindlisten-Abschluss überzählige
uninterpretierte Bytes enthalten, werden als solche gekennzeichnet;
sie sind **keine** attestierten Originalquellen.

Die Programmeinträge und ihre Namen sind **nur aus der privaten
Besitzeraufnahme** abgeleitet und werden nicht ins öffentliche Repo
übernommen. Eine zweite Original-Sitzungs-/VXD-Treiberattestierung,
SoundFont-Bank2, zusätzliche Modulator-/Key-/Velocity-Ketten und
CPU-TSC-/DRY-PCM-Parität bleiben ausdrücklich offen.
