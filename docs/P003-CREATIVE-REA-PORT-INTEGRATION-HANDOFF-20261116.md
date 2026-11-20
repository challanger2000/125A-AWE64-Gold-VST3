# P003 — Original-Creative-REA-Port: verbindlicher Integrationsstand

Stand: 16.11.2026. **Bestehender PR #4 / Branch `develop/p002-creative-register-oracle`; kein zweiter konkurrierender Treiberport.**

## Was die unabhängige Kontrolle wirklich geliefert hat

Das private Kontrollpaket `125A-P003-Konsolidierte-Creative-Originaltreiber-Kontrolle-V2-20261116(1).zip` enthält Prüfer, Original-LE-Aufrufbelege und synthetische Tests, **keinen lauffähigen C++-Port der vollständigen `SBAWE.VXD`**.

- Original-SHA `d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`.
- Historische Original-V4-Sitzung `P003-20261116T003509Z-d6e65f07-ERGEBNIS.zip`: 816.594 physische EMU8000-Callbacks. 816.583 CPU-/Chip-Ereignisse haben **direkte originale E8-Aufrufe mit Zielprüfung**: 25.254 LCOD, 92.354 PNPC, 698.975 PCOD, **175 verschiedene dynamisch beobachtete CALL-Stellen**.
- Drei Inline-I/O-Opcode-Events; acht frühe CPU-Ereignisse ohne gesicherte Modulzuordnung. Kein 100%-Call-Nachweis behauptet.
- Original-LE-Relokation ist bei PCOD/PNPC **unverzichtbar**: ein unrelokiertes `E8 00000000` ist kein gültiges Laufzeit-Callziel.
- 86Box `PC_after_IO`; die echte LCOD-Basis lautet `C10151E4`, nicht `C10151E5`. Originale Rücksprünge liegen auf `CALL+5`.

**Neue, separate Besitzer-Stressaufnahme:** `P003-20261116T014751Z-3594ea53-ERGEBNIS.zip` mit 1.533.305 CPU-/EMU8000-Callbacks, 4.176 erfolgreichen Original-Creative-MIDI-Aufrufen und Stereo-Caller-/Modus-1-Indizien. Sie darf nicht mit den 816.594 Ereignissen des älteren V2-Reports vermischt werden.

## Jetzt im Repository integriert

- `tools/p003_rea_three_object_audit.py` rekonstruiert die drei Codeobjekte und ihre tatsächlichen LE-Relokationen, leitet sessionspezifische Basen aus originalen CPU-Post-I/O-PCs und CALL+5 ab und fordert die zugehörige vollständige CPU/Chip-Portreihenfolge. **Keine festen Eventzähler der alten Aufnahme**.
- `tests/test_p003_rea_three_object_audit.py`: 15 synthetische Negativ-/Positivkontrollen; lokal bestanden.
- CMake/CTest führt diesen Test stets aus. Zusätzlich optional `p003_private_original_creative_rea_three_object_v4` bei vorhandenen Umgebungsvariablen `AWE64_P002_PRIVATE_VXD` und `AWE64_P003_PRIVATE_ORIGINAL_V4_ZIP`. Private Rohdaten sind nicht Teil des Git-Repositories.
- Ein vollständig ausgeführter privater 816k-/1.53M-Ereignis-VXD/LE-Gegentest war in dieser Entwicklungssitzung mangels `SBAWE.VXD` **nicht** möglich. Die Zahlen im Kontrollpaket sind Kontrollbefunde, keine erneuten unabhängigen Treffer dieses neuen Repo-Auditors.

## Erwartete echte Übergabe des REA-Treiberports

Der Kontroll-Chat soll **nicht** dieselben Auditberichte noch einmal liefern, sondern den kontinuierlichen originalen MIDI→Register-Ablauf in überprüfbarem C++23-Quellcode:

1. Erhaltene originale Treibersemantik für CC, Bank/Program, NoteOn/Off, Voice-Lifecycle, Mono/Stereo/SampleLink, Polyphonie und Voice-Steal. Jede übersetzte Routine mit Original-Objekt+Offset und den korrekten LE-Fixups dokumentieren.
2. Nur die Windows-95-VxD-/WinMM-/AWEManager-Schicht durch schmale native Host-Eingänge ersetzen; die originalgetreuen Algorithmen nicht mit heuristischen Klang- oder Envelope-Formeln austauschen.
3. EMU8000-I/O über den **unveränderten** vorhandenen Original-86Box-Kern bzw. dessen Chip-Port-Brücke ausführen. Hardwareports einschließlich 8/16/32-Bit-Zugriffbreite und originaler Registerreihenfolge erhalten.
4. Zeitfolge als expliziten, deterministisch prüfbaren Portplan mit Originalzeitbezug bereitstellen; `source-clocked` nur als Forschungsmodus. **Autonome** Treiber-VST3-Timing-Parität erst nach eigenem Gegentest behaupten.
5. Die vorhandenen `CreativeVxdAdditionalAllocatorModes`-C++-Kandidaten, den ROM/RAM-SF2-Descriptorcode und den VST3-Prozessor analysieren und direkt anschließen oder quellenbegründet ersetzen. **Nicht** einen neuen parallelen Synth-/DSP-Kern beginnen.
6. Zuerst kompilierten C++23-Funktionsport mit eigenständigen ABI-/Register-Regressionen einchecken, dann nachweisbare native VST3-Integration; keine Änderungen an proprietären Originalbinaries und **keine Original-VXD/ROM/SF2/PCM/Raw-Traces auf GitHub**.

## Produkt-Gates (keine neuen Forschungsnummern als Selbstzweck)

- **Native Noten** mit echter Bank/Program- und Stereo-/Mono-Zuteilung statt fixer Diagnose-Noten.
- Originale physische Portreihenfolge, CC/NoteOff/Steal und relevante CPU-Zeitabstände differenziell nachweisen.
- Eigene autonome VST3-PCM-Ausgabe gegen authentische Original-WAV, nicht nur Register-Replay.
- 125A Test Lab, Windows-x64-VST3 und Abschlussprüfung in Fender Studio Pro **erst bei vollständiger Integration**.

**Noch NICHT geliefert:** ein vollständiger Creative-Treiberport; autonomes VST3/Original PCM; Windows-CI-PASS auf dem neuesten HEAD. PR #4 bleibt offen, kein Merge oder Release.
