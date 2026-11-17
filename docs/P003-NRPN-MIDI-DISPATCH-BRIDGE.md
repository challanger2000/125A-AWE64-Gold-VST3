# P003: Original-Win95 NRPN dispatch integrated into native C++23 MIDI Engine

Status: **research integration, NOT original complete, NO-MERGE**.
Original owner's SHA-pinned SBAWE.VXD v4.38.0.2:
d343f2428ab3e593c9e42a668cf521b7801e1239792eaf7f2b11b5f179bc7c9a.

## Implemented on the real C++23 MIDI dispatch path

- CC98 -> LCOD+0x7309 (device+0xB56), NRPN LSB.
- CC99 -> LCOD+0x740A (device+0xB5A), NRPN MSB.
- Both preserve the separate channel+0x460 RPN number and call
  LCOD+0x7350 to select/reset original 30 per-channel parameters.
- The MIDI runtime reads the true 16-bit parameter value from
  owner channel+0x464 and tracks NRPN mode 0x0200 and selector
  +0x462. A callback's original EAX=1 is **not** a MIDI failure.
- RPN CC100/101 keep the same attached private device's
  +0x45E/+0x460/+0x464 words synchronized after an NRPN switch.
  RPN CC38 updates raw +0x464 as well.
- CC121, whose original x86 at LCOD+0x6C68 conditionally invokes
  device+0xB5A with third argument zero, now uses the authentic
  installed callback if present. First clear +0x45E/+0x460/+0x462,
  carry out the existing controller-reset hardware sequence,
  then invoke LCOD+0x740A(0): it writes mode 0x0200 and resets
  the 30 original parameter slots.
- A source-verified absent original callback still supports the
  preexisting NULL branch. Conflicting source-vs-absent claims
  are rejected before mutation.
- **CC6 and CC38 in NRPN mode remain fail-closed (MissingSource)**.
  Original LCOD+0x6B69..+0x6C11 calls device+0xB52
  (LCOD+0x70E5) to update EMU8000 register parameters. That path
  is source-ported separately but has NOT been authenticated as
  part of this MIDI runtime, so simulating it is forbidden.
- No new software synth, DSP, timing scheduler or fabricated SF2 state.

## Source contract

New fields in OriginalCreativeMidiEngine.hpp:
- OriginalNrpnMemory original_nrpn_memory; non-owning mutable
  std::span<uint8_t> of the *owner-managed* original Win95 device.
- bool original_nrpn_callbacks_installed_verified; caller policy assertion.
- The device must be at least 0x151E bytes, owned for the full
  MIDI runtime lifetime, and stable across audio callbacks.
- Attestation must validate the private original driver/session
  and actual callback installation. A bool is NOT authentication.
- Default source = null/unverified; **the production VST3 is not
  automatically switched**. This is an integration seam, not yet a
  complete source binding. VST3 host-side memory ownership and
  validated 30x6 table attachment remain prerequisites.

## Tests and limits

On the local 125A C++23 port snapshot:
- GCC Release CTest: **27/27 PASS**.
- Clang 17 + AddressSanitizer/UndefinedBehaviorSanitizer:
  **27/27 PASS**; strict Werror build of the NEW bridge target PASS.
  The whole older Clang suite with global -Wconversion -Werror
  had an unrelated preexisting test warning at
  test_original_creative_midi_engine.cpp:406 (implicit conversion in
  initializer list); the sanitizer suite without that unrelated
  warning policy was clean.
- The three updated source files were verified identical to the
  corresponding GitHub research-branch blobs by their Git SHA.
- New regression exercises direct dispatch CC98/99, RPN switching,
  CC6/CC38 NRPN fail-closed, CC121 with installed B5A callback,
  30 parameter resets, missing/truncated owner image, conflicting
  NULL-vs-present assertion, and distinct multiple instance state.
- Entire Win95 guest live x86 -> C++23 port-register trace,
  autonomous TSC scheduling, and native VST3 dry PCM parity:
  **NOT VERIFIED**.

This PR must not merge into Codex PR6 without source-backed
Win95 reference execution and Windows-MSVC/real host validation.
