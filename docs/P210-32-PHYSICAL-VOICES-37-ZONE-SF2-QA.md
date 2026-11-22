# P210 — 37-zone SoundFont vs 32 original EMU8000 physical voices

## Scope and private evidence

The previously provided genuine 13-file SoundFont archive was inspected
offline. In one original bank, two distinct presets each select **37**
simultaneously applicable SF2 instrument/sample zones at MIDI key **84**
and velocity **102**. Their overlapping zones have exclusive class **0**.
A total of 37 selected regions is NOT evidence that 37 hardware voices
can be audible: the genuine EMU8000 has only **32 physical voices**.

Keep these owner assets private: do not upload the original private SF2,
ROM, samples, preset metadata, output WAVs, or decompressed banks to CI.

## Bounded current source path

P203 already changed the VXD voice allocator from the historic unsafe
fallback to voice0 into a no-candidate **-1** return. P204 reserves all
voices for one NoteOn before any voice-main register writes. P204 skips
regions whose mono voice or stereo pair could not be reserved. Therefore,
a simultaneous 33- or 37-mono-zone NoteOn **should** remain bounded at 32
physical voices and omit excess source layers, not alias previously
reserved voices.

P210 is a *separate source-and-test-only proof*, not a new subjective
timbre change. No original 86Box C EMU8000 chip DSP changes, host IDs,
ROM/SBK loads, original 77 presets, or private installer changes.

## P210 Windows QA contract

1. Generate **public synthetic** SF2 files with 31, 32, 33, and 37
   overlapping mono zones on note 84. The 37-zone case has 24
   independently generated sample IDs, with later zones reusing some
   IDs (an intentionally stressful pattern; no private sample bytes).
2. The **actual compiled original 86Box EMU8000 chip** must show 31,
   32, 32, 32 distinct hardware-voice reservations/triggered mains.
   For 33 and 37 requested regions, exactly **1 and 5** excess zones
   must be rejected without voice0 overwrite or replay.
3. One allocator stop per granted physical voice must precede **all**
   main IFATN register writes. Every selected hardware voice must
   receive a true register-programming main and note trigger, and
   nonzero synthesized chip PCM must result.
4. A **test-only intentionally unsafe pre-P203 fallback** that reuses
   already reserved voice0 must fail the exact negative assertion
   `P210_EXCESS_DUPLICATE_VOICE_RESERVATION`. This mutation does NOT
   compile into the product VST3.
5. Independently compile the established **C++ operator-new callback
   allocation counter** with a 37-zone note-84 profile. At 44.1 and
   48 kHz, sequential and two-instance concurrent real original-chip
   renders must remain bit-consistent and show **zero** live callback
   heap allocations.
6. Existing P192–P209, original PCM and generator tests, the Steinberg
   validator, both 125A Plugin Testers, real GUI/host/recall, and
   installer safety tests must remain unchanged and green.

## Verification status

The four synthetic fixture files have been generated and their RIFF
chunk structure and zone/sample indices independently checked offline.

**The full newly extended P210 Windows workflow result has not been
verified as completed. Do not declare this QA gate passed from source
review or from the previously successful P209 run.**

P209's exact green run remains an earlier version and cannot stand as
evidence for the added P210 voice-limit/RT checks.

## Release gate (NO-GO)

An original Win95 SBAWE.VXD register/PCM side-by-side against owner
programs, actual ASIO playback and sustained CPU p95/p99/max in Fender
Studio Pro, and independent adversarial Codex acceptance remain required.
No final user-facing release, installer or physical-hardware parity claim.
