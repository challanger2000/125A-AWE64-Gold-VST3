# P016 — Public non-silent EMU8000 PCM recall regression (2026-11-20)

**Scope:** Isolated native AWE32Emu VST3 research branch, NOT the original
Win95/P007 VM. Original GPL-derived EMU8000 DSP and the pinned reconstructed
Creative Win95 driver unchanged.

## Production correction under test
`research/native-awe32emu/p009-vst3/P009CreativeDefaults.hpp` is the shared
original Creative channel reset routine. Production
`NativeVst3::resetMidiControllersForStateRestore()` calls this exact routine
when restoring a fully validated active project. It clears controller data
from the previous project before replaying explicit new state.

## Public PCM test (not an original ROM test)

`tests/P016PublicRecallPCM.cpp` links the same pinned native
`Synth.cpp`, `SoundFont.cpp`, `Emu8000.cpp`, `Emu8000Box.cpp`, and
`86box/snd_emu8k.c` as the production VST3. The emulator receives a 1 MiB
all-zero **synthetic** test ROM plus a generated two-layer public SF2 with
audible waveform samples; no Creative/owner ROM or bank is bundled.

The test compares non-silent int16 stereo PCM for:
1. Fresh engine + original default reset + target state.
2. Dirty prior state (CC7=0, CC64 on, pitchbend full, RPN range=24, FX sends
   etc.) + **production shared reset** + same target state.
3. **Negative control:** dirty prior state + same target, but intentionally
   omit the reset; hash must differ from (1).

All six combinations on the authentic EMU8000 chip rendering path (CH1 / CH16
at 44.1 / 48 / 96 kHz) passed in
[Windows GitHub Actions run 44779863333](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44779863333).
A test is rejected for silent PCM or a matching negative-control hash.

| Rate | Fresh = dirty→reset→target SHA-like 64-bit PCM fingerprint | No-reset negative-control fingerprint |
|---|---|---|
| 44.1 kHz, CH1 and CH16 | `CD715C70395B46CE` | `0668679189E8A325` |
| 48 kHz, CH1 and CH16 | `07FDBB2A2C7B1F49` | `0668679189E8A325` |
| 96 kHz, CH1 and CH16 | `2696B5D92C53AFAE` | `0668679189E8A325` |

These are deterministic FNV-style fingerprints of the test PCM, **not
cryptographic hashes**. The actual successful test also checked nonzero
sample energy. A later workflow change adds explicit six-case log counting
and archiving.

## Limits / NO-GO

The successful P016 public probe proves only the native original-chip
**synthetic test SF2** controller-reset parity. It does NOT exercise VST3
`setState()` into real output audio, Win95 driver trace parity, authentic
EMU8000 ROM acoustic identity, or the owner's 77-preset SF2.

The P015 private `P010NativeHostQA.exe --private` VST3 host test is separately
prepared for original ROM, SYNTHGM.SBK, and owner SF2 but cannot run on
public GitHub CI with proprietary missing files. Keep status NO-GO for private
original sound acceptance, plugin asset-identity recall, legal distribution,
and final Fender Studio Pro audition.
