# P007 private Win95/Creative boot-to-PCM smoke (research only)
The Windows-only P007PrivateBootSmoke executable is an internal QA host,
not shipped with the product VST3 and NOT an 86Box.exe frontend.

It loads the actual 125A_AWE_Core.dll from inside the one .vst3 folder,
makes a throwaway private FAT guest HDD copy in TEMP (never edits the
template), initializes original 86Box with the configured BIOS/ROM/assets,
performs original guest CPU 1-ms slices, waits for Win95 COM2's exact
Creative midiOutOpen READY handshake and sends NoteOn/NoteOff through
the original 86Box UART. It checks original PCM ring nonzero after NoteOn,
not just MIDI packet formation.

Public CI can compile the probe and test its no-private selftest, but
cannot claim Win95 boot or original Creative PCM without private licensed
files. A positive smoke requires executing this QA process on a Windows
machine where the PRIVATE VST3 research bundle exists.

The final product must still be *only* one .vst3 folder, and user should
not be required to launch any test executable.
