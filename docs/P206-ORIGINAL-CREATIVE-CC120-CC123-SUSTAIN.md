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
