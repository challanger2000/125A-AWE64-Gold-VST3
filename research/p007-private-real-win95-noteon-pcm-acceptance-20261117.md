# P007-P: PRIVATE real Windows 95 original Creative -> EMU8000 PCM acceptance

2026-11-17. **Actual private OS and Creative driver tested offline in local Linux QA host**, not on public GitHub CI and not yet in Studio One/Studio Pro.

- Original CPU/IDE/UART/EMU8000 from pinned `turican0/86Box-AWE32` `fee1e81977824fd2d5eed4d239589dc30d251dd7`, in-process shared core, no separate 86Box.exe and no outside named pipe.
- Source was user-provided PRIVATE 512 MiB FAT16 Win95 disk, Creative AWE64 Gold original driver (SBAWE.VXD/SBAWE32.DRV), ROM awe32.raw and CT4540 PnP.BIN, 28 MiB original wavetable RAM.
- Research test helper `P007LinuxPrivateBootAuto` used **no hardcoded keyboard input**; it called the actual product core export `p007_core_guest_boot_assist()` on each CPU tick.
- BIOS prompt observed and answered via original guest keyboard at tick **7500** (F1).
- Windows 95 entered GUI, known network dialog answered automatically via original guest keyboard at tick **37500** (Enter).
- Original Creative MIDI WinMM helper selected and opened the original AWE device. Win95 VCOMM COM2 returned ERROR_FILE_NOT_FOUND; as tested, original guest physical 16550 x86 COM2 transmit/receive then delivered the guest READY marker.
- `P007_REAL_WIN95_CREATIVE_READY slice=39562 noteon=1`
- `P007_REAL_WIN95_CREATIVE_NOTEOFF slice=42563 success=1`
- `P007_REAL_PRIVATE_RESULT code=0 ready=1 note=1 noteOff=1 pcmNonzero=269654 peak=0.02456665 HddReads=1562 HddWrites=2642`
- `P007_PRODUCTION_GUEST_AUTOBOOT_ORIGINAL_CREATIVE_NOTEON_PCM_PASS`
- PCM samples are from the original 86Box AWE wavetable path; not a synthetic oscillator or replayed reference.
- The local guest disk was a temporary private copy; public repo contains **no Win95 image, Creative binary, PnP ROM, AWE-ROM, or SoundFont**.
- Separately, earlier manual startup path passed `P007_REAL_WIN95_ORIGINAL_CREATIVE_AWE64_NOTEON_TO_EMU8000_PCM_PASS` with `pcmNonzero=270352` and `peak=0.02462769`.

**Explicit acceptance boundary:** In-process **Linux QA** guest note-on-to-PCM proved. A compiled **Windows native VST3** is not yet independently audio-accepted in a real DAW; host audio callback cadence, ASIO integration, first-use writable path behavior, multi-instance and CPU performance remain for further acceptance. Do not call P007 a released product until completed.
