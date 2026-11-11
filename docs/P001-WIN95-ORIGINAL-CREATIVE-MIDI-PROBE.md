# P001 – tatsächlicher Win95-Creative-MIDI-Treiberprobeversuch

Der unveränderte Chipkern und die Win95-VM haben bis jetzt ausschließlich
initialisierende Registerzugriffe und fast stummes PCM geliefert. Die zuvor
gespielte Cakewalk-MIDI-Datei war über den korrekten Ausgang
\`SB AWE32 MIDI Synth [620]\` geroutet. Trotzdem war der hörbare Notennachweis
nicht erbracht. Es wäre falsch, das bereits als erfolgreiche Note-On-Klangtreue
auszugeben.

## Unabhängiger Originaltreiber-MIDI-Stimulus

\`src/win95/p001_original_creative_midi_probe.c\` ist ein von uns
geschriebenes Win95-PE32-Programm **ohne MSVCRT/UCRT**. Es importiert
ausschließlich die damaligen \`KERNEL32.DLL\` und \`WINMM.DLL\`, enumeriert
die echten MIDI-Geräte mittels \`midiOutGetDevCapsA\`, akzeptiert genau einen
AWE-Synth-Ausgang (\`AWE\` + \`SYNTH\`), ruft \`midiOutOpen\` und
\`midiOutShortMsg\` für feste GM-Programm-0-Noten 48/60/64/67/72 und
dazugehörige Note-Offs auf. Kein von uns erzeugter PCM-Klang und kein Ersatz
für das originale Creative-Treiberverhalten.

Das Programm protokolliert seine Entry-Point-Ausführung, Gerätenamen,
WinMM-Rückgabecodes sowie alle Nachrichten in \`C:\P001MID.LOG\`. Nur wenn
diese privaten Logdaten **und** die unabhängig von 86Box erzeugte
\`EMU8K_TRACE\` **und** ein **hörbar nicht stummes** \`AWE32_WAV\` vorliegen,
kann die anschließende Prüfung mit \`awe64_replay\` den MIDI-ROM-Klangpfad
als getestet markieren.

Das Linux-CI baut lediglich das Win95-kompatible \`P001MID.EXE\`,
prüft PE32-Subsystem 4.0 und ausschließlich KERNEL32-/WINMM-Imports
und lädt **nur diese selbstgeschriebene Win95-Testanwendung** als
kurzlebiges Artefakt hoch. Keine Creative-Bank, kein proprietärer Treiber,
kein Windows-95-Datenträger und keine ROM-Samples werden veröffentlicht.

**Verbindliche Beweisgrenze:** Ein erfolgreich kompiliertes Testprogramm
beweist keine Gast-Ausführung; ein \`midiOutShortMsg\`-OK beweist nicht
automatisch eine hörbare EMU8000-Stimme. Beides wird separat verifiziert.
