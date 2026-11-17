# P007Q — Real-time pause/resume and private Creative PCM

**Date:** 2026-11-17. **Scope:** draft Windows-x64 single-folder `.vst3`, same original source-pinned 86Box/EMU8000. No extra emulator executables, processes or 86Box replacement DSP.

## Reproducible public Windows CI

[Windows build 44133100351 — PASS](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44133100351):
- Audio FIFO wrap, 16-bit conversion, saturation, normal sample continuity, new pause/resume trim, resetting trim counters: `P007_INTERNAL_ORIGINAL_86BOX_WAVETABLE_PCM_RING_PASS`.
- Native Windows VST3 and complete in-folder original 86Box DLL built and LoadLibrary tested. No additional EXE required by the product.
- Steinberg validator **47/47 PASS**.
- Private VM negative checks preserve existing disk; an incomplete install cannot masquerade as a valid Windows95 disk.

## Local PRIVATE original-Windows95 test — **PASS** (not in public CI)

A Linux-native QA host using the exact original 86Box source and P007Q audio-ring source, **the original private Windows 95 guest HDD and installed Creative AWE64 Gold drivers**, intentionally never called `p007_audio_pull` during the 40-second Win95 boot. After the **actual guest's Creative WinMM READY**, the host first ran the exact same consumer-only backlog trim called by the production VST3 audio callback:

- Full FIFO: **65,536 frames** waiting (stale initial audio).
- Trim action: **63,488 stale frames discarded** and **2,048 frames retained**.
- The producer-drop counter was **1,678,864 frames**, **expected** since the test intentionally denied all host audio callbacks before Creative READY. Normal production DAW streaming should keep this counter near zero.
- Original Win95 automatically passed BIOS F1 and post-Windows network modal with product boot helper.
- Original Creative READY + host NoteOn MIDI accepted: `slice=39562`.
- Original Creative NoteOff accepted: `slice=42563`.
- **269,654 nonzero original EMU8000 PCM values** after NoteOn, `peak=0.02456665`.
- Test process returned zero: `P007_PRODUCTION_GUEST_AUTOBOOT_ORIGINAL_CREATIVE_NOTEON_PCM_PASS`.
- Original private guest image was copied into a temporary research directory and not edited in place.

## Private packaging

A new `125A-AWE64-Gold-P007Q-PRIVATE-RT-STABIL-20261117.zip` research ZIP was created **locally only**, **one** `AWE64_Gold_P007.vst3` directory (89.99 MiB compressed), replacing 21 native CI files without changing the **512 MiB private guest HDD**. Full ZIP CRC, original guest relay, Creative VXD/DRV, ROM and SHA256 image equality were checked. No licensed binary is uploaded to GitHub.

**Still unverified:** authentic Windows x64 VST3 audio in Studio Pro/ASIO and GUI acceptance; multi-instance ownership and long-term persistence across upgraded guest binaries; no claim of final product release.
