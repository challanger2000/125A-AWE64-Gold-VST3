# P211 — 37-zone original-chip VST3 audible project recall parity

## Actual Windows VST3 defect, not merely a parser label

The Windows x64 VST3 was driven through genuine Steinberg host APIs with
four exclusively synthetic SoundFont files sharing identical 24 sample
headers and PCM. Only the number of overlapping zones on MIDI note 84
differed: 31, 32, 33 and 37. The original 86Box C EMU8000 core remained
untouched, with exactly 32 physical hardware voices.

The original P211 Win64 host QA established direct-play hashes:

| Selected SF2 zones | Stereo PCM FNV hash |
| ---: | --- |
| 31 | 7C9266B24D335325 |
| 32 | 3DEDA12A36D27B25 |
| 33 | 3DEDA12A36D27B25 |
| 37 | 3DEDA12A36D27B25 |

But after accepted V5 external SF2 SHA/path restore, the 37-zone original
chip produced the DIFFERENT hash CF021C66EFFAD725. This happened for
31 to 37, 37 to 37, and a separate new 37 to 37 plugin instance.
Source 0b5459b516ce3b96a5b2417c02cf5dbd4f5e8873 failed Windows
run 45077035378. The independent 3-path diagnosis was reproduced on
source b7c5ba78dc88ea287fe7b1f0c87a58274be88c61 (run 45077741414).

## Root cause and production repair

A silent V5 project reopen had been applying new project CC, RPN, FX and
program restoration AFTER replacing its silent old Synth with a freshly
constructed original 86Box chip. That reset unnecessarily wrote chip
hardware registers on the brand-new voice set before the first note.

Now, only when the outgoing Synth has produced NO audible PCM and no
prior crossfade is pending, the project state is applied to the silent
old Synth BEFORE swapping the new candidate. The new original 86Box
core receives the correct project bank/program/controller state once,
directly from its pristine initialized hardware registers.

For an ALREADY AUDIBLE outgoing chip, the original 15-ms genuine chip
crossfade order is left unchanged; never rewrite old channel controls
during the outgoing tail. No chip DSP, original ROM, original bank,
GUI parameter IDs, V1-V5 state format, send rule or original voice
allocator changes.

## Verified full native Windows source

Exact tested Windows source SHA:
ee4bc8d2c78c58f9c75a3e30849496499d24c6a3

GitHub Windows Actions #45078434780, job #137380819409:
COMPLETED SUCCESS.

In the real Windows VST3 host, after the production correction:

- 31 layers: 7C9266B24D335325
- 32/33/37 layers: 3DEDA12A36D27B25
- V5 31 to 37 restore: 3DEDA12A36D27B25
- V5 same-bank 37 to 37: 3DEDA12A36D27B25
- V5 second fresh 37 to 37: 3DEDA12A36D27B25

These three positive V5 reopen comparisons are exactly
PCM-identical to direct 37-zone original-chip playback. The host checks
external SoundFont file SHA and path roundtrip and generates genuine
original-chip nonzero stereo PCM, not browser-only or parser-only output.

All earlier gates remained green in the SAME Windows run:
Steinberg validator 47/47 PASS. Pinned 125A Tester v0.2.10
41 PASS/0 WARN/0 FAIL and v0.3.3 41 PASS/0 WARN/0 FAIL.
P210 physical 31/32/33/37 native 86Box chip 32-voice reservation
positive and unsafe voice0-alias negative PASS. P210 37-zone 44.1
and 48 kHz two-instance concurrent callback zero-heap PASS.
Original PCM/multirate, SF2, runtime state, GUI/EditorHost, release
tails, and Win64 interrupted installer rollback PASS.

Only public synthetic ROM/SF2 went into Windows CI. No original owner
awe32.raw, SYNTHGM.SBK, personal SF2 sample data or other private media
was uploaded. The P211 synthetic fixtures are cleaned out of the
temporary VST3 host test resources after QA.

## Remaining owner acceptance limits

The genuine Windows-95 Creative driver has NOT been run in these GitHub
actions, so sound parity against the Win95 guest on private banks is
not demonstrated by this success. Real ASIO Fender Studio Pro sustained
playback/CPU/latency/project reopen on the owner's PC is NOT RUN,
and independent Codex full audit approval is NOT obtained.

P211 is a verified significant correction, NOT a released/fully
accepted instrument or permission to publish the owner's copyrighted
original ROM, Creative assets or SoundFonts.