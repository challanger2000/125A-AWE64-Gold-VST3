# P002 — complete pinned-fork AWE64 Gold wavetable signal path research

**Independent research branch:** `research/p002-awe64-gold-signal-path`

**Development base:** `3036e66` from `develop/p002-creative-register-oracle`

**Date:** 2026-11-13

A final read-only fetch found the development tip at `2d06381`. Its six
intervening commits change only the Win95 RAM-evidence documentation, Python
tests and RAM-trace tools; no chip adapter, audio output or mixer source
changed. The current-VST comparison below therefore remains current without
merging the developer's independent work into this research branch.

This report identifies the complete *digital wavetable path implemented by the
pinned fork*. It does not claim physical AWE64 Gold line-out parity beyond the
measurements already encoded in that fork. No DSP was invented, no calibrated
EMU8000 source was changed, and no GUI, Studio One or ASIO work was performed.

## Immutable source identity

- Fork tag: `v6.0-AWE32-0.93`
- Fork commit: `fee1e81977824fd2d5eed4d239589dc30d251dd7`
- `snd_emu8k.c` blob: `e60eb10f00a8e1187380faa25989f6ba0433d99a`
- `snd_sb.c` blob: `6d9a2da64e696237e0f3e4f210d75bc6e8ea0a63`
- `sound.c` blob: `afdab37d332cbf7e2cfc18c379175c551d3373ec`
- `snd_emu8k_trace.c` blob:
  `7d61e5c6d18261568aab0e2856a8e109f9671848`

The existing CMake gate already rejects a changed fork commit or
`snd_emu8k.c` blob. The new test includes the pinned `snd_sb.c` directly
from the submodule; it does not copy its mixer arithmetic.

## Exact path

```text
sample ROM/RAM + voice registers
  -> per-voice interpolation, envelope, filter, attenuation and volume slide
  -> per-voice stereo pan sum in emu8k.buffer
  -> per-voice mono reverb/chorus send sums
  -> fitted EMU8000 reverb return
  -> calibrated EMU8000 chorus return
  -> EMU8000 INIT-register-selected bass/treble EQ
  -> calibrated chip output factor 57737/65536 (-1.1 dB)
  -> AWE32_WAV chip tap (PCM16 clamp only in the recorder)
  -> AWE64 Gold CT1745 wavetable/FM L/R gain
  -> CT1745 master L/R gain
  -> optional CT1745 bass/treble filters
  -> CT1745 output gain L/R
  -> wavetable handler sum
  -> fork output policy:
       float: int32 / 32768, no clamp
       int16: clamp [-32768,32767]
```

The source anchors are:

- `snd_emu8k.c:2512-2534`: per-voice volume/pan and effect sends.
- `snd_emu8k.c:2777-2800`: fitted reverb, chorus, chip EQ, then
  `57737/65536`.
- `snd_emu8k.c:2806-2808`: `AWE32_WAV` is called after all of the above.
- `snd_emu8k_trace.c:98-105`: the recorder explicitly defines this as the
  chip output **before the card mixer**.
- `snd_sb.c:538-598`:
  `sb_get_wavetable_buffer_sb16_awe32()` is the actual AWE64 Gold
  post-chip callback.
- `sound.c:700-730`: the independent wavetable sink and final float/int16
  conversion.

The AWE64 Gold constructor uses `sb_awe32_pnp_init()`
(`snd_sb.c:3880-3905`), which registers
`sb_get_wavetable_buffer_sb16_awe32`. The Goldfinch callback at
`snd_sb.c:517-535` bypasses CT1745 mixing and is therefore not the AWE64
Gold path.

## Processing already present in the VST3

`Chip::render()` calls the unchanged pinned `emu8k_update()` and copies
`emu8k.buffer`. Consequently the VST3 already contains, once and in the
right order:

- voice interpolation and synthesis;
- voice filtering, envelope, attenuation, pan and voice summation;
- fitted reverb and calibrated chorus;
- the chip-level INIT3/INIT4 EQ;
- the chip-level `57737/65536` output correction;
- 28 MiB / 28672 KiB RAM mapping.

The current VST3 then calls `digitalPcm16()`: it clips the chip int32 sample
to PCM16 and divides by 32768. That exactly follows the **AWE32_WAV
diagnostic tap policy**, which is why the prior bit-identical VM comparisons
remain valid.

## Missing from the VST3

The current adapter copies the chip buffer directly; it never executes
`sb_get_wavetable_buffer_sb16_awe32()` or equivalent pinned source. It
therefore omits:

1. CT1745 register state and `sb_ct1745_mixer_write()` decoding;
2. the wavetable/FM L/R level (`mixer->fm_l/r`);
3. master L/R level;
4. optional CT1745 bass/treble branches from `filters.h`;
5. CT1745 output gain;
6. the fork's final **float** wavetable sink behavior.

At reset, items 2-5 are almost transparent, but not bit-transparent:
two successive `32767/32768` gains cause integer truncation. A synthetic
input 40000 becomes 39997, and 32767 becomes 32765. The callback itself does
not clip; the tested outputs remain above/below PCM16 until the sink.

## Exact initialization and no-double-application rules

EMU8000/chip initialization:

- `eq_bass=-1`, `eq_treble=-1`; first processing chooses bass position
  **5** and treble position **9** when INIT words do not match a table row.
  The source identifies these as the hardware/SDK default. These are
  chip-level shelves and are already active in the VST3.
- Fitted reverb initializes to preset **4 / Hall 2**, identified in source as
  the driver default. It contributes audio only when voices feed the reverb
  send.
- Chorus and all effect state remain owned by `snd_emu8k.c` and by the
  captured INIT register writes. They must not be recreated outside the chip.

CT1745 reset through the original `sb_ct1745_mixer_reset()`:

| Register(s) | Reset | Decoded wavetable effect |
|---|---:|---|
| 30/31 | F8/F8 | master L/R = 32767/32768 |
| 34/35 | F8/F8 | wavetable/FM L/R = 32767/32768 |
| 41/42 | 00/00 | output gain L/R = 1 |
| 44/45 | 80/80 | treble L/R position 8, bypass |
| 46/47 | 80/80 | bass L/R position 8, bypass |

The field name `fm_l/r` is misleading for this callback: the fork uses the
same CT1745 FM level for the EMU8000 wavetable stream.

Do **not** add a second EQ, reverb, chorus or -1.1 dB correction around the
VST3. The CT1745 bass/treble code is a separate card-mixer stage and is
bypassed at reset. If CT1745 register control is later exposed, execute the
fork's exact state and filters; do not translate its positions into the
already-active EMU8000 INIT EQ.

No captured private Win95 mixer-port trace was present in this environment.
Therefore the reset values above are proven fork initialization values, while
the mixer values later selected by the original Creative Win95 installation
remain unmeasured. They must not be guessed from EMU8000 traces: those traces
only cover EMU8000 ports.

## Reproducible exact-function test

`tests/p002_fork_awe64_wavetable_mixer_path.c` includes the unmodified
pinned `snd_sb.c` in its translation unit and calls its actual static
`sb_get_wavetable_buffer_sb16_awe32()`. Stubs delimit the input at the
post-EMU8000 buffer and suppress unrelated DSP/MPU/gameport side effects;
they implement no audio operation.

The test proves:

- the real mixer reset produces the register/scalar values above;
- exact reset-path before/after vectors, including integer truncation;
- samples outside PCM16 survive the callback, locating clipping later;
- a real CT1745 left-level mutation changes only the left channel;
- a non-neutral CT1745 bass register enters the original `filters.h`
  branch;
- the source EMU buffer is reset after transfer.

```sh
export PATH=/workspace/research-tools/bin:$PATH
cmake -S . -B /workspace/build-p002-signal-path -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DAWE64_BUILD_P001_VST3=OFF
cmake --build /workspace/build-p002-signal-path --parallel 4
ctest --test-dir /workspace/build-p002-signal-path \
  -R p002_pinned_fork_awe64_gold_wavetable_mixer_path \
  --output-on-failure
```

This research test is Linux GCC/Clang-only because it relies on GNU-compatible
linker section garbage collection to isolate a static function from the large
PC device source. It is intentionally not part of the Windows VST3 product
target.

## Integration handoff

The main developer should add a narrowly scoped post-chip adapter that reuses
the pinned fork behavior from:

- `sb_ct1745_mixer_write()` / `sb_ct1745_mixer_reset()` for exact register
  state;
- `sb_get_wavetable_buffer_sb16_awe32()` for the ordered post-chip stage;
- `sb_bass_treble_4bits`, `sb_att_2dbstep_5bits` and the exact
  `low_iir`, `low_cut_iir`, `high_iir`, `high_cut_iir` functions
  used by that callback.

Keep two explicit acceptance boundaries:

1. **Chip oracle:** preserve the existing pre-card-mixer AWE32_WAV-compatible
   PCM16 tap. This protects all previous register/PCM evidence.
2. **Full fork wavetable output:** after CT1745 processing, use the fork's
   float sink conversion (`sample / 32768.0f`) for the VST3 output. Do not
   silently clamp there; the fork's default float mode does not.

Do not import these unrelated paths:

- SB DSP PCM (`sb_get_buffer_sb16_awe32`);
- OPL/FM generation and `sb_get_music_buffer_sb16_awe32`;
- CD, line, microphone, PC speaker, capture/AGC, DMA, IRQ, MPU, gameport,
  PnP ROM or IDE logic;
- Goldfinch's mixer-bypass callback;
- global PC sound-card summation beyond the one AWE64 wavetable handler.

The generic CT1745 bass/treble source itself says it is not an exact physical
implementation. It is nevertheless the requested pinned-fork behavior.
Hardware claims for non-neutral CT1745 controls require an independent
AWE64 Gold line-out measurement and original mixer-register capture. Until
then, integrate the exact reset path and retain non-neutral controls as a
separately gated capability.

There is also a source-level asymmetry in the pinned implementation:
`snd_sb.c:591` uses `out_l` (not `out_r`) in the dry term of the **right
treble-cut** branch. The neutral reset position 8 bypasses that branch, so it
does not affect the proven default path. An integration claiming exact fork
behavior must preserve it; changing it as an apparent typo would instead be
a deliberate divergence requiring its own measurement and approval.

This report establishes source flow and exact pinned-fork execution. It does
not establish physical analog line-out parity, original-driver post-boot mixer
settings, Studio One/ASIO behavior, or release approval.
