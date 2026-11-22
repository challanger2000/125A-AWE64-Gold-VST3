# P206 — Original Win95 CC120 / CC123 sustain and release semantics

**Source-backed finding:** the independently reconstructed original
SBAWE.VXD `OriginalCreativeVxdLive.cpp`, function
`original_channel_all_notes_off` (LCOD+0x6C82), shows:

- CC120 ignores the sustain pedal and calls the original envelope
  release routine for *both* EMU8000 envelopes; it does **not** perform
  the native `KillVoice` hard mute.
- CC123 consults sustain. If pedal is on, physical key marker becomes
  `(channel << 8) | 0xff` with **zero hardware envelope writes**.
  When pedal is released, both EMU8000 envelopes release for each
  channel-matching physical voice. Without pedal, CC123 releases now.
- Both commands walk original 32 *physical key marker* rows; not
  generic `inUse` or a stale channel field. Released voices are
  marked `0x0ffe` and are never retriggered by a repeated CC120.
- Existing DOS/SDK path, original unmodified 86Box chip and
  VST3 host state/GUI are unchanged. Source-backed correction is
  restricted to `Awe32::Driver::Win95`.

**Native Windows acceptance:** the actual original 86Box C chip and
unchanged 28 MiB RAM driven through three publicly generated valid
SF2 fixtures (three mono, mono + stereo, stereo + mono), three
independent controller paths each (9 traces): CC123/pedal ON makes no
hardware releases; pedal OFF releases all three physical voices;
CC120/pedal ON releases BOTH envelopes of all 3 physical voices with
no DCYSUSV hard-mute; CC123/pedal OFF releases all; repeated commands
do not release the same voices again. Note-held produces nonzero
original chip PCM. Test-only negative old CC implementation must
fail specifically on the CC123 pedal-ON no-release assertion.

**Not yet owner release:** private original Win95 guest captures,
original ROM/SBK all 13 SoundFonts against Windows VST3 DAW render,
real ASIO p95/p99, independent Codex full gate and license review
remain necessary; no unsupported claim of finished plugin.

## Native Win64 final acceptance — completed on 22 November 2026

**Unchanged production code/QA SHA:** `a9d7415405aeabc419168410e01ef8cd2ef86ad9`.
[Full Windows GitHub Actions #45063883625](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45063883625), job `137335639798`: **COMPLETED SUCCESS**, 45 successful steps.

**Actual original 86Box C chip port trace:** **9/9 PASS** across
- three independent mono zones,
- one mono zone and one correctly paired stereo,
- right-first linked stereo and remaining mono;

and each fixture's:
- CC123 + pedal ON: **zero** release writes; original-chip audio continues; pedal OFF releases **three** physical voices, DCYSUSV and DCYSUS, no hard mute and no double release;
- CC120 + pedal ON: ignores pedal, **releases both hardware envelopes** of three voices instead of muting; subsequent CC120/pedal OFF has zero redundant register writes;
- CC123 + pedal OFF: **three** envelope release pairs without DCYSUSV hard stops.

The independently compiled **TEST-ONLY P206 old-source mutant was killed for the exact source defect**:
`P206_WIN95_ORIGINAL_CC_NO_GO P206 original CC123 sustain must not write a release`.
This mutant is not shipped in the product VST3.

**Full retained regressions:** authentic original-chip P204/P205 stereo and allocator timing plus causal mutants PASS; P202 all eight 44.1/48k old/new PCM cases x2 and 44.1k CPU p95 reductions with PCM corruption mutant killed PASS; Steinberg Windows x64 validator **47 passed / 0 failed**; published 125A Plugin Tester v0.2.10 **41 PASS / 0 WARN / 0 FAIL** and v0.3.3 **41 / 0 / 0**; sounding P187 host-state stress **852,731 accepted / 0 stale / 0 mixed / 0 rejected**, audible original-chip energy `1280.720460930`; GUI/EditorHost real Windows two-editor test and P189 interrupted installer target/backup rollback integrity PASS.

**Release remains NO-GO.** The tests prove original-chip emulation *for these source-grounded controller and synthetic media paths*, not original Win95 guest parity on all owner media, owner private 13-SF2 live Windows VST3 sound, Studio Pro actual ASIO p95/p99/max, or independent full Codex release certification. No owner ROM/SBK/SF2 bytes were added to GitHub source or Actions.
