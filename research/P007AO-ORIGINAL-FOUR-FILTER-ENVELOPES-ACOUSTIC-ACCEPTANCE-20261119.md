# P007AO — original Creative/EMU8000 filter ADSR acoustic acceptance

**Date:** 2026-11-19. **Result: 4/4 PASS, original Windows 95 + original Creative driver + pinned 86Box-AWE32/EMU8000 core.**

This acceptance is based on the owner's private **actual** Windows run, not GitHub's synthetic analyser CI. The original Win95 ROM/HDD and detailed CSVs are *not* committed to this repository.

## Authentic evidence independently rechecked

Owner-supplied:
- `P007AO-ORIGINALKLANG-ERGEBNIS.txt` — 15/15 notes, original Win95 WinMM ready, original EMU8000 passive-register observer, VST3 host QA PASS, original filter proof PASS.
- `P007AL-ORIGINAL-EMU8000-REGISTER-EVENTS(3).csv` — 1,220 genuine EMU8000 port-write rows across 15 takes; no observer overflow reported.
- `P007AL-ORIGINAL-PCM-SPECTRAL-BANDS(2).csv` — 2,520 records (15 takes × 24 quarter-second bins × 7 frequency bands). These are genuine original-guest output PCM-derived data.

The filter-modulation depth registers read **PEFE 0x00C0**, and the independent amplitude-release registers **DCYSUSV 0x8001**, on all 10 relevant takes (4–13) with correct original chip voices **0,3,6,9,12,0,3,6,9,12**. Original driver resets and the pointer low 5-bit voice routing are honored.

## Distinct A/B/A design and spectral results

The measurement uses per-time-bin normalized band energy, the B result minus average of initial and restored A, and requires three or more consecutive 250ms bins of coherent change in a single band, conservatively checked against *that band’s* A/A-reference drift:

| Control | Take A / B / A-return | Max band change | Verified consecutive bins | Original spectrum verdict |
|---|---|---:|---:|---|
| Filter Attack | 4/5/6 | -4.7651 dB (band 6) | 6 | **PASS** |
| Filter Decay | 6/7/8 | -7.3742 dB (band 6) | 10 | **PASS** |
| Filter Sustain | 8/9/10 | -0.85 to -0.96 dB (stable band 3); peak -1.7882 dB in another band | 7 | **PASS** |
| Filter Release | 11/12/13 | -1.5385 dB (band 4); stable band 3 approximately -0.6 to -0.8 dB | 7 | **PASS** |

The four changes are *objectively measurable in genuine PCM*. These results do not purport to be an independent double-blind listening test or a bit-identical comparison to a physical AWE64.

### Critical invalid-reference bug prevented

- Sustain comparison: note-on modulation-envelope sustain `DCYSUS = 0x007F` on take 8 (A), `0x7F7F` on take 9 (B), **`0x007F` on take 10 (return A)**. Verified directly in raw original port CSV; fixes the false UNSTABLE_A_RETURN in P007AN.
- Release comparison has a **separate** A/B/A: takes 11/12/13 each hold filter mod-envelope sustain `0x7F7F` until NoteOff. The three note-off mod-release words are respectively **`0x807F → 0x8002 → 0x807F`**. All share `PEFE 0x00C0` and `DCYSUSV 0x8001`. This makes release rate audible over the extended amp tail; the old zero-mod-envelope release test was invalid.

## Why old 3/4 summary remains in the report

`P007AI_ORIGINAL_CREATIVE_FOUR_FILTER_ENV_SUMMARY measured=3 inconclusive=1` is an **older RMS/roughness-only heuristic**. It rates filter Release inconclusive because the total signal amplitude changes less than that heuristic's thresholds. It **does not override** the newer frequency-resolved original PCM A/B/A result `P007AL_FOUR_CORRECTED_FILTER_ENVELOPES_PCM_SPECTRAL measured=4 unresolved=0` and `P007AL_FOUR_ORIGINAL_FILTER_ENVELOPES_PCM_PROVEN`. The two are different measurements; only the corrected original spectral protocol is the acceptance criterion here.

## Boundaries and disposition

- **Closed:** four filter ADSR original PCM response paths, signed-depth polarity, private host note allocation, register decoding, independent A/B/A controls, no false proof without original guest.
- **Unchanged:** original Creative driver, pinned original 86Box-AWE32 EMU8000 DSP core, shipping product DSP, original GUI.
- **Not implied:** general release readiness of the full plugin, elimination of the earlier guest 'Illegal instruction' boot log, or global hardware bit-equivalence. Those must be assessed independently.
- **No additional filter-ADSR audio test required solely for this issue.**

Windows CI workflow #44425982261 also PASS; it is *supporting build/negative-control evidence*, separate from the private original PCM result.

**Verdict: P007AO FILTER ADSR ACOUSTIC ACCEPTANCE = PASS (4/4).**
