# P185 — Remediation of independent Codex NO-GO on PR #46

Status: **NOT RELEASE APPROVED**. Original audited integration HEAD: `5de21c8a6f15f35c5ca1fa35d7d599053f922b7a`.
Development: `fix/p185-codex-pr46-five-failures-20261121`. This report applies to the P185 source on that branch. Do not confuse a successful synthetic Windows host test with private Creative-ROM/owner-SF2 parity or Fender Studio Pro acceptance.

## Provenance and independent failure report

The owner supplied the independent Codex NO-GO diagnosis, based on real VST3 host API calls in a Linux diagnostic build, for the five defects below. The Codex audit report/repro archive were originally stored in Codex's `/workspace/scratch/125a-pr46-audit` directory, **not retrievable in this GitHub repository**. The five findings, including a reported 59,626/82,121 mixed-state save failure, are owner-supplied Codex evidence, not reconstructed local run numbers.

The old P184 two-window test also accepted the original HWND before the WM_CLOSE message had been dispatched; the window-count check alone did not prove reattachment.

## Requirement -> root cause -> production correction -> executable actual-host regression

| Gate | Previously missed contract | P185 correction | Actual Win64 VST3 regression |
| --- | --- | --- | --- |
| P185-D | CH1 browser Kit 5 must really play CH10 Kit 5 | Reassert CH1 redirected drum program on physical channel 9, **after** ordinary CH10 program restore | `verifyP185BrowserDrumKitActuallyChangesPCM`: valid synthetic SF2 Drum Kit 0 and 5 with independent PCM; all channels initially Kit 0, CH1 program changed ALONE via actual VST3 parameter flush; verify CH10 direct PCM and V4 fresh-instance recall |
| P185-R | RPN pitch-bend range12 must survive subsequent NRPN selection and reopening | Replay stored completed RPN 0/0 data before restoring final NRPN selection; replay pitch wheel **after** range | `verifyP185PitchRangeSurvivesNRPN`: original 86Box chip PCM equality range12 before/after NRPN, inequality from default-range negative |
| P185-S | Explicit ORIGINAL SEND is preserved even if stored knob >0 | New V5 0x0200 marker **only** for previously ambiguous explicit ORIGINAL+nonzero SEND; preserve existing V5 bitstream for other cases; decode previous V5 fallback for legacy | `verifyP185OriginalModeWithNonzeroSavedSend`: exact mode bits, fresh readback, old V5 backward compatibility |
| P185-B | Stopped ROM bank must not be overridden by older CC0 | Canonical restored Bank field authoritative; do not replay obsolete raw CC0/32, clear stale queued browser bank reassert | `verifyP185StoppedROMBankRestoresAgainstOldCC0`: deactivate, recall saved ROM and stale CC0=SF2, activate, compare original-chip PCM to separate ROM and SF2 baselines |
| P185-T | Saves during simultaneous MIDI/FX/GUI automation must contain one generation | Full immutable 4-slot read-pinned snapshot; nonblocking RT writer; getState reads pinned single generation without unbounded seqlock spinning; explicit host pending values serialized consistently | `verifyP185CoherentGUIAndMidiAutomationSaves`: 20,000 0-frame audio callbacks switching all six key parameters between A and B, concurrent host saves, **no mixed values and no rejected saves** |
| P184-H | Editor close/reopen must really detach the old view before counting a replacement | Require old HWND absence in the two visible genuine editor windows before accepting new view | `P184NativeEditorLifecycleQA.py` on **test-only** instrumented pinned Steinberg EditorHost; production VST3 unmodified |

No modifications were made to the upstream original 86Box EMU8000 DSP source. The changes are in the host adapter, controller, QA fixtures, and test-only EditorHost.

## Test quality defects in pre-P185 coverage

- The earlier drum regression accidentally used `Plugin.start(5,2)`, which set **all channels including the sounding CH10** to Kit 5. Thus it could pass on the original broken routing; P185's stricter test starts all channels at Kit 0 and automates **only CH1**.
- Previous P178 changed the complete SF2 asset/program tuple while testing host save; it did not alternate live MIDI+GUI+FX automation atomically. P179 kept a constant state. Both could pass despite the independent mixed-state defect.
- Previous RPN mapping checks largely verified serialized MIDI controller bytes and positive RPN tests separately, but not 12-semitone PCM after NRPN and fresh instance.
- Existing SEND mode tests covered manual/zero and type recall, but not explicit ORIGINAL with a nonzero saved SEND knob.
- Prior bank test covered host bank selection after CC0 during processing, but not the stopped-host reload path with a stale saved CC0 tuple.

## Executed evidence

### Initial P185 test run — FAIL / root cause exposed

[Windows run 44937654986](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44937654986), `fd9d1da6307c96d74f99ffa3b7dd3e0ec4fc7917`:

- Actual Windows build + Steinberg 47/47 PASS.
- First new automation stress case: 79,195 accepted saves, **zero mixed**, but **188,290 save rejections**. Explicit regression **FAIL**, not a false PASS.
- This motivated the immutable bounded read-pinned snapshot redesign instead of increasing the seqlock retry limit.

### Second P185 test run — PASS for its declared exact scope

[Windows run 44938184492](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/44938184492), `967c0f60a8d2e02d2e6983dd92a149501de4b944`:

- Actual Windows x64 native VST3 build and Steinberg validator **47/47 PASS**.
- New 20,000-callback A/B automation test, **262,719 successful saves; mixed=0; rejected=0** (public synthetic host run). A separate public host pass logged 183,553/0/0.
- P178 repeated 2,735,016 valid V5 state snapshots without torn/failed save; P179 500/500 paced audible saves PASS.
- All first-generation P185 synthetic original-chip MIDI/FX/bank/drum counterexamples PASS.
- Both Plugin Tester v0.2.10 and v0.3.3 **41 PASS, 0 WARNING, 0 FAIL**.
- P184 modified independent Win32 editor close/reopen & surviving peer test PASS.
- **IMPORTANT:** That run used the **weaker original drum fixture** and did not yet independently prove that CH1 alone controls Kit 5. A further build with the stronger drum oracle and deferred bend wheel replay is required. Treat the strengthened P185-D/R status as **NOT RUN** until its matching exact-SHA Windows CI passes.

## Additional P185 cross-channel fault found during self-challenge

**Interaction beyond the five Codex reports:** CH1 drum browser temporarily programs the physical CH10 lane with its selected Kit 5. When the user subsequently selects a melodic/ROM CH1 bank, CH10 must return to **its own independently stored kit**. The initial P185 correction changed CH10 to the correct browser kit, but left that kit on CH10 after switching CH1 away from drums. This could silently change an otherwise untouched MIDI channel.

**Root-cause remediation:** Track whether CH1 currently borrows CH10's program slot on the audio/lifecycle thread, and restore CH10's own bank/program only when the CH1 drum override is relinquished. Never reprogram all other MIDI channels on every CH1 edit. This is bounded, allocation-free, original Creative Program Change behavior and does not edit original chip DSP.

**Extra negative/positive regression:** In `verifyP185BrowserDrumKitActuallyChangesPCM`, all channels start with Kit 0, real VST3 CH1 parameter flush selects Kit 5, real VST3 CH1 bank automation exits drums, then *actual original-chip CH10 PCM* must equal a fresh independent CH10 Kit 0. This regression must fail if the browser kit erroneously persists after CH1 returns to melodic/ROM mode.

The new cross-channel interaction is **NOT RUN** until its exact-commit Windows CI completes.

## Remaining release gates — NOT VERIFIED

- Independent Codex audit of the repaired exact commit, including test oracle adversarial review.
- New exact-commit Win64 original licensed Creative ROM/SYNTHGM.SBK/private 77-preset SoundFont parity, original-hardware digital PCM comparisons, private installer with those assets.
- Fender Studio Pro/ASIO host acceptance, musical listening and user release authorization.
- GPL/source-distribution compatibility if publicly distributed.

**No product FINAL, no PR merge, no Studio Pro acceptance until the actual required gates have their own evidence.**
