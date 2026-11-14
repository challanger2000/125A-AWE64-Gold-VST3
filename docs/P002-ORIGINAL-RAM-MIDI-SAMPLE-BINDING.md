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

## 2026-11-14 — Genuine owner SF2 + original source VXD through native PCOD descriptor compiler

A **separate local source-only private execution** used the actual
owner-owned `Synth 512 Bank.sf2` (555,754 bytes; SHA256
`d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b`)
and the actual original Creative `SBAWE.VXD` (77,370 bytes; SHA256
`d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`).
The LE loader extracted exactly the original 820 bytes of original
PCOD/LCOD/PDAT tables, separately SHA256-checked as
`0703643c6df3a8af8aa3ee94a272b40ff349462245f6e2491c27dc3344a1761a`.
No Creative or owner binary bytes were committed.

The native C++ source bank parser and driver converter independently
generated all **four genuine-bank 148-byte RAM voice descriptor candidates**,
using the actually observed physical sample pool at `0x200010`. Both
C4 and D4 select two physical source layers using **sample 82**. Start
`0x23F8CF`, loop start `0x23F8D8` and loop end `0x23FA69`
match the previously independently recorded original port geometry.
With *explicit neutral synthetic channel-state inputs only*
(CC7=100, CC11=127, pan=64, tuning=0), the native converted IP words
were as follows:

| Original SF2 MIDI input | Selected layer | Native PCOD candidate IP |
| --- | --- | --- |
| C4, velocity 100 | 0 | `0xE411` |
| C4, velocity 100 | 1 | `0xE3EF` |
| D4, velocity 100 | 0 | `0xE6BC` |
| D4, velocity 100 | 1 | `0xE699` |

**These are source-derived candidate descriptors, NOT independently
confirmed Creative-driver IP words or real DRY PCM!** The authentic
original 0x1592 Win95 VXD channel/device snapshot has not been
supplied to this particular offline test; using default channel state
is only a native compiler diagnostic. It must never silently become
a P002 completion or acoustic equivalence claim.

To reproduce without committing owner assets, the new
`tools/p002_make_private_original_vxd_ram_descriptor_tables.py`
strictly SHA-verifies the original user VXD and creates a NEW private
820-byte table file. The Windows x64 target
`awe64_p002_private_original_ram_descriptor_probe` independently
checks both authentic file SHAs before compiling the four native
descriptors. Windows CI **builds** the source-only executable and
tests six corrupt-source/geometry cases; it does not execute private
sources on a public runner.

## Still mandatory before closing P002

The original Creative driver's independently generated **full**
NoteOn/NoteOff register streams (not copied/replayed clips) for normal
MIDI scenarios, including actual VXD allocation, filter/envelopes,
controller/state and timing, and the equal-clock native-VST3 vs original
Win95 **DRY PCM** comparison remain outstanding. The dynamic RAM
address builder is an input to these tests, not an acoustic result.
Original `snd_emu8k.c` remains unmodified; no final GUI or Studio
One/ASIO tests were undertaken. **P002 remains DRAFT.**

## New native RAM-SF2 VST3 path (P002 research, 13 November 2026)

The source-constrained native RAM NoteOn compiler is now wired to the
**exported Windows-x64 VST3**, instead of only the offline proof tools:

- `CreativeSf2RamSamplePcm` derives the physical RAM sample word addresses
  for selected MIDI bank/program/key/velocity zones.
- `CreativeSf2VoiceDescriptor` builds real 148-byte original VXD
  format-2 generators and physical RAM sample metadata from source
  tables. Real sampleType 1 is not misclassified as ROM sampleType 0x8001;
  the original ROM-only +16 attenuation is not silently applied to RAM.
- `CreativeSf2RomBankNoteCompiler` independently generates the authentic
  61-original-VXD-typed-OUT sequence **per physical voice** (122 for
  the owner's captured two-layer note), with original live 30-voice idle
  choice, VXD marker/envelope/IP state, source NoteOff and CC64.
- `P002PrivateRomBank` is linked to the exported `P001Minimal` VST3.
  Real RAM samples must already be uploaded and read back from the actual
  SHA-pinned Win95 source RAM port stream, never guessed from raw SF2.
- `P002PrivateOriginalRamCapture` re-verifies the **full, exact owner
  SoundFont SHA256** on native RAM mode activation. The source VXD
  resource pack remains private and its 0x1592-byte initial idle state
  must come from the original guest; do not manufacture that state.

This mode is **disabled by default** and enabled only with:
`AWE64_P002_NATIVE_ORIGINAL_RAM_NOTES=1`, the original
`AWE64_P001_ROM_PATH`, the original private VXD source pack in
`AWE64_P002_PRIVATE_VXD_RESOURCES_PATH`, and the independently
authenticated three private 2026-11-13 owner RAM evidence files in
`AWE64_P002_PRIVATE_ORIGINAL_RAM_SF2`,
`AWE64_P002_PRIVATE_ORIGINAL_RAM_PORTS`, and
`AWE64_P002_PRIVATE_ORIGINAL_RAM_FINAL_PCM16`.

No `AWE64_P002_PRIVATE_ROM_BANK_PATH`, raw synthetic ROM,
`AWE64_P002_PRIVATE_NOTE_FIXTURE` or original captured 324-port note
replay is permitted concurrently with this native mode. Invalid or
incomplete sources reject VST3 activation. P001 and existing original
ROM-SF2 modes retain their old behavior.

**Important unproven gates:** Native chip register **time offsets**
across original Win95 CPU VXD execution (original two-sample NoteOn
window), full matching original guest chip state and actual
source-vs-VST3 equal-clock **DRY PCM16** have NOT been demonstrated
by these synthetic regression tests. The physical source bank address
mapping is exact for the source-attested one owner's SoundFont; no
unverified generalized Creative RAM allocator was introduced. No
Studio One / ASIO / GUI action until the final overall release phase.
P002 remains DRAFT.


## Original source VXD table cryptographic binding

The preceding native RAM-SF2 note compiler uses exact original Creative
1996 LCOD/PDAT tables, not surrogate pitch/attenuation approximations.
The user-private original 77,370-byte `SBAWE.VXD` had full-file SHA256
`d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a`.
The original LE page-mapped source yielded **820 table bytes**, in this
exact order: 59 signed instrument-template WORDs, 128 pitch DWORDs,
128 signed slope bytes, 15 negative-volume WORDs, and 16 positive-volume
WORDs. Their independent concatenated SHA256 is:

`0703643c6df3a8af8aa3ee94a272b40ff349462245f6e2491c27dc3344a1761a`

This digest was calculated locally from the owner's original VXD, not
from a fabricated fixture. **No VXD bytes or original table contents**
were published. The P002 private-resource builder now validates this
exact table hash even after checking the full VXD SHA; the exported
Windows VST3 performs the same independent **820-byte SHA256 check
during activation** before accepting the original RAM note mode.
All 59 source preset-template WORDs must additionally be zero. The
older ROM source workflow remains unchanged.

Both checks specifically close a provenance gap: the old `125ABK01`
resource pack contained a self-declared VXD digest in its header but
could otherwise be produced with unverified substitute pitch and volume
tables. The new RAM mode rejects such packs rather than treating them
as original Creative logic. The separately supplied mutable Win95
0x1592-byte device-state snapshot still requires independent provenance
and is not authenticated by this tables-only hash. This improvement
is not itself a successful original/native DRY-PCM equivalence test.
