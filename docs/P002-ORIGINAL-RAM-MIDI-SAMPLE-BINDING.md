# P002 — real Creative RAM SoundFont MIDI note to physical shdr sample binding

This document records independently executed native source parsing against
the owner's **actual** `Synth 512 Bank.sf2`, not a hand-typed sample map.
Original bank SHA-256:
`d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b`
(555,754 bytes; 83 actual RAM-mono sample headers, 260,921 pool words).

The original Creative **MIDI CC0 MSB=1** selects user-bank slot #1.
Within this selected SF2's actual `phdr`, the internal preset bank is
**0** and program **0** is `Grungy Ramp Bass`. They are different
numbering systems; do not offset every `phdr` bank to 1.

## Independently reproduced native note-to-sample binding

Native C++ `candidateOriginalSf2RamMonoNotePcmPlan` and the new
`candidateOriginalSf2RamPhysicalNotePlan` read the owner's real SF2,
select program 0, velocity 100, and keys C4 (60) and D4 (62) without
hardcoding a preset or copying any original driver events.

For **both** notes the result is **two layers**, each using SF2 sample
**ID 82**, `Saw_a1`. Thus four source SF2 layers agree with the four
independent original Windows-95/Creative physical voice groups:

| Original route | Key | Physical voices | Matched SF2 sample IDs |
| --- | --- | --- | --- |
| WinMM | C4 / 60 | 0, 3 | 82, 82 |
| AWE_SEND_MIDI | D4 / 62 | 6, 9 | 82, 82 |

The previously recorded independent source Win95/86Box chip events on all
four voices have CCCA **0x23F8CB**, PSST **0x23F8D7** and CSL
**0x23FA68**. Source SF2 sample 82 + the actually measured hardware
pool start **0x200010** gives:

| Original source shdr | Physical source word | Source VXD initial NoteOn |
| --- | --- | --- |
| start = 260287 | 0x23F8CF | CCCA = start − 4 = **0x23F8CB** |
| loopStart = 260296 | 0x23F8D8 | PSST = loopStart − 1 = **0x23F8D7** |
| loopEnd = 260697 | 0x23FA69 | CSL = loopEnd − 1 = **0x23FA68** |

This is **four independently recovered MIDI-to-RAM sample bindings**,
not merely the weaker check that addresses happen to fall somewhere
inside the allocated bank. The source result does not claim all other
SF2 samples are now independently matched or that C4/D4 are sample- or
bit-exact in the final native VST3.

## Code and negative controls

- `tools/p002_verify_original_ram_note_geometry.py` now rejects any
  original-trace voice that does not bind to **one unique real SF2 shdr**
  start/loop geometry. It separately authenticates the original guest
  CC0 result, physical bank region and note time ranges.
- `tests/test_p002_verify_original_ram_note_geometry.py` includes a
  83-header synthetic bank with **sample 0 starting at word zero** and
  a forged-one-word-loop-shift negative case with matching guest FNV.
  Sample index zero must never be rejected merely because the source
  VXD's `-4` is relative to **physical** EMU8000 RAM, not the SF2 pool.
- `CreativeVxdSampleAddress` adds a separately source-gated RAM
  CCCA/PSST/CSL port-value builder for a supplied original
  Q/pan/chorus/voice; it does not create a new DSP or physical allocator.
- `CreativeSf2RamSamplePcm` adds a bounded, allocation-free
  bank/program/key/velocity-to-physical-RAM candidate plan, with exact
  28 MiB chip memory bounds and source-upload provenance gate.
  No unsupported bank is silently remapped into ROM. The owner's seven
  real +2 zero-word loop guards remain valid at the SF2-read stage.
- Native tests cover the source sample 82's four physical voices,
  0-relative sample starts, a valid two-word guard, missing source
  authentication, invalid memory ranges, wrong pool size, wrong program
  and ROM sample rejection.

The isolated **unchanged original chip** Linux build completed **32/32
CMake CTests PASS**; source-geometry Python regression completed
**5/5 PASS**. The separate native C++ executable run against the
owner's real private bank reported **4/4 source MIDI-layer RAM
address matches** for C4/D4. No source SF2/ROM/Windows/trace binary data
were committed or uploaded to Actions.

## Still mandatory before closing P002

The original Creative driver's independently generated **full**
NoteOn/NoteOff register streams (not copied/replayed clips) for normal
MIDI scenarios, including actual VXD allocation, filter/envelopes,
controller/state and timing, and the equal-clock native-VST3 vs original
Win95 **DRY PCM** comparison remain outstanding. The dynamic RAM
address builder is an input to these tests, not an acoustic result.
Original `snd_emu8k.c` remains unmodified; no final GUI or Studio
One/ASIO tests were undertaken. **P002 remains DRAFT.**
