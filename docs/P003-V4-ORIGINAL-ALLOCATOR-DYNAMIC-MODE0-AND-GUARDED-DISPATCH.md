# P003 – Dynamische Original-V4-Stimmenpfade und bewachter 3-Modi-Dispatcher

16. November 2026 · Entwicklungszweig `develop/p002-creative-register-oracle` · keine Produktionseinbindung.

## Authentifizierte Originalmessung

Aus der privaten Besitzer-ZIP `P003-20261116T003509Z-d6e65f07-ERGEBNIS.zip`
(SHA256 `fd72103675d5c53d9517626ad0ff18f59afd505cbc9e80b497ff8d9af7221969`)
und dem originalen `SBAWE.VXD` (SHA256
`d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`)
wurde die komplette 86Box-V4-CPU-/Chipspur **erneut unabhängig 1:1**
verglichen: **816.594/816.594 physische EMU8000-Portereignisse**,
**760.209 Writes**, originale Zeitstempel ohne Regression.
Bei originalen Byte-Reads sind Port/Richtung/Reihenfolge belegt;
ein vollständiger 16-Bit-Lesewert wird im Gast-Byte-Callback nicht
behauptet.

Die **sessionspezifische** LCOD-Adresse
`0xC10151E5` wurde aus **816.583/816.594** echten I/O-EIP-Opcodeankern
ermittelt, nicht aus einer früheren VM angenommen.

### EBP-Rücksprunghinweise in Originalcode-Regionen

| SHA-geprüfte Originalfunktion | Physische I/O-Events mit gestapeltem Rücksprunghinweis |
|---|---:|
| LCOD+0x3D6F..0x3EA1 – Allocator-Dispatcher | 24.756 |
| LCOD+0x3EA1..0x3FE8 – **Modus 0** | **16.071** |
| LCOD+0x3FE8..0x416C – Modus 1 Paarstimme | **0** |
| LCOD+0x416C..0x42A7 – Modus 2 Bereich | **0** |
| LCOD+0x53F5..0x5700 – Descriptor-/zweiter NoteOn-Pfad | 52 |

Das sind **Portcallbacks, nicht 16.071 Allokationsvorgänge**.
Eine nicht beobachtete gestapelte Adresse beweist nicht, dass ein
Codepfad niemals ausgeführt wurde. Die originale EBP-V4-Spur zeigt
bekanntlich einen nicht geklärten **Ein-Byte-Versatz** gegenüber den
x86-Architekturwerten; der neue Prüfer verschiebt diese Adressen
**nicht** automatisch.

### Die echten Descriptor-Dispatch-CALLs

Im **SHA-gepinnten originalen LCOD** wurden die echten x86 `E8 rel32`
OP-CALLs mitsamt Ziel `LCOD+0x3D6F` nachgerechnet:

| CALL-Stelle | Originalzweig | V4-Rücksprunghinweis auf **CALL-Endbyte** |
|---|---|---:|
| +0x4C40 | ältere Mono-Zuweisung | 0 |
| +0x5522 | Descriptor-Flag `0x04`, Modus 1 | **0** |
| +0x5560 | alternativer Modus 0 | **52** |

An der **architektonisch erwarteten Rücksprungadresse (CALL+5)**
wurden an diesen Stellen 0 Events erfasst. Die
ein Byte frühere Stackadresse (CALL+4) ist ein **Diagnosehinweis,
noch kein architektonischer Live-CALL-Beweis**. Er passt jedoch zur
Modus-0- und Descriptor-Kontrollfluss-Analyse.

Der aktuelle Original-Gasttest belegt also **nicht** die tatsächliche
Ausführung von Modus 1 oder 2. Wir dürfen dafür kein bereits
gültiges Original-Port-/PCM-Acceptance-Ergebnis behaupten.

## Umsetzung auf GitHub

- `tools/p003_audit_original_v4_allocator_modes.py`:
  verifiziert originalen VXD-SHA, CPU-/EMU8000-Spuren, originale
  EIP-/Stacktiefe und diese drei Original-Dispatch-CALLs. Privat bleibt
  privat: kein VXD, ROM, SF2, PCM oder Stackdump im Repository.
- `tests/test_p003_audit_original_v4_allocator_modes.py`:
  überprüft getrennte Modusbereiche, falsche Return-Sites,
  ungültige E8-/Zielmutanten, 1-Byte-Grenzfälle und fehlende
  Originaltreiberdaten. Öffentliche Linux-CI nur mit Synthetik.
- `CreativeVxdAdditionalAllocatorModes.{h,cpp}`:
  gemeinsame `candidateOriginalWin95VxdDispatchAllModes`-
  Schnittstelle für die isolierten originalen Modus-0/1/2-Algorithmen.
  Nur **explizit bestätigte** x86-Modus- und echte Live-Gerätezustände
  können die Auswahl aktivieren; ungültige/missing state ist
  `std::nullopt`, echte erschöpfte Auswahl ist `0xFF`.
- **Kapazitätsgatter:** `device+0x06` bleibt separate 1..32
  physische Kapazität. Modus 0 akzeptiert die bisherige Musikgrenze
  30 nur mit **zusätzlich** bestätigter Original-Scanobergrenze;
  Modus 1/2 können das volle Originalkapazitätsfeld verwenden.
- `tests/p003_original_vxd_additional_allocator_modes.cpp`
  prüft alle drei Dispatcher-Modi, nicht autorisierte Quelle,
  30/32-Kapazitätsgegensatz, Paarstimme, Bereich, Original-`0xFF`,
  fehlende Voice-Snapshots und unzulässige Modus-ID.

**Unverändert:** Die gepinnte ursprüngliche `snd_emu8k.c`/EMU8000
DSP-Quelle, der nachgewiesen source-clocked PCM16-Referenzmodus,
keine unvalidierte Mode1/Mode2-Umschaltung der tatsächlichen
VST3-MIDI-Wiedergabe und kein Studio-One-/ASIO-Endtest.

## Fehlende Nachweise und weiterer Entwicklerauftrag

1. Eine **neue originale Win95-MIDI-Sequenz**, die nachweislich den
   Descriptor-Flag-`0x04`-Zweig und die bereichsbeschränkte
   Modus-2-Routine ausführt; diese darf nicht aus der bisherigen
   C4-/D4-Aufnahme fingiert werden.
2. Vollständige originale Voice-/Descriptor-/Port-/CPU-Zustände für
   alle 32 physische Stimmen und Stereo-Paar-Release-Reihenfolgen.
3. Erst nach Originalprüfung: Einbindung in tatsächliche
   MIDI→EMU8000-VST3-Ausführung und unabhängige native PCM16-
   Differentialtests mit neuen Noten/Velocity/Overlap/Programmen.
4. Autonome Gast-CPU-Startphase und originaler Treiber-Cycle-Pfad
   bleiben für allgemeine bitidentische Ausgabe ungeklärt.

**Gate: P003 autonome Treiber-/PCM-Bitidentität NO-GO; kein Merge.**
