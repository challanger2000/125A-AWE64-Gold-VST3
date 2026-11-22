# P015 — Clean MIDI controller state on VST3 project recall (2026-11-20)

## Actual root cause

P013 saved 16-channel MIDI controllers as 0xFFFF when never touched in a
project. A fresh `Synth::ChannelState` gives those slots authentic defaults,
but calling `setState()` while a previous project was already loaded into
the same active native VST3 left old chip-side controller values intact.
For example, old CC7=0 could mute a subsequent legacy v1 project even though
its own state contained no CC7 setting. Other carryover candidates: pedal,
bend, RPN 0,0 pitchbend range, reverb/chorus send, pan, expression.

## Implementation

- Before replaying a *validated* replacement state to an already active
  synth, send the pinned AWE32Emu's original ChannelState defaults:
  CC1=0, CC7=100, CC10=64, CC11=127, CC64=0, CC91=0,
  CC93=0, RPN 0,0 range=2, RPN deselected 127/127,
  PitchBend center.
- Then replay the restored bank/program and the controller values explicitly
  stored in the incoming v1/v2/v3/v4 project.
- Never send the reset on a fresh `setActive()` path. Preserve its original
  initialization/register behavior.
- Reject corrupt incoming project data **before** any DSP state modification.
- No sample processing allocation; no changes to pinned EMU8000 DSP math,
  ROM/SBK/SF2 sample data, oscillator, interpolation, filter or FX DSP.
- The difference concerns the *transition between projects*, not emulation
  of any novel Creative controller functionality. NRPN remains selector-only.

## Verification gates

**Public Windows VST3 host CI**:
- Active VST3 receives mapped CC7=0.
- Live legacy v1 project restore upgrades to state v4, with CC7 unspecified
  (0xFFFF) and no invented NRPN.
- Live v4 restore returns the previous saved value; a malformed v4 controller
  is rejected without mutating persistent state.
- Repeat old-v1 restore to catch persistent ghost state.
- Existing SDK 47/47, zero-heap RT, concurrent-instance, v1–v4 state,
  and sample-offset test suites must remain green.
- **This verifies state transitions, not actual original ROM PCM.**

**PRIVATE owner authentic PCM gate** (not run in public CI):
- Two real native Windows VST3 instances load the owner's authentic
  `awe32.raw`, `SYNTHGM.SBK`, and owner `user.sf2`.
- Compare byte-identical float PCM for pristine `setState(target)`
  against `setState(dirty)` then `setState(target)`; old state leaves
  CC7=0, CC64 on, FX sends, bend+RPN range on CH1 and CC7=0 on CH16.
- Repeat with GM-ROM bank0 and owner 77-preset SF2 bank1, separately
  for CH1 and CH16; reject silence. Include legacy v1 after dirty v4.
- The gate lives in `P010NativeHostQA.cpp`, `--private` only.
- **NOT PASS until genuine owner assets are available and hashes match.**

## Release limits

This isolated research branch is **not** P007 and must not overwrite it.
Final release still needs independent real Win95 Creative register/PCM parity,
private P014/P015 audio acceptance, robust DAW state-thread sequencing,
SF2 bank identity/asset recall policy, legal redistribution review, and
a final real Fender Studio Pro audition. Public CI `SUCCESS` alone is
insufficient evidence of original AWE64 Gold sound.
