# P002: First independent MIDI-to-EMU8000 IFATN register candidate

**12 November 2026. BOUNDED SUBSYSTEM PASS; P002 AS A WHOLE REMAINS NOT APPROVED.**

## Actual generated register writes

The newly implemented C++ library at src/creative_compat/CreativeCandidateRegister.{h,cpp} takes a real MIDI NoteOn velocity (1–127), explicit original-bank/zone parameters and the selected physical EMU8000 voice (0–31). It calculates a 16-bit IFATN filter/attenuation register and emits the two native hardware I/O words:

- Port E22: register index 1 (0x20) combined with the caller-supplied physical voice index.
- Port E20: IFATN = (initial cutoff shifted left 8) | initial attenuation.

This is independently generated from **MIDI and parameters**, not replayed from an original register file. It is a **small, testable part of a MIDI-to-register translator**. The original Creative 32-voice allocator is not yet implemented and the physical voice is supplied explicitly; the implementation has NOT been silently wired into the approved P001 VST3 product.

## Evidence: 254 actual Win95/Creative IFATN measurements

The true privately recorded original Creative 1996 Win95 MIDI driver and the pinned, unmodified calibrated 86Box EMU8000 generated a genuine velocity 1–127 sweep for each of GM program 0 and 40 (254 real samples), original ROM SHA256 4e143b94f758734f594ded78f4e5115635975c16fa37fabeae0baa4938ca710e. The private 254-row numeric observation file has SHA256 **b9b30b9b583c8befbf43c2f8b0f3b725f9cbf6eeca71e120a091aa8cd094d8d2** and is NOT included in GitHub.

The **one shared 127-level numerical velocity function** was derived by mathematical inversion of the actual original GM0 attenuation observations; it is not a separate preset-specific velocity table. In the second original program it predicts **all 127 independent GM40 attenuation observations** with zero mismatch. The filter rule with bank-provided cutoff and velocity gate predicts all 127 GM40 original cutoff values (including 125 middle velocities not used as endpoints) and the unmodulated GM0 cutoff. All 254 measured IFATN values match.

Independent historical corroboration: torvalds/linux at tag v2.6.12, sound/oss/awe_wave.c, voltab2 has **exactly the same numeric 127 valid velocity levels**. Index 0 is intentionally not usable for a NoteOn: a zero-velocity NoteOn is NoteOff. The historical Linux driver also documents the shared 8/3 volume conversion and a max(70,velocity) filter rule with a bank-dependent gate.

This is historical GPL source **used as an analytical reference**; its driver code was NOT copied into the VST3. The array in our C++ code was extracted separately from the owner's original Creative/Win95 numeric measurements. Before any product redistribution, verify license compatibility and the provenance of every code/data component.

Candidate formulas, using exclusively caller-supplied bank terms:
- attenuation = clamp_0_255(floor((combined_volume_index + shared_velocity_level[velocity])*8/3) + sample_attenuation)
- cutoff = velocity_filter_enabled ? clamp_0_255((max(70,velocity)*initial_cutoff + 160)>>7) : initial_cutoff
- IFATN = (cutoff << 8) | attenuation.

The first measured experiment used two parameter sets: for GM0, combined_volume_index=16, sample_attenuation=0, cutoff=178, velocity_filter_enabled=false; for GM40, 13, 0, 210, true. These are **empirical combined terms**, NOT yet authenticated offsets parsed from original SYNTHGM.SBK or SF2 bank fields. The production C++ library has NO GM-program-number conditional logic or per-preset gain patches.

## Exact original hardware port-word and timing check

The private original Creative sweep has **120,467 EMU8000 port events** (72,864 writes, 47,603 reads). Original trace SHA256 **51ca1b2065279329ec1ba7c8d181f9b8537f4bafdbe454889c98d7292b4c679f**.

For each of the 254 actual measured NoteOn voices, the original trace contains exactly the independently calculated pair: E22 selector for physical voice + E20 IFATN word, adjacent on the same chip-clock frame. Each original pair occurs 0–2 frames after the voice start:
- 123 of 254 are at start frame +0.
- 130 of 254 are at start frame +1.
- 1 of 254 is at start frame +2.

**254/254 identical voice-selector/data pairs**, no wrong register word. The new private verification tool at tools/p002_verify_velocity_candidate.py requires BOTH SHA-pinned original private CSV and the SHA-pinned original 120,467-event trace. It checks 254 numeric rows AND real E22/E20 port pairs and rejects missing/extra/wrong voice, register bit or frame. No original ROM, driver, trace or WAV enters CI.

An independent private numeric checker used the array extracted directly from the current GitHub C++ file and the unmodified original owner CSV: **254/254 IFATN outputs, 889 numeric assertions** PASS; mutations of GM40 cutoff at velocity 71 and GM40 attenuation at velocity 100 were both detected. The genuine raw trace was independently scanned for each actual E22/E20 pair with **254/254 matches**.

## Real native Windows and Test Lab evidence

[Windows CI #43128728260](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43128728260): **4/4 Release CTests**, including original P001 chip/RAM regression, Creative MIDI contract and the new native C++ MIDI-to-IFATN candidate port-word generator. **41/41 Python regression tests**; real 125A Plugin Test Lab P74/P73 headless VST3 ABI inventory success. Additional private-original port-pair verification and negative controls are now required by the new source test; their CI synthetic negative controls must remain distinct from privately measured real data.

## P002 remaining, and deliberate NO-GO scope

The candidate still needs original bank/zone source parameter decoding, original 32-voice allocation, program/zone selection, release/sustain, sample memory, pan, IP pitch, envelopes, filter resonance, FX sends, exact event/port ordering and timing. Its current true physical voice number is externally supplied, not autonomously chosen. A separately generated **complete** MIDI->chip register stream has NOT been shown equivalent to the original Creative Win95 driver, and the original chip PCM parity does not imply it.

Keep the original EMU8000 C kernel untouched, and run the existing 125A Plugin Test Lab continuously. Final Studio One/physical ASIO tests are **deferred to the completed GUI/product acceptance**. P002 PR #4 remains DRAFT; no product/release PASS.
