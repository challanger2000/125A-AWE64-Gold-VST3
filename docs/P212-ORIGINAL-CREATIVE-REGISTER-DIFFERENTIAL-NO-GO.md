# P212 — Direct original Creative Win95 SBAWE.VXD vs native EMU8000 source: NO-GO on SF2

## Private-original provenance and test scope

The owner's *actual* 86Box Win95 Creative driver EMU8000 OUT register
capture was compared LOCALLY, without copying any private reference bytes,
ROM, driver, sound bank or guest disk into public GitHub or CI.

Matching bank/program/key from original guest:

- bank 0, ROM, program 7, MIDI note 60: **76 real hardware OUT writes**;
- bank 1, owner's genuine SF2, program 7, note 60, **four physical
  layered voices: 304 original OUT writes**.

The original capture was recorded from the genuine Creative Win95 driver
and 86Box C chip. The candidate reconstructed Win95-driver source was
the exact source set compiled for P211 on Windows. It ran locally through
the unchanged original 86Box C EMU8000 chip with the SAME original
owner ROM/SBK/SF2 media. This compares chip port tuples, not Windows
VST3 DLL audio callbacks. Local inputs were SHA checked, and no owner
assets were published.

## Reproduced original-driver deviation and repair

P211 source was writing **324** register OUT tuples for the 4-voice SF2
case and **78** for the single-voice ROM case. Private guest has
**304** and **76**, respectively.

Every native Win95 voice main included an unjustified SECOND
DCYSUSV=0x00FF stop when its VXD allocator had already stopped the voice.
Further, the SF2 3D voice reservation had already reset VTFT but
the main wrote TWO more resets where the original uses ONE.

The P212 source-only correction removes the redundant main stop and
only the redundant SF2 VTFT write. No original C chip DSP source,
power-on register defaults, ROM, sample PCM, RAM, editor, state or
other SoundFont generator mapping was changed.

The strict private comparison after the fix under MIDI
note 60, velocity 110 and explicitly dry Reverb Send 0% yielded:

| Source | Guest OUTs | Native OUTs | Port/selector order mismatches | Value mismatches |
|---|---:|---:|---:|---:|
| ROM bank 0 | 76 | 76 | 0 | **0** |
| Four-layer SF2 bank 1 | 304 | 304 | 0 | **61** |

The ROM parity is **register port+word order equality ONLY**, not
PCM parity or cycle/frame exact hardware timing.

The SF2 original 61 register-word differences include original
ATKHLDV and ATKHLD, IFATN cutoff, TREMFRQ/FM2FRQ2 frequency,
ENVVOL, IP/pitch, sample loop-address +/-1, VTFT/CVCF original
attack-volume initialization and DCYSUSV voice start. These are
audibly consequential. We have NOT inferred which individual
conversion step is solely responsible, and must NOT hard-code the
observed register output for just this one preset. Original
LCOD/PCOD generator math and native bank merge deserve a true
source/driver-derived repair over all owner SF2 fixtures.

## Original temporal behavior is NOT reproduced

The real Win95 guest SF2 port-write timestamps span **eight distinct
44.1-kHz chip frames**, whereas the native comparator submitted all
writes in a single chip frame. For ROM, original 76 words span two
frames; native writes again appear in one. Even if all SF2 word values
became equal, **PCM bit-exactness would not follow** from a frame-free
port tuple comparison. Actual self-generated Creative CPU cycle/
chip-frame scheduling is NOT established and must NOT be substituted
with a replay of the private recorded timestamps.

The available original guest result contains original output levels,
but NOT a directly aligned original program-7 WAV file with identical
NoteOn/NoteOff framing; do not claim an audible WAV bit-compare pass.

## Full native Windows VST3 regression after P212 correction

**Exact compiled/tested source SHA:**
`6935ec7b7b1fd4f1f32755ffe07e0f49317a3f4e`.

**GitHub Actions run #45083092888, job #137395440900:
COMPLETED SUCCESS (22 November 2026).**

- Original 86Box chip source preserved; P212 patch applied fail-closed.
- Steinberg validator **47 PASS / 0 FAIL**.
- Both 125A Plugin Testers **41 PASS / 0 WARN / 0 FAIL**.
- P204 three-zone original physical voice reserve chronology and its
  intentionally wrong interleaved control PASS.
- P205 exact allocator VTFT reset and missing-reset mutant PASS.
- P210 31/32/33/37 layer physical voice limit with bad voice0 alias
  negative control and zero-heap parallel original-chip callback PASS.
- P211 direct vs restored 37-zone **Windows VST3 PCM** parity PASS.
- Multirate, EditorHost, real hosted GUI, MIDI/state and interrupted
  Win64 installer integrity checks all PASS in this exact run.

The earlier same-source run #45082765689 failed ONLY its P205
negative-control expected-error-string assertion after the now
authentic guest-derived oracle was renamed. The exact causal wrong-
reset mutant was correctly detected; the follow-on full run repaired
the test's string check without weakening its detection.

No genuine private ROM/Creative driver/user SF2/guest files were
uploaded into public Windows Actions; their SHA-checked data were
used for the original comparison ONLY on the private local runner.

## Release verdict

**Original Creative sound parity: NO-GO.**
**Native Windows functional regression of this specific correction: PASS.**
Real Win95 driver PCM/CPU timing, SF2 voice-generator values and a
final owner DAW/ASIO roundtrip remain unproven. Do NOT mislabel this
functional source fix as an original-accurate finished VST3.