# P021 – Critical 48/96 kHz authenticity defect: wrong synth core was audible

Date: 20 November 2026; isolated branch `research/awe32emu-native-isolated-20261120`. P007 Win95 VM and original chip DSP untouched.

## Status of owner-provided P020 acceptance

The owner's real Windows x64 `P020-ERGEBNIS.txt` ends in `P020_TEST_EXITCODE=0`. All 77 genuine user SF2 entries, original `awe32.raw`, Creative `SYNTHGM.SBK`, GM drums, 16 MIDI channels, host automation P014 and project recall P015 are successfully exercised **at 44.1 kHz and 64 real native chip frames**. Host advertises 12/64/70/140/279 frames at 8/44.1/48/96/192 kHz.

**Critically, the old P020 private 48/96 kHz test merely checked that sound was audible! That test did not prove it was original 86Box chip audio.**

### Root cause (direct source audit)

The upstream pinned `AWE32Emu/src/Emu8000.cpp::RenderBlock` has two routes:
- `m_outputRate == kNativeSampleRate` (44100): calls `RenderNative()` *but then* if chip is `Chip::Box86`, returns actual 86Box `m_box.RenderFrame()` PCM. So P019 original owner PCM tests and P020 44.1k host play the original chip as intended.
- `m_outputRate != kNativeSampleRate` (8/48/96/192k): the resampler fills `m_nativeL/R` with `RenderNative()`, i.e. the **secondary, approximate C++ DSP emulation**, not `m_box.RenderFrame()`. MIDI events were still delivered, the output was not silent, so the P020 48/96k smoke test incorrectly appeared authentic.

Evidence: P018 (980-window) and P020 (64-window) private 48/96k `ORIGINAL_SF2_RATE_TEST` hashes were **identical despite a radically different 86Box chip FIFO**, consistent with the native non-chip branch: 48k `48B50DC86A2BC325`, 96k `7E939E3BECD86B25`. These old hashes are now an explicit negative control in the private Windows host harness.

### P021 surgical correction

`p021_box86_native_resampling.py` refuses unexpected pinned-source drift and replaces only the `Chip::Box86 && outputRate!=44100` path. It now renders the original 44.1k `m_box.RenderFrame()` native signed 16-bit stereo and performs linear resampling **of that actual chip PCM**, with preallocated fixed-sized buffers and no changes to voice allocation, filtering, modulation, chorus, reverb or Creative registers. The 44.1k branch remains unchanged, as do the independent P019 980/128/64 timing probes, except their 8/48/96/192k behavior is corrected by the same pinned upstream patch.

`P021OriginalChipResamplingProbe.cpp` independently records 49,152 native 44.1k original-chip stereo frames and 8,192 output frames at each 8/48/96/192k rate, with original chip ROM (private) or public zero ROM and a two-layer generated SF2 (public). It compares each rate to directly resampling the independent native 44.1k chip PCM for dry and FX, **8 cases**, requiring ≤1 int16 LSB deviation and non-silent reference. This would have **failed with the old approximate native DSP**. CI archives results and does not assert provenance based on audio non-silence alone.

The original-owner Windows private VST3 host test now also rejects the two previously recorded approximate-DSP PCM hashes at 48/96k. The private owner original-ROM ChipRateProbe is stronger, because it compares directly against the 44.1k original chip, rather than only against historical negative fingerprints.

### Remaining evidence / risks

The P021 fix must pass Steinberg 47/47, original ROM public fixture probe 8/8, realtime callback memory and multi-instance tests and real Windows host silence behavior before being called build-valid. Then privately rerun genuine owner-ROM/SBK/SF2 VST3 and original-chip rate probe. **The corrected non-native host sample-offset MIDI alignment also requires further edge-case testing**, since a linear interpolator may look ahead up to two native chip samples. It would be premature to claim sample-exact event timing at every rate from the old P014 tests, which focused mainly on 44.1k.

No proprietary ROM/SBK/SF2 assets may be committed to GitHub; P007 remains untouched; no DAW Studio Pro testing until final gate.
