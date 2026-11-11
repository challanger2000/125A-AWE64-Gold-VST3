# P001 – tatsächlicher Win95-Creative-MIDI-Treiberprobeversuch

Der erste originale Windows-95-VM-Lauf lieferte fast nur Stille. Ein neuer, von den echten Win95-Gastprozessen ausgeführter WinMM-Notentest hat später hörbare originale Creative-EMU8000-Samples produziert; siehe `docs/P001-WIN95-GENUINE-CREATIVE-MUSICAL-PCM.md`. Die zuvor
gespielte Cakewalk-MIDI-Datei war über den korrekten Ausgang
`SB AWE32 MIDI Synth [620]` geroutet. Trotzdem war der hörbare Notennachweis
nicht erbracht. Es wäre falsch, das bereits als erfolgreiche Note-On-Klangtreue
auszugeben.

## Unabhängiger Originaltreiber-MIDI-Stimulus

`src/win95/p001_original_creative_midi_probe.c` ist ein von uns
geschriebenes Win95-PE32-Programm **ohne MSVCRT/UCRT**. Es importiert
ausschließlich die damaligen `KERNEL32.DLL` und `WINMM.DLL`, enumeriert
die echten MIDI-Geräte mittels `midiOutGetDevCapsA`, akzeptiert genau einen
AWE-Synth-Ausgang (`AWE` + `SYNTH`), ruft `midiOutOpen` und
`midiOutShortMsg` für feste GM-Programm-0-Noten 48/60/64/67/72 und
dazugehörige Note-Offs auf. Kein von uns erzeugter PCM-Klang und kein Ersatz
für das originale Creative-Treiberverhalten.

Das Programm protokolliert seine Entry-Point-Ausführung, Gerätenamen,
WinMM-Rückgabecodes sowie alle Nachrichten in `C:\P001MID.LOG`. Nur wenn
diese privaten Logdaten **und** die unabhängig von 86Box erzeugte
`EMU8K_TRACE` **und** ein **hörbar nicht stummes** `AWE32_WAV` vorliegen,
kann die anschließende Prüfung mit `awe64_replay` den MIDI-ROM-Klangpfad
als getestet markieren.

Das Linux-CI baut lediglich das Win95-kompatible `P001MID.EXE`,
prüft PE32-Subsystem 4.0 und ausschließlich KERNEL32-/WINMM-Imports
und lädt **nur diese selbstgeschriebene Win95-Testanwendung** als
kurzlebiges Artefakt hoch. Keine Creative-Bank, kein proprietärer Treiber,
kein Windows-95-Datenträger und keine ROM-Samples werden veröffentlicht.

**Abnahme:** Tatsächliche Win95-Gast-Ausführung, fünf originale MIDI-NoteOn/Off-Paare, ein deutlich hörbares digitales Chip-Signal und samplegenaue unabhängige Register- und PCM-Parität sind inzwischen privat nachgewiesen. Dies ersetzt weder Hardware-Line-Out noch die übrigen GM-/SF2-/DAW-Abnahmen. Die Gast-EXE schrieb keinen abschließenden Erfolgsmarker, weshalb eine saubere Gastprozess-Terminierung nicht behauptet wird.

## Nachtrag – tatsächlich ausgeführter Original-Win95-MIDI-Test

Die Testanwendung wurde im Win95-Gast ausgeführt: echtes Creative-WinMM-Gerät SB AWE32 MIDI Synth [620], fünf NoteOn/NoteOff-Paare alle mit Rückgabecode 0; private Gastlogdatei und 86Box-Register-/WAV-Aufzeichnung liegen vor. Der separate native Chip-Replay verglich **12.861.520 Original-PCM16-Samples exakt**, davon **398.777 nicht stumm**, eine musikalische Ein-Bit-Mutante wurde korrekt verworfen. Das Testprogramm verursachte erst nach Ausgabe der Noten eine ungültige Operation beim Abschluss, daher ist dessen sauberer Exit ausdrücklich **nicht** nachgewiesen. Vollständige Messungen: docs/P001-ORIGINAL-CREATIVE-VM-MUSICAL-PCM-PASS.md.

## Korrigierter Pentium-P54C-Test – gültiger zweiter Lauf

Der originale i686-Compiler erzeugte CMOVNE (0F 45 C2), auf Pentium P54C illegal: Win95-Absturzadresse 0x004014B9 nach fünf erfolgreichen Notenpaaren. Der neue Build benutzt -march=pentium -mtune=pentium und prüft PE-Disassembly gegen CMOV. In der echten Win95-VM mit AWE64 Gold (28 MiB RAM) war anschließend der Abschlussmarker P001_GUEST_WINMM_MESSAGES_SENT_OK vorhanden, ohne Fehlerdialog. Die zweite von 86Box selbst erzeugte Register-/WAV-Aufnahme wurde unabhängig vom nativen Adapter mit **8.972.880 von 8.972.880 PCM16-Samples identisch** abgespielt. Negative hörbare Sampleänderung bei Frame 2.933.838/Kanal 1 führte zu Exitcode 11. Details und exakte SHA256: docs/P001-ORIGINAL-CREATIVE-VM-MUSICAL-PCM-PASS.md.
