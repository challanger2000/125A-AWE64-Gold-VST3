# P007AI — Why four original Creative Filter-ENV knobs remain inconclusive: missing envelope-to-filter modulation depth

**18 November 2026 | Research-only | Actual original ROM/Win95/Creative note output NOT yet measured in this P007AI branch.**

## Original owner results (source: P007AH user-supplied private report)

- In the user's real original 86Box+Win95 Creative EMU8000 Pad 8 (Sweep) program 95, all twelve notes were recorded at 44.1 kHz; WinMM READY, stable A1/A2 and correctly restored baseline.
- **AMP Decay** became acoustically measurable: amplitude sustain RMS increased from **0.000197065** (original) to **0.000321259** (+63%), returned to **0.000197041**. The user now has **8/12** genuine P165 audio-proven controls (prior 7/12 plus AMP Decay).
- Filter Envelope **Attack** 1104, **Decay** 1105, **Sustain** 1106, **Release** 1107 remained **INCONCLUSIVE**, not defective. All four A/B/A originals remained stable. Probe verdict: `measured=1 inconclusive=4 unstable=0 total=5`, final true original Win95 test `P007W_ACTUAL_WINDOWS_VST3_HOST_QA_PASS`.

## Critical upstream original Creative source

Original *Sound Blaster AWE32 Developer's Information Pack* (Creative Technology 1994–1996), Part IV MIDI NRPN Implementation, **page 93**:
https://www.phatcode.net/res/244/files/adip301.pdf

- **NRPN LSB 24** = **Envelope 1 to Filter Cutoff** modulation depth, signed range **[-127,+127]**, 56.25 cents/step, ±6 octaves at envelope peak.
- **NRPN LSB 5/7/8/9** = Envelope 1 Attack/Decay/Sustain/Release. These only change a filter contour where modulation depth actually routes that envelope to the filter; therefore a patch with depth near zero can sound insensitive despite legitimate original-processor NRPN commands.
- Creative signed 14-bit NRPN: status `B0`, CC99=127, CC98=24, CC6 and CC38 holding `8192 + signedDepth`. For **-96**, data entry `63,32`; for **+96**, `64,96`.
- Original **CC121** resets the SoundFont generator overrides to original patch defaults. It will also reset any temporary NRPN24 modulation amount, so the depth MUST be reapplied after each A/B/A "return to zero" before comparing the next reference. Crucially, this is diagnosis ONLY, not new default behavior in the shipping plugin.

## Implementation: P007AI signed depth investigation

- New branch `research/p007ai-creative-env-filter-depth-20261118`, based on P007AH; **production VST3, old approved P165 photographic GUI, original EMU8000 and Win95 guest remain unchanged**.
- `P007OriginalEnvFilterDepth.hpp`: exact signed MIDI mapping for original Creative NRPN24 and a four-packet bounded retry sender.
- `P007OriginalEnvFilterDepthTest.cpp`: mapping test for full negative and positive range, malformed protocol, positive/negative ±96 source values, failed packet retry.
- `P007ActualHostQA.exe <PRIVATE original VST3> --private-env-depth`: an owner-private original Win95 test. One original 86Box guest boot, **14 real MIDI notes**:
  1. Two original Pad 8 (Sweep) references.
  2. Depth **-96** and **+96** via the ORIGINAL Creative WinMM driver through the existing COM2 16550 UART; each of the four 6-byte control packets sent **at least 40 ms apart** to avoid recreating P007AD's FIFO overrun.
  3. Choose the most useful original acoustic depth direction using measured PCM, never by synthetic amplitude estimates. An inaudible, missed NoteOn does not qualify.
  4. Re-record a new baseline with the selected depth, then each of the four remaining physical P165 Filter Env knobs modified and restored, **reapplying the same signed depth after the original VST3's CC121 neutral return**.
  5. Reset the original guest to its SoundFont default via genuine CC121 and verify final baseline restoration.
- Each note uses longer Pad 8 onset/sustain/tail windows; report per-knob original PCM RMS/roughness and verdict `MEASURABLE` / `INCONCLUSIVE_WITH_THIS_ROM_PATCH` / `A_B_A_REFERENCE_UNSTABLE`. Only if 4/4 are genuinely measurable and correctly restored may it emit `P007AI_FOUR_ORIGINAL_CREATIVE_FILTER_ENVELOPES_PCM_PROVEN`; `P007AI_PRIVATE_ORIGINAL_ENV_DEPTH_DIAGNOSTIC_COMPLETED` alone is NOT an acoustic PASS.
- GitHub Windows CI compiles and executes the signed protocol unit tests, original P165 EditorHost, Steinberg validator 47/47, original Creative short-MIDI pacing tests, project recall and negative assertions: **synthetic-only guest must NEVER pass original private NRPN24 diagnostic**.

## Release constraints

This experiment changes ONLY the private QA tester, not the product VST3. If genuine original measurements confirm the signed modulation depth as the missing dependency, next decide how to expose the authentic Creative NRPN24 amount without sacrificing the approved P165 original 12-knob layout, 0%=neutral, original 86Box/EMU8000 behavior, proper project recall, or 0% FX send=dry. Do not silently force NRPN24 into every SoundFont preset.

**Do not merge, ship, or mark 12/12 controls as working before the owner's authentic Win95 original PCM evidence.** Reverb/Chorus type variation via original SysEx, guest SF2/SBK loading and final Studio Pro/ASIO tests remain separate.
