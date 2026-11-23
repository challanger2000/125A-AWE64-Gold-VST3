# Original Creative Win95 EMU8000 private differential — confirmed correction, release NO-GO

## Reference, scope and privacy

The source reference is the owner's own Creative Windows-95 driver
running inside their 86Box guest with original ROM/SBK and an original
SoundFont bank. The 1996 Creative driver is NOT the 86Box public
reconstruction and is not a FluidSynth/other SF2 player.

The deterministic comparison uses the same single-bank instrument,
MIDI bank1/program7/key60/velocity110, dry signal (zero sends).
A second reference uses original hardware ROM bank0/program7/key60.
The owner's private assets, Win95 disk, Creative driver code and
recorded source trace are NEVER committed to the GitHub repository,
nor copied into the public Actions runner.

Comparison uses 86Box's authentic unmodified EMU8000 C chip and
the reconstructed native AWE32Emu SF2/VXD source, compiled locally
with the owner media. This is not yet the actual Fender Studio Pro
ASIO callback on the owner's PC.

## Reproduced original-driver register faults and corrections

| Native port comparator vs original guest | SF2 mismatched 16-bit OUT values (of 304) |
| --- | ---: |
| P212 before source-accurate SF2 generator work | 61 |
| After P213 converted instrument/preset vol attack, hold, delay | 33 |
| After P214 preset decay, Win95 LFO default and three chip-address words | 9 |
| After P215 independent x86 PCOD mod-hold and scoped vol sustain | **1** |

Native and original SF2 port selector/data write **order and count** now
agree: all **304 of 304** port tuple positions align. The corresponding
ROM bank0 case has **76 of 76** identical port selector/data/value
writes. These are hardware register *content and order* assertions.

The one remaining SF2 value mismatch is the first physical voice's
initial pitch (IP): original Creative `0xCFEA`, native `0xCFEB`.
Another voice with matching octave-normalized pitch data yields
`0xDFEB` on both implementations, so there is no justification for
globally subtracting one from pitch registers. Actual sample data and
individual bank contents remain unchanged.

### Independent numerical PCOD check

The original Creative x86 PCOD timecents conversion for hold
(operators 27/35) uses 32-bit two's-complement multiply wrap,
integer truncation and a masked shift. Its implementation reproduces
**65,536/65,536** independently verified input values over the entire
signed 16-bit input range. The output little-endian FNV64 signature is
`8DE7932315CFB4EA`. Public synthetic source fixture also asserts
Win95 hold register high byte `0x74` for a zero timecent input,
Win95 SF2 preset sustain output `0x0D` for synthetic generator 900,
and preserves SF1/SBK and previously unverified DOS behavior.

## Real Windows native VST3 acceptance after these changes

The precise compiled source branch head, BEFORE this docs-only commit,
is `a1e625dd9e935e39c4077dcb331c086e577d724e`.

**Full GitHub Actions Windows run #45086057058 / job #137404833163:
COMPLETED SUCCESS on 2026-11-23 UTC**.

- Steinberg Validator **47 PASS / 0 FAIL**.
- 125A Plugin Tester v0.2.10: **41 PASS / 0 WARN / 0 FAIL**.
- 125A Plugin Tester v0.3.3: **41 PASS / 0 WARN / 0 FAIL**.
- Full in-memory P213, P214 and P215 numeric and actual SF2
  generator fixtures passed, including 65,536 exhaustive PCOD cases.
- P210 authentic original C chip: 31, 32, 33, 37 overlapping SF2
  zones correctly limited to **32 physical** voices and detected an
  unsafe allocator alias mutation.
- P211 real Windows VST3 audio and project recall: **direct 37-zone
  bank** PCM identical to restored 37-zone PCM in two fresh instances.
- Original-chip multirate audio, heap-free real-time callbacks, MIDI
  controls, state, EditorHost GUI, and installer interruption tests
  all passed in the same exact Windows run.

The P213 and P214 standalone full Windows runs #45085226299 and
#45085519157 also completed successfully before this candidate.

## What remains empirically wrong — release NO-GO

Original SF2 Win95 304 writes span eight physical chip frames, whereas
the fully synchronous native worklist currently submits them within
one chip frame. The matching 304 values/order does **not** establish
bit-exact audio. Do not substitute a one-preset timestamp replay or
hard-code sample-frame timings; reconstruct the real driver work/cycle
scheduling and verify with independent guest captures.

Original physical chip IP has one unresolved 16-bit value difference.
The source-of-truth remains the independent guest reference, not the
synthetic fixture.

There is NO fully aligned Win95 Creative vs final Windows VST3
actual WAV/audio comparison for the same bank and MIDI passage, and
no final Fender Studio Pro ASIO owner-system measurement. Consequently
**no approved release installer** can be claimed from this report.
