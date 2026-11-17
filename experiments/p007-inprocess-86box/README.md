# P007 – Original 86Box AWE32 core IN PROCESS (not a wrapper EXE)

**Goal:** One self-contained `.vst3` folder, all original 86Box code runs inside the DAW process. There must be no second 86Box.exe process, serial COM bridge, P006Supervisor or WAV file playback in the finished instrument.

**First hard gate:** Can the exact pinned 86Box-AWE32 v6.0-AWE32-0.93 source (`fee1e81977824fd2d5eed4d239589dc30d251dd7`) produce a Windows-x64 DLL that can be loaded into an ordinary process without launching an emulator EXE?

This experiment replaces the CMake *target type* from executable to library **in a throwaway copy** and links the real original 86Box core, including CPU, machine, disk, timers and original EMU8000. It exports a tiny stable ABI containing symbol-address probes. It does NOT call `pc_init` / `pc_run` until lifecycle/thread/frontend safety has been established.

**Reference upstream APIs:** `src/86box.c` provides `pc_init`, `pc_init_roms`, `pc_init_modules`, `pc_run`; `src/sound/sound.c` drives original EMU8000 at WT_FREQ=44100 through `wavetable_poll()`. Emulation calls including x86 timers must run exclusively on the emulation thread (86Box thread API contract).

## Acceptance stages

- **P007-A:** Windows loader imports and resolves the pinned original CPU/emulator code from an in-process DLL. No `CreateProcess` or external 86Box EXE.
- **P007-B:** Remove SDL/Qt user interfaces and platform hooks with a minimal headless frontend while preserving the entire original PC model; initialize privately owned Win95 guest without the DAW audio thread ever waiting.
- **P007-C:** Route raw MIDI on emulation thread directly into guest Creative WinMM and original EMU8000; digital PCM out through fixed lock-free ring at exact sample offsets. No COM2 bridge, no file-based WAV.
- **P007-D:** Boot or restore private guest in background, use one 86Box engine inside a VST3 bundle and prove real MIDI note -> original Creative -> original chip -> plugin stereo.
- **P007-E:** Realtime 44.1 kHz, 64/128/256 frame ASIO blocks, transport, offline rendering and project recall; only then GUI and Plugin Test Lab. The full guest image and ROMs remain private.

**Current stage:** P007-A only. Building or loading a DLL is NOT Win95 boot and is NOT audio acceptance. The original proprietary Win95, ROM, Creative driver/SBK and SF2 are NOT committed to this repository.

## Why not reuse P006

P006 is an external-process prototype with a Windows95 serial relay and PCM bridge. That does not meet the owner's explicit single-plugin requirement. This is a separate branch. Do not merge P006 external process code into product.
