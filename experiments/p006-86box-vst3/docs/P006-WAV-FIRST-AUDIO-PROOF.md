# P006 – first original Creative audible audio, without rebuilding 86Box

Status: Windows binaries built; real guest end-to-end sound NOT verified.

## Contents of Windows CI artifact

- AWE64_Gold_P006.vst3 : native Windows x64 instrument; VST3 validator 47/47
- P006_Win95_Original_Creative_Relay.exe : genuine Win95 PE32 x86, subsystem 4.0, only KERNEL32/WINMM imports
- P006_WAV_Tail.exe : temporary host-side file tailer of authentic fork pre-mixer AWE32_WAV

## Private runtime requirements

1. User-owned 86Box AWE32 fork at pinned fee1e819..., with existing private Windows95 guest, Creative original AWE WinMM synth installed, ROM/SBK and bank.
2. Guest Win95 COM2 must be connected to the 86Box host's Named Pipe (COM) device, Server mode, pipe name 125A_P006_MIDI_DEMO; reconnect enabled. Run P006_Win95_Original_Creative_Relay.exe inside guest. Guest's WinMM enumerator MUST match original 'AWE' or 'EMU8000' output; it never selects Microsoft's soft synth.
3. Start the existing, UNPATCHED 86Box fork with host environment variable AWE32_WAV pointing to a new absolute WAV path, e.g. C:\\P006\\AWE32_LIVE.wav. This is a built-in feature of the pinned private fork. Keep 86Box executing.
4. Put native AWE64_Gold_P006.vst3 in a scanned VST3 directory and activate instrument in Studio Pro (x64, 44100Hz). It creates local shared audio mapping Local\\125A_P006_AUDIO_DEMO and connects MIDI to \\\\.\\pipe\\125A_P006_MIDI_DEMO.
5. Run P006_WAV_Tail.exe with the same WAV pathname; it reads growing 16-bit stereo PCM from the ORIGINAL EMU8000 and places it into the shared audio ring. It ignores potentially unfinished WAV RIFF size fields and validates the actual 44-byte fork header.
6. Play a C3 note. The intended trace is Studio Pro NoteOn -> MIDI pipe -> Win95 guest COM2 -> original Creative WinMM -> original 86Box EMU8000 -> AWE32_WAV -> P006_WAV_Tail -> plugin.

## Boundaries

- AWE32_WAV flushes buffered capture to disk only around once per 44100 frames. Playback may have >=1s latency and dropouts. This is *only* a first sound probe, not realtime-ready.
- Pre-card-mixer DRY chip PCM, not original AWE64 Gold analog-card output. Exact digital dry PCM comparison requires extra original reference.
- First plugin supports NoteOn/NoteOff, fixed 44.1k, one instance, no automatic VM startup, no general MIDI CC/Program Change or offline rendering guarantee.
- The full bridge works only when guest COM2 and the Creative original MIDI output have been configured and started; merely installing the VST3 is not enough.
- Do NOT publish/copy private Win95 disk images, VXD, ROM, SBK, SF2, or guest recordings to the public repository.

## Prefer direct audio once proven

fork/P006ForkAudioCapture.c and fork/apply_to_pinned_fork.py provide passive shared-memory capture directly at 86Box wavetable_poll. Original snd_emu8k.c stays unchanged. Its 980-frame production chunk (~22.2ms) still requires smaller scheduling chunks and ASIO validation.

Windows Action: https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44076025187

RESEARCH / PRIVATE TEST; no Studio Pro / ASIO or Win95 live-note result yet.
