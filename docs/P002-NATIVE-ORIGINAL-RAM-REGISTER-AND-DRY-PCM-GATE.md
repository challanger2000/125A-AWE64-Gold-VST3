# P002: independent native RAM-SF2 EMU8000 port-write and DRY PCM gate

## What the exported VST3 now records

The real Windows VST3 contains an explicitly enabled native-original-RAM
source mode, `AWE64_P002_NATIVE_ORIGINAL_RAM_NOTES=1`. It runs the
source-derived note compiler and live original EMU8000 voice allocator
rather than replaying captured C4/D4 NoteOn port clips.

Only after the authentic **original RAM bank capture** and exact original
1996 VXD source-table checks have succeeded, the additional optional
`AWE64_P002_NATIVE_REGISTER_OUT_PATH=<NEW ABSOLUTE PRIVATE PATH>`
arms a **262,144-event fixed buffer** in `Chip`. Each actual native
16-bit OUT forwarded to the pinned unchanged EMU8000 is stamped with
the 44,100-Hz monotonic host render-frame count. This is a separate,
independently generated native sequence.

Buffer allocation occurs during `setActive(true)`, not during
`process()`. No allocations, synchronization or file I/O occur when
capturing in the audio thread. `setActive(false)` writes a NEW file with
`CreateFileW(CREATE_NEW)`, refusing overwrite, empty capture or overflow.
The report is intentionally owner-private and is not published to
GitHub/CI. The original EMU8000 DSP, Creative driver, ROM and user SF2
remain unchanged.

This records **WRITES only**; it does not prove original driver register
READ parity, complete original voice allocation/ownership, or PCM parity.
Its chip-frame zero is the **native activation**, not the independent
Win95 guest boot/driver clock. No clock correction or automatic alignment
is performed.

## Exact source-original Win95 evidence

The owner-provided independent calibrated-fork EMU8K_TRACE has
SHA256 `4ec6e1764afade0ae965c2ec81cae27b107ff10701c9f727b7d20e06876422f9`.
The real original guest event starts recorded previously include
C4 original WinMM NoteOn around chip frame **3,154,566**, C4 NoteOff
**3,172,293**, and D4 direct `AWE_SEND_MIDI` NoteOn around
**3,201,361** / NoteOff **3,219,527**. The existing source trace
contains RAM uploads and thousands of unrelated chip events, so the
exact note-specific frame window must be selected independently.

The owner-provided `P002-Creative-Win95-Original-Dry-C4-D4.wav` is
stereo PCM16/44,100Hz, **198,450 frames**, SHA256
`b4e97086a13cf9c85f20a6e6ec0d9555fb7c708c1d6689fd5f38e0e7c4655dcf`.
Measured audio has an initial digital-silence prefix:
first nonzero frame **38,938**. It is a concrete audible original-owner
reference, but this filename and audio alone do NOT authenticate exact
zero-FX or a common chip-clock origin relative to the source register
trace. Do not guess a sample offset from silence or normalize RMS.

## Strict native vs original physical-port comparison

Run against the separately captured **private** original Creative trace
and the genuinely emitted **private** native file (never synthetic data
for a real claim). Example for the first three original C4-on chip frames:

```bash
python tools/p002_compare_original_native_ram_note_ports.py \
  --original PRIVATE_ORIGINAL_EMU8K_TRACE.txt \
  --native PRIVATE_ACTUAL_NATIVE_VST3_PORT_OUT.txt \
  --original-origin 3154566 \
  --native-origin ACTUAL_NATIVE_NOTE_ON_FRAME \
  --frames 3 \
  --private-report PRIVATE_NEW_C4_PORT_REPORT.json
```

The native origin must come from the **host's recorded MIDI event sample
position** and must not be chosen by scanning for a passing waveform.
The comparator parses original READ/WRITE trace syntax and exact
`frame PORT VALUE` native format, selects **WRITES only** in each
caller-selected two-sided time window, and compares relative frame,
original order, 16-bit port and value with zero tolerance. It rejects
the wrong original source SHA, missing original/native events, missing
native evidence header, overflow/partial capture (no completed export),
different write counts and first differing word. It records mismatch
index/field instead of a spurious PASS. Tests deliberately mutate
timing, ordering, width-equivalent port values and the source SHA.

**A matching three-frame C4 window does not prove P002 acceptance.**
Repeat for D4, both source NoteOffs, overlapping voices, other MIDI
velocities, controllers and banks. A full native-original generated
register engine must be independently proven, not copied from the
source-original port fixture.

## Bit-exact PRE-CARD DRY PCM is the final acoustic gate

After verifying the actual native/sample chip clocks, obtain the
independently emitted **real** native pre-CT1745 signed-PCM16 WAV with
`AWE64_P002_NATIVE_DRY_PCM_WAV_PATH=<NEW ABSOLUTE PRIVATE PATH>`
**together with** `AWE64_P002_NATIVE_ORIGINAL_RAM_NOTES=1`,
the three exact original Win95 RAM upload SHA-locked inputs, the
verified original VXD source pack, and the original private ROM.
Do not enable the PCM path in ordinary P001 or ROM mode.

The real exported VST3 allocates a bounded 30-second stereo
44.1kHz PCM16 buffer on `setActive(true)` (5,292,000 bytes);
it records **every raw original EMU8000 stereo frame BEFORE the
CT1745 AWE64 Gold card** during `process()`, without disk writes,
locks, buffer reallocations, normalization, or additional DSP.
It writes a fresh, explicit PCM RIFF/WAV file with `CreateFileW
(CREATE_NEW)` only from `setActive(false)`. An empty, overflowing,
or invalid capture fails closed; no incomplete report is accepted.
Both private native WAV and native port OUT evidence are measured
against one actual activation clock and must never be uploaded.

For host monitoring, the existing `AWE64_P002_PRE_CARD_PCM_DIAGNOSTIC=1`
still bypasses the CT1745 stage, but it does **not** by itself save
a WAV. The new native `...DRY_PCM_WAV_PATH` is the direct byte-precise
recording of chip samples and does not depend on a host export,
resampler, or floating-point conversion.

Use the same genuine original ROM/RAM/VXD source state and MIDI
and run the strict no-alignment two-sided PCM comparison:

```bash
python tools/p002_compare_original_dry_pcm.py \
  --original-precard-wav PRIVATE_ORIGINAL_DR_WRITE.wav \
  --vst3-precard-wav PRIVATE_NATIVE_VST3_DR_WRITE.wav \
  --original-start-frame VERIFIED_ORIGINAL_WINDOW_START \
  --vst3-start-frame VERIFIED_NATIVE_WINDOW_START \
  --frames VERIFIED_IDENTICAL_WINDOW_FRAMES \
  --private-report PRIVATE_NEW_PCM_REPORT.json
```

That comparator forbids automatic time shift, gain matching, smoothing,
frame truncation and silence-only passes. The original WAV alone cannot
stand in for an actual exported VST3 RAM note: **full original-vs-native
DRY PCM remains unverified** until both real waveforms have been
captured from the same scenario and compared.

CI synthetic regression success means only the capture/comparison
*infrastructure* is tested. The original user's private Win95/Creative
reference evidence is never committed or uploaded. P002 stays DRAFT,
with GUI and Studio-One/ASIO tests deferred to the final end-to-end
release stage.
