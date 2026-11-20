# P007AH — five inconclusive P007AG original Creative controls: owner evidence and targeted Pad8 Sweep retest

2026-11-18 • Research only • Original P007AE audio/CPU/GUI unchanged

## Actual owner P007AG 22-note genuine original test (15:48 CET)
Input: `P007AG-10-REGLER-ORIGINALKLANG-ERGEBNIS.txt`, privately uploaded by the owner; raw Win95 disk and user's Windows paths are NOT committed.

- Owner's true Win95/Creative WinMM READY observed; original EMU8000 PCM, 22 notes; all independent A/B/A baseline restores stable; no corrupted note or synthetic fake.
- **5 newly proven:** AMP Sustain, AMP Release, Filter Resonance, Reverb Send, Chorus Send.
- **5 inconclusive with original GM Program 40:** AMP Decay; Filter Envelope Attack, Decay, Sustain, Release.
- **0 unstable A/B/A restorations.**
- Along with P007AE Filter Cutoff + AMP Attack, **7/12 genuine original Creative P165 controls have acoustic proof**.
- Exact owner summary: `P007AG_ORIGINAL_WIN95_CREATIVE_TEN_CONTROL_SUMMARY measured=5 inconclusive=5 unstable=0 total=10 original_guest=1 synthetic=0`. The trailing test exit is **FAIL due to strict “10/10” acceptance**; it is NOT a crash, corrupt state, broken original driver, or proof that five settings do nothing.
- Positive effect sizes: Filter Resonance onset RMS 0.004658190 → 0.002213329 (−52.5%), Reverb Send tail 0.000012351 → 0.000119484 (~9.7×), Chorus Send onset 0.004675372 → 0.009685801 (~2.1×), AMP Release tail ~27.6×.
- AMP Decay tail rose ~2.1× but onset ~2% and sustain was not above the prespecified threshold; we explicitly **do not** count unrelated tail variance as successful decay proof.
- Original Win95 boot `0F FF / INT 6` is the already-resolved deliberate P007AF CPU exception, not a problem.

## Reason for a different instrument

The Creative Sound Blaster AWE32 FAQ states the filter envelope is strongly patch-dependent and explicitly identifies **Pad 8 (Sweep)** as an instrument that exercises the filter-envelope path: https://ftp.braindrainlan.nu/Drivers/Sound%20Cards/Awe64gold%20skivan/CREATIVE.WWW/DOCS/V0000162.HTM
(NB: that document inconsistently calls it “instrument 86”; standard GM assigns **Pad 8 Sweep = instrument 96 1-based, PC 95 zero-based**, not 86: https://www.cs.cmu.edu/~music/cmp/archives/cmsip/readings/GMSpecs_Patches.htm ).
Program 40 is 0-based Violin and may not route its modulation envelope to the filter at all. Creative documents **NRPN 24** separately as “Envelope 1 to Filter Cutoff” depth, proving there is an independent modulation *depth* control: changing Envelope 1 ADSR alone does not necessarily produce audible filter changes if the depth is zero.

## Implemented source-only targeted experiment

- Branch `research/p007ah-pad-sweep-inconclusive-five-20261118`; all 125A plugin/Win95/ROM/EMU8000 bits left unchanged.
- QA-only new CLI mode `P007ActualHostQA.exe <private original AWE64_Gold_P007.vst3> --private-pad-sweep` using **authentic** Win95 original driver, no stand-in DSP.
- Select exact GM Program Change **95**, bank 0; capture 2 genuine original-reference notes and 5 per-knob B/A pairs (total **12 notes**). Five targeted remaining: AMP Decay (VST3 1101), Filter Env Attack 1104, Decay 1105, Sustain 1106, Release 1107.
- Longer NoteOn (~3.15 seconds) and NoteOff tail (~2.75 seconds), with altered onset/sustain/tail measurement windows for slow pads and envelope-release test.
- Report true PCM RMS/roughness/tail per note and individual `MEASURABLE` / `INCONCLUSIVE_WITH_THIS_ROM_PATCH` / `A_B_A_REFERENCE_UNSTABLE` outcomes. **A completed diagnostic is not misrepresented as 5/5 acoustic proof.** Original-source identity/WinMM READY and zero-guest-synthetic negative checks stay intact.
- The remaining envelope NRPNs are already present on the real VST3; **NO alteration to production synthesis or GUI is necessary for this diagnostic**.
- Windows CI must compile exact new test mode, run original P165 real editor/47 Steinberg tests, reject synthetic-only guest passed as original, and release a QA executable separately. The actual private Win95 ROM bank still requires owner's private test.

If Pad8 still fails to expose the four Filter Env controls, the next diagnostic is **test-only** creative NRPN 24 envelope→filter depth, paced like any other 16550 UART CC packet and verified independently; never claim SoundFont-neutral presets should be silently altered in shipping VST3.
