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
