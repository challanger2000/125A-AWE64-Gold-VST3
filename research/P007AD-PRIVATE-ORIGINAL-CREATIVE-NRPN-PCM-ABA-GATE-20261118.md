# P007AD — original Creative Win95 NRPN audio differential gate

2026-11-18 | Research-only | NOT a release certification

## Product constraints
- An installed product is ONE AWE64_Gold_P007.vst3 containing the original unmodified 86Box/EMU8000, original Win95 guest/Creative driver in private bundle, authentic approved P165 hardware GUI.
- No external host, emulation EXE or second synthesizer. Diagnostic P007ActualHostQA.exe is **QA-only**, outside the installed VST3.
- No proprietary Win95 HDD, Creative ROM or private sample content in GitHub Actions, public source, or job artifacts.
- The earlier P007Y private test proved original Win95 Creative NoteOn generated genuine nonzero EMU8000 PCM. It did **not** prove the new twelve P165 generator controls audibly change the original driver's output.

## Original manufacturer controls (Creative AWE32 FAQ, section F/H)
Primary-source page: https://ftp.braindrainlan.nu/Drivers/Sound%20Cards/Awe64gold%20skivan/CREATIVE.WWW/DOCS/V0000162.HTM

CC99 = 127; CC98 = generator number; CC6 = (8192 + actual value) >> 7; CC38 = (8192 + actual value) & 127.
P165 VST3 IDs 1100..1111 map to amp ENV2 11/13/14/15, filter ENV1 5/7/8/9, cutoff/resonance 21/22, reverb/chorus SEND 26/25.

CRITICAL SEMANTIC CORRECTION:
- A raw NRPN cutoff of **zero** is approximately 100 Hz and would overwrite the source patch. Hence user-knob **0% = SoundFont neutral**, not NRPN value 0, for first TEN tone generator offsets.
- Returning a modified tone generator to 0% invokes original Creative **CC121 Reset All Controllers**, restoring its original SoundFont settings; remaining nonzero tone offsets and both FX sends replay via bounded UART producer.
- Reverb/Chorus 0% is a **real zero SEND**: NRPN 26/25 actual value 0 on initial guest READY and after patch changes/CC121. This proves exact byte delivery in synthetic QA only, not that authentic Win95 driver applies the send as expected.
- Full four-CC transaction retries after FIFO rejection. One parameter transaction maximum per audio block, no disk/86Box CPU work on audio thread.
- Project version v2 44-byte state stores twelve controls, includes integrity-check; legacy v1 16-byte state still loads. The approved full P165 photo and 73% darker hardware islands remain unchanged.

## Executed public Windows CI scope
- Official Steinberg validator, actual compiled VST3 save/reopen, full native Windows GUI/editor and 12-knob mouse automation tests, synthetic-only guest MIDI route/44.1-48-96k PCM tests.
- Adversarial synthetic MIDI mock rejects first CC98 for filter NRPN 21. Actual plugin retries full CC99/98/6/38, receives cutoff value 76 at 60% plus AMP11. Returning filter to zero yields Creative CC121 and **no destructive NRPN cutoff raw=0**, and reverb/chorus sends zero are each transmitted.
- Local pure reference A/B/A PCM evaluator rejects fake effects from silent/dropout B notes, stale background without NoteOn, unchanged A/B tone, unstable original A/A.
- **None** of these synthetic/CPU tests can establish the licensed original Win95 driver applies NRPN audibly.

## Independent genuine original-drive one-boot test (not run in public CI)
The `P007ActualHostQA.exe <private-vst3> --private-nrpn` option:
1. Verifies original-core CPU exports and actual licensed private Win95/Creative image, refuses the synthetic fixture.
2. Boots original Win95 Creative inside the VST3 using disposable TEMP profile; owner DAW guest state unaffected.
3. Requires an authentic WinMM READY acknowledgment and a quiet post-READY baseline before any NoteOn.
4. Plays six identically timed ROM/GM Program40 Note60 notes on the same original 44.1kHz EMU8000 path. Each original note uses NoteOn/NoteOff and independent onset/sustain/tail digital PCM windows:
   - A1: untouched SoundFont, both FX sends dry
   - A2: exact original repeat (negative control / determinism)
   - B: filter cutoff knob 10% on original Win95 Creative NRPN21
   - A3: filter knob 0%; original Creative CC121 reset to patch defaults
   - B: AMP attack knob 65% via original Win95 Creative NRPN11
   - A4: AMP knob 0%; original Creative CC121 reset to patch defaults
5. Measures RMS and normalized derivative "roughness" to detect filter lowpass even when overall volume stays similar, onset attenuation for attack, exact expected A/B/A recovery; refuses B missing/failed NoteOn.
6. Prints per-scenario real PCM measurements and `P007AD_PRIVATE_ORIGINAL_PCM_DIFFERENTIAL` summary. Prints `P007AD_PRIVATE_REAL_WIN95_CREATIVE_NRPN_ABA_PCM_PASS` **only if** stable reference, genuine original NoteOn attribution, effective cutoff, effective attack and original return to patch all pass.

**NOT YET PROVEN:** Actual running private `--private-nrpn` result; reverb/chorus send differential; full 12-control acoustic matrix; original Guest SF2/SBK load; live variation SysEx; Studio Pro ASIO validation. Do not confuse original executable built under Windows CI with owner-private original audio test actually run.

## Planned next acceptance
Run the consolidated test on the owner's original private Win95 once. If the original driver ignores some NRPN or audio difference fails, instrument its register writes and diagnose driver control path before enabling final release. No fake 125A-owned DSP workaround.
