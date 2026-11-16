# P005 – Original Creative 288-Slot-VXD-Programmverzeichnis: Quellenübergabe

Stand 16.11.2026. **Keine privaten Creative-/Win95-Speicherbytes in GitHub ablegen.**

## Bereits implementiert

- \`src/vst3/P005OriginalCreativeVxdDirectoryImporter.hpp\` liest
  288 **originale, 32-Bit-Gast-Virtualadressen** aus dem
  VXD-Gerätezustand, beginnend bei \`device + 0x6CA\`.
- Für jeden tatsächlich vorhandenen Zeiger verfolgt der Importer die
  Originaladresse in einem vom Quellenprüfer bereitgestellten
  \`P005OriginalGuestMemorySegment {guest_address, bytes}\`.
- Die ursprünglichen, nicht rearrangierten Datensätze haben je
  **0x1A = 26 Bytes**. Relevante Quellfelder sind WORD \`+0\`
  (Originalmarker), BYTE \`+2\` (Programm), BYTE \`+3\` (Bank).
  Ein WORD-Marker \`0xFFFF\` beendet die jeweilige Liste.
- Originalzeiger = 0 bedeutet *wirklich leerer* Slot. Identische
  Originalzeiger dürfen über mehrere Slots auf dieselbe Liste zeigen.
  Die Reihenfolge aller 288 Slots und der Zeilen jedes Slots bleibt
  unverändert. Keine 77-Presetbank wird auf 128 oder 288 gefüllt.
- Die Daten werden **nur außerhalb des Audiothreads** eingelesen und
  vor Veröffentlichung atomar in
  \`P005OriginalCreativeBankCatalog\` übernommen. Fehlende Gastseiten,
  falsche Adressbereiche, Überlappung, unvollständige Zeilen, fehlende
  Endmarkierung und unbeglaubigte Quellen werden zurückgewiesen.
- Eine übernommene Tabelle ist noch kein gültiger SoundFont-Klang.
  Erst Original-SF2-Notendeskriptoren und alle extern verifizierten
  Stimmenzustände können \`sealNonRealtime()\` und anschließend die
  sourcegebundene MIDI-Runtime autorisieren.

## Was vom Original-Windows-95-Lauf noch fehlt

Das vorhandene private Besitzerergebnis
\`P003-20261116T014751Z-3594ea53-ERGEBNIS.zip\` enthält
\`Original-Register.trace\`, \`Original-CPU-IO.trace\`, DRY-WAV,
Original-SF2s und Protokolle, **aber keinen vollständigen Dump der
von den 288 VXD-Bankzeigern referenzierten Gast-Speicherseiten**.

Daher muss eine **originale** Win95-/86Box-Aufzeichnung zusätzlich
die folgenden Informationen liefern:

1. Das authentisch aufgenommene **0x1592-Byte-VXD-Geräteabbild** zu
   einem dokumentierten Zeitpunkt *nach* dem Laden der betreffenden
   originalen SoundFont-Bank, vor deren Löschung. Darin liegen ab
   Offset \`0x6CA\` die 288 Original-Zeigerwörter.
2. Für **jeden Nichtnull-Zeiger**: den echten 32-Bit-Gast-Startwert
   und die dazugehörigen originalen, zusammenhängenden 26-Byte-
   Speicherzeilen bis einschließlich **der ersten \`FFFF\`-Zeile**.
   Die Daten müssen an den Originaladressen lesbar sein. Keine
   willkürliche Umrechnung von Gast- in Hostadressen, keine
   Platzhalterbytes und kein SF2-PHDR-Ersatz.
3. Eine unabhängige **Quelle/Provenienzprüfung**, die Originaltreiber,
   Geräteabbild, referenzierte Gastseiten und den Zeitpunkt der
   SoundFont-Ladung konkret miteinander verbindet (SHA-Manifest und
   Original-Gastmessung auf der Besitzerseite). Ein SHA im Packheader
   allein genügt ausdrücklich nicht.
4. Bei mehreren Bankwechseln: eindeutige Zuordnung der jeweiligen
   Original-Gastabbilder und Programmtabellen zu **Bank1/Bank2**. Die
   Originalspur legt die C4-/D4-NoteOns vor die zweite Bankladung.

**Nur** wenn diese Aufzeichnungen wirklich existieren, wird
\`deviceAndGuestMemoryIndependentlyAuthenticated=true\` vom
besitzerseitigen Prüfer gesetzt. Die Importer-Klasse verifiziert die
SHA-Signaturen nicht selbst. Der bekannte
\`P002PrivateRomBank::exportP005UnattestedVxdDeviceImage\` liefert
derzeit ausdrücklich nur **Staging-Daten aus dem P002-Quellpack** –
keinen Ersatz für diese unabhängige Guest-Memory-Authentifizierung.

## Tests

- \`tests/p005_vxd_original_guest_directory_import.cpp\`:
  synthetische source-Virtualadressen, Aliaszeiger, originale
  Zeilenreihenfolge, Fehl-/Überlappungsfälle, Nullslots und
  Speicherbesitz nach Auflösung des Imports.
- \`tests/p005_vst3_sf2_note_stage.cpp\`:
  synthetisches 288-Slot-Gastzeigerabbild mit kompletter 26-Byte-
  Programmzeile -> \`P005OriginalCreativeBankCatalog\` ->
  portierter **Creative CC0/C0-Programmwähler** -> NoteOn/NoteOff ->
  derselbe Original-EMU8000-Chip-Port. Ohne neue DSP- oder
  CPU-Zeitformeln.
- Erster nativer Windows-Importer-Lauf:
  [43895011219](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43895011219),
  **78/78 CTests PASS**, EMU8000-/CT1745-Differential PASS.
  Die zweite durchgehende Original-MIDI-Bindung und die atomare
  Allocation-Fail-Sicherung werden separat in einem neueren
  Windows-Lauf geprüft.

**Nicht nachgewiesen:** vollständige originale Win95-MIDI/EMU8000-
DRY-PCM-Parität, alle SF2-Modulatoren/-Noten-/Velocityzustände oder
autonome Creative-x86-Zeitpunkte. PR #6 bleibt DRAFT/NO-MERGE.
