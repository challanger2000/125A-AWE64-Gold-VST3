# P009 — Real-time Memory Allocation Elimination (P009RT)

This is a **reviewable unified diff** against *exact upstream* AWE32Emu commit
17b0578fe59909dfaa784b5531d6d1a3a80729ce, **after** P009's
apply_per_instance.py fixed the original shared EMU8000 clock. The original
86Box/EMU8000 PCM code, original Creative note-register recipe, real ROM
sample and bank generator semantics remain untouched.

- SoundFont note-zone selection reuses instance-private, preallocated vectors.
- Voice generation and DOS/Win95 voice reservation reuse private scratch.
- Peak layer count is calculated from real instrument/preset zones while
  loading; unbounded >4096 layers fail closed, rather than malloc on RT.
- Port FIFO (65,536 writes) and 980-sample output FIFO preallocate on init.
  On FIFO overflow apply *all* queued chip writes in order, never drop any.
- Chorus presets 0..7 have all line buffers reserved at native 44100 Hz.
- Native/reverb/chorus/resampler vectors reserve for <=1024 host frames and
  any supported 8000..192000 output rate.
- P009 VST3 calls Synth::PrepareRealtime(1024) on setActive(), **before**
  any audio callback. No user-ROM assets in public CI.

## Local private owner evidence (do not upload the source assets)

- Original ROM/SYNTHGM.SBK/77-program owner SF2/second owner SF2: direct
  patched CLI offline 102/102 audible and **all 102 PCM SHA256 exactly
  matched** the earlier pinned unmodified candidate (P008 102-result JSON).
- Original GM ROM piano, melodic7, kick, snare, both SF2s, velocity30/127:
  identical PCM SHA256 across all 9 individually rerun cases.
- Original-asset callback test of NoteOn/NoteOff, CC1/91/93, PitchBend,
  ProgramChange and 1/257/1024 frame blocks: **0 global C++ new/new[]
  allocations** at 44.1/48/96k, two concurrent instances with identical
  PCM hashes (multiple probes per instance, 512 events per rate).
- These are private Linux/owner-asset tests, **NOT** a Windows VST3 original
  ROM playback/validator proof or full real-time certification.

## Remaining hard gates
Windows P009 VST3 RT-allocation regression with genuine vs public fixture,
resampler correctness under host split events, bounded write FIFO overflow
adversarial test, low latency (<980 frames), full MIDI CC, project recall,
multi-instance studio host tests and legal source redistribution permission.

P007 original Win95/86Box remains untouched; PR #33 stays DRAFT.
