# P207 — Original Creative Win95 unsigned pitch saturation

**Finding:** Source-derived original owner Win95 SBAWE.VXD pitchbend
implementation at LCOD+0x6F56, independently reconstructed into
`third_party/creative-vxd-port/src/OriginalCreativeMidiEngine.cpp`
(`codex/p005-vxd-full-port`), performs an x86 **unsigned** comparison
to `0xFFFF` before the signed-negative condition. A negative 32-bit
pitch total wraps to a huge unsigned value and saturates to original
hardware **IP=0xFFFF**. The current native `Synth::RefreshChannel`
and `Synth::StartVoice` both use C++ signed `std::clamp(..., 0,
65535)`, incorrectly turning such pitches into **IP=0**.

**Fix:** constrain both original Win95 VXD paths to a shared
`OriginalVxdPitchWord()` helper. Compute sum in signed 32 bits and
reinterpret as unsigned for the saturate-to-`0xFFFF` check. DOS/SDK
pitch math and hardware/86Box chip code are unchanged. No surrogate
synthesizer, no alternate pitch DSP or GUI/host state change.

**Independent original-chip oracle:** on three synthetic SoundFonts
(3 mono, mono+stereo, right-first+mono), play MIDI key 0 using a public
root-key-60 reference sample so that RPN0 pitch range=127, full-down
wheel yields negative base+delta. Confirm the precise **three** hardware
IP register values and identical physical voice identities in live
pitch bend and already-bent NoteOn. Verify center restores original
IP; a test-only old `std::clamp` branch must fail exactly at the
negative-pitch condition. No private owner binaries are in GitHub.

**Gate:** full Windows original-chip port and audible-chip regressions,
Steinberg native VST3 Validator, published and development 125A
Testers, host recall stress, actual EditorHost GUI, native Win64
installer integrity. Broader real owner Creative Win95 PCM parity and
actual Studio Pro/ASIO acceptance remain separate.

## Independent actual Windows x64 acceptance (after corrected source oracle)

**Product code SHA:** `879ba4b390686ab007a6cd44a9aec2ea8b6634a8`.
**Test/QA-corrected SHA:** `1f7063e755eb24a9ee0dabb07e2c8721902e090a`.
[Full Windows Actions #45066235514](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45066235514): **COMPLETED SUCCESS**.

Three real original 86Box C chip / synthetic SF2 worklists:
`THREE_MONO`, `MONO_FIRST_STEREO`, `RIGHT_FIRST_STEREO`,
each with exactly three live physical voices. In the
`C-1` root-key-60 case, baseline IP=36864 and authentic RPN0/0
range=127 at full-down bend gives delta=-43349. Original
Win95-style unsigned saturation writes IP=`65535`; the old
`std::clamp` writes the incorrect IP=`0`. This independent
test-only old-source mutant **failed for the precise intended reason**:

`P207_ORIGINAL_WIN95_PITCH_NO_GO P207 VXD negative pitch must saturate to FFFF voice=0 original=36864 expected=65535 actual=0`

Production passes both live bend-after-NoteOn and NoteOn-under-held-bend
hardware port traces, plus center-recall and stereo+mono voice mapping.
The initial QA run #45065742725 correctly **FAILED** because the
test compared original NoteOn *stereo-worklist order* to original
pitchbend *ascending physical-voice order* by vector index. Fixed
the test, **not the production code**, to join by unique physical
voice ID, then reran full Windows QA.

Win64 Steinberg validator **47/47 PASS**; published 125A Plugin Tester
**41 PASS / 0 WARN / 0 FAIL**, development tester **41/0/0**,
P187 sounding host saves **1,126,299 accepted / 0 stale / 0 mixed /
0 rejected**, positive genuine-chip energy **1280.720460930**.
Win64 GUI/EditorHost, original-chip PCM/CPU regressions and interrupted
installer integrity **PASS**. Actual owner Win95 guest A/B, Studio Pro
ASIO CPU and independent Codex release audit are still independent
NO-GO gates.
