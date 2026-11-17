# P006 – 86Box-AWE32 + Original-Windows95-Creative -> Windows-x64 VST3

STATUS: PROTOTYPE; not yet verified with real guest audio. Private/Freeware research.

This avoids all native C++23 Creative driver reimplementation. The ORIGINAL
Windows 95 Creative AWE driver runs inside the existing 86Box-AWE32 guest.
The original upstream EMU8000 snd_emu8k.c remains unmodified.

## Source-confirmed attachment points

86Box src/char/char_pipe.c: built-in Named Pipe (COM) port backend.
Configure guest COM2 -> Named Pipe (COM), path 125A_P006_MIDI_DEMO,
Server mode and reconnect enabled. Guest helper reads COM2 and calls
WinMM midiOutShortMsg on the ORIGINAL installed AWE synth. The unrelated
MPU-401 external MIDI input is NOT automatically the Creative software synth.

86Box src/sound/sound.c: wavetable_poll at 44100 Hz, stereo, WTBUFLEN 980
(approx 22.22 ms produced per block). P006 optional fork patch adds a
passive sink into Win32 shared memory BEFORE original OpenAL output.

Native VST3: noteOn/noteOff -> lock-free MIDI queue -> dedicated Windows
named pipe worker -> guest serial relay -> original Creative WinMM.
Fork wavetable PCM -> single-producer shared float32 ring -> VST3 stereo.
No file I/O or Windows pipe call in VST3 process().

## What has actually passed

GCC14 C++23 transport tests PASS; Clang AddressSanitizer/UBSan PASS.
Windows Server 2022 x64 MSVC bridge transport PASS.
Windows Server 2022 x64 Steinberg VST3 built and validator 47/47 PASS
(GitHub Actions 44073649709). These are NOT guest audio acceptance.

## What remains before it is playable

1. Rebuild a disposable private pinned 86Box fork with the passive hook.
2. Compile and execute the Win95 32-bit guest helper in the private VM.
3. Confirm COM2 arrives at the installed original Creative AWE WinMM.
4. Confirm MIDI note -> original EMU8000 -> VST3 PCM at 44.1 kHz.
5. Verify ASIO 64/128/256, transport, latency, sample positions and offline.
6. Remove default 980-frame wavetable production block or account for it.
7. Add per-instance VM startup, unique pipe/mapping, controller changes/GUI.

The current demo is intentionally SINGLE INSTANCE. Shared map name is
Local\125A_P006_AUDIO_DEMO and MIDI pipe name is
\\.\pipe\125A_P006_MIDI_DEMO. It does NOT autostart the companion VM.

Private owner Windows 95 disk, Creative VXD/SBK, ROM, SF2 and captures
MUST NOT be included in public GitHub source or CI artifacts.

Code paths: guest/P006_Win95_NoCRT.c (PE32 no modern runtime),
fork/apply_to_pinned_fork.py (exact upstream commit/hash guard),
fork/P006ForkAudioCapture.c (passive hook), bridge/* (ABI),
vst3/* (native wrapper), tests/test_bridge.cpp (CI).

This research branch does not modify Codex PR6 or reverse-engineering PR11.
