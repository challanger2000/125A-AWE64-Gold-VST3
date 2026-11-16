# P003 — V4 dynamische SBAWE.VXD-Aufrufermessung (16.11.2026)

## Ausgangslage

Die Original-SBAWE.VXD hat eine durch x86-Opcode-/Statuskontrollen bestätigte
**statische** MIDI-Kette: LCOD+0x3847 MIDI-Parser, NoteOff 0x6072,
NoteOn 0x6188, Descriptor-Builder 0x42A7/0x4658, Stimmenpfade
0x4BB4/0x53F5, Allocator 0x3D6F, I/O-Hilfen 0x3C49–0x3D44.
Siehe unabhängigen [Kontrollkommentar #6903773827](https://github.com/challanger2000/125A-AWE64-Gold-VST3/pull/4#issuecomment-6903773827).
**Statische Kanten sind keine Nachweise tatsächlicher Ausführung oder Timing.**

## Implementiert — V4 im unveränderten 86Box-EMU8000-Fork

- `tools/p003_instrument_86box_io.py`: Patches **nur** die
  `src/io.c`-Kopie des SHA-gepinnten originalen 86Box-Forks.
  Unveränderte 8/16/32-Bit-EMU8000-Hardwarecallbacks werden weiter
  identisch protokolliert (V3-Basis).
- Optional `AWE64_P003_GUEST_CALLSTACK=1` aktiviert das zusätzliche
  private Format `P003_CPU_GUEST_PHYSICAL_8_16_32_V4_STACK`.
  Pro Portevent: CPU-TSC, verbleibende CPU-Zyklen, CS:EIP, Port,
  Wert, Sequenz, **EBP, bis zu vier stackgespeicherte Rücksprung-EIPs
  und EAX/EBX/ECX/EDX**.
- Stackdaten werden nur aus bereits übersetzten RAM-Leseadressen
  (`readlookup2`) gelesen, mit Seiten-, Ausrichtungs- und
  monotonen Frame-Grenzprüfungen. Nicht lesbare Wörter ergeben
  `FFFFFFFF`, niemals eine erfundene Rücksprungadresse.
  Keine Original-VXD-, CPU-Code- oder EMU8000-DSP-Änderung.
- `tools/p003_join_original_guest_cpu_chip_clocks.py` akzeptiert
  V3 und V4, fordert weiterhin **exakte 1:1-Übereinstimmung** der
  originalen CPU-/Chip-Portereignisse. V4-Rücksprungadressen bleiben
  bis zur Bytecode-Verifikation ausdrücklich **unbestätigte Hinweise**.
- `tools/p003_verify_original_vxd_live_callers.py` liest die
  echte `SBAWE.VXD` **nur privat**, prüft den bekannten SHA-256,
  rekonstruiert LE/LCOD und bestimmt die **sessionspezifische
  LCOD-Ladeadresse aus 12 bekannten Original-I/O-Opcode-Sites**.
  Kein festes Load-Bias `0xC10151E5` aus einer alten Sitzung.
  Eine dynamische Rücksprungadresse wird nur dann als direkter
  Original-Aufruf anerkannt, wenn die vorhergehenden fünf Codebytes
  einen echten `E8 rel32` zum anhand des beobachteten I/O-Opcodes
  ermittelten **richtigen Original-I/O-Helfer** bilden.
  Keine Protokollstelle wird durch eine falsche Caller-Zuordnung
  nachträglich „passend gemacht“.
- Der aktualisierte `tools/p003_owner_two_bank_run.py` setzt den
  V4-Modus im **privaten VM-Klon** automatisch, prüft V4-Port-/Stack-
  Vollständigkeit und schreibt die CPU-/Chip-/WAV-Spuren in die
  herkömmliche Ergebnis-ZIP; die Original-VM bleibt unangetastet.

## Prüfungen und Reproduzierbarkeit

- [86Box V4 Windows Build — erfolgreich](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43780025499)
- [Ein-Klick Windows-Testhost — erfolgreich](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43779969623)
- [V4 Source- und Negativtests — erfolgreich](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43780025507)

**Privates Komplettpaket** (nicht GitHub, wegen der Win95-VM):
`125A-P003-CREATIVE-ORIGINAL-DYNAMISCHE-AUFRUFER-V4-GESAMTPAKET.zip`;
enthält die **byteidentische** bereits vorhandene private VM-ZIP und
Gast-EXE, den neu gebauten 86Box-V4-Recorder, den aktualisierten Host,
CMD-Starter und SHA256-Manifest. Alle sieben ZIP-Einträge und Hashes
gegengeprüft.

## Evidenzgrenzen (unbedingt beachten)

**Noch NICHT gezeigt:** tatsächlich ausgeführte originale MIDI-Parser-,
NoteOn-/NoteOff-, Descriptor- und Stimmenallocator-Pfade; originale
Funktions-Eintrittszeitpunkte oder aus den x86-Instruktionen
abgeleitete exakte CPU-Zyklen; beliebige MIDI-Noten/Programme/SF2
und autonome VST3-PCM-Bitidentität. V4 beobachtet zunächst die
**Rücksprung-Kette bei I/O-Callbacks**, nicht alle Funktions-Eintritte.
Die Instruktions- und Caller-Verifikation muss erst an einem neuen
Besitzer-Win95-Durchlauf mit realem V4-Protokoll erfolgen.

Bisherige authentifizierte Quelle-clocked P003-Referenz bleibt
`160000/160000` PCM16-Werte gleich. Autonomer VST3-Modus
bleibt **NO-GO**; kein Merge/Release/ASIO-Endtest.

### Nächster Nachweis

Ein einziger neuer kompletter Owner-V4-Win95-Testlauf, hochgeladene
`P003-...-ERGEBNIS.zip`, dann private originale VXD-Byteprüfung
und Auswertung der **tatsächlich beobachteten CALL-Return-EIPs**
und CPU-Abstände. Erst anschließend kann die native driver-seitige
MIDI-/Stimmensteuerung aus echten Original-Profilen statt aus
notenspezifisch geschätzten Taktmustern weiterentwickelt werden.
