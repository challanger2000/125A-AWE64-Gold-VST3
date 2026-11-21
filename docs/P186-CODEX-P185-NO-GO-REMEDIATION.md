# P186 — Response to second independent Codex NO-GO on P185

**Audit base:** PR #46 at `9f8f373e686951f3638526a999a61dceed553f3f`. The owner supplied the second independent Codex verdict **NO-GO**, reporting that the original A01–A05 counterexamples now pass, along with the CH10 bank-parameter return and P184 HWND anti-false-positive check. Ten additional failures were reported. The audit script/raw ZIP remains in a separate Codex scratch directory, not copied into this repository. Do not claim access to those exact test bytes.

**Release status: NO-GO until native exact-commit Win64 targeted regressions, independent re-audit, licensed private-media and owner DAW verification.**

## Issues, concrete changes, and adversarial QA

| Area | Reported new failure | P186 implementation / new test |
| --- | --- | --- |
| CH1 drum bank / CC0 | Real mapped MIDI CC0 could switch the logical bank without restoring the original physical CH10 program | `applyAdvancedController` reasserts affected hardware channel program when CC0 changes bank; `verifyP186CH10ProgramAndCC0DrumRouting` compares real original-chip synthetic Kit0/5 PCM on CH1 and CH10 |
| Independent CH10 Program Change | A separate CH10 program change could silently change what the CH1 drum browser sounds | Before a physical CH10 NoteOn, reapply its **logical source channel's** kit program (CH1 browser or native CH10), without touching the pinned EMU8000 DSP core; independent genuine VST3-host PCM regression |
| Overlapping NoteIDs | Notes of identical pitch but different VST3 NoteIDs could remain active after a drum bank switch | 512 preallocated fixed active-route records, physical channel+pitch reference count, exact NoteID preference and safe fallback for unspecified IDs. NoteOff follows NoteOn's original route, even after bank change. Test 2 distinct IDs, route change and real sustained-chip released-vs-held PCM |
| RPN NULL | RPN 127/127 must deselect selection **without resetting** the previously effective bend range | Preserve the last valid RPN0/0 data entry, apply it to the genuine Creative chip before restoring NULL selection, and apply wheel last. Test real mapped CCs and fresh V4 PCM |
| NRPN Data Entry | CC6 in unsupported NRPN mode must not overwrite previously effective RPN0/0 pitch range in the serialized V4 state | Retain effective range while forwarding unsupported CC to chip with RPN deselected. Test real CC mapping, subsequent reopen and range12-vs-default2 PCM |
| SEND ORIGINAL / Cutoff | Pending editor values could incorrectly override actual audio automation and create audible vs project-state divergence | An audio point supersedes the controller edit captured at block start; explicitly newer editor-only GUI/FX/SEND edits are also forwarded on the audio thread, with explicit SEND mode overriding SEND value. P185 automation stress is seeded with deliberately conflicting GUI and FX values; still requires independent external reproduction |
| GUI+MIDI+FX saves | Codex reported 72 mixed and 929,301 stale snapshots in 950,661 successful saves under sounding automation | Clear stale controller overlays after actual automation points; keep full bounded native snapshot publication. Target test has A/B genuine host-points, hostile initial controller values, concurrent live getState and counts of rejected/mixed saves. **Do not claim 0 stale until additional exact current audio truth oracle has run** |
| Explicit SF2 activation | After a successful original owner V5 SHA check, a different SF2 could be loaded at activation and its new SHA committed | Check pinned owner SHA before and after non-RT instrument construction, do **not** rewrite explicitly pinned owner SHA on activation, return failure instead of loading a different asset. Independent replace-valid-SF2-between-setState-and-setActive test |
| Parallel installers | A failed overlapping second installer could remove the successful first process's installed bundle | Exclusive persistent per-destination FileShare.None lock held over all stage/backup/target actions, with lock release in an outer finally. Second invocation fails **before observing or modifying target state**. P182 Windows QA now simulates a second installer while the target lock is held and verifies installed bytes, backups and no stage change |
| Test provenance | Synthetic installer test manifest recorded a fixed old source SHA instead of current CI revision | P182 test manifest now uses `GITHUB_SHA`; verify exact source SHA from the job, not an unrelated historical constant |

## Scope and limitations

- No original 86Box EMU8000 C/DSP modifications, no proprietary Creative ROM/SBK/SF2 assets in Git or GitHub Actions.
- The public Windows CI's generated 1 MiB test ROM and synthetic SoundFonts are **not** proof of a genuine Creative owner-ROM/77-preset private installation.
- A CH1 drum note and a native CH10 note with the same pitch share the original synthesizer channel. The bounded reference-counter approach prevents a hung NoteOff but intentionally delays the **first** same-pitch physical NoteOff until all matching held notes end; independent per-voice release precision is **NOT VERIFIED** and needs separate hardware-faithfulness review.
- A controller change sent between two actual audio callbacks is implemented at the next callback; original VST3 sample-offset queue events remain timed in their own block.
- Other untested controls and host MIDI paths remain NOT VERIFIED.
- Fender Studio Pro / owner's ASIO device and physical original-hardware PCM parity remain NOT VERIFIED.

## Evidence

Evidence must be filled with actual commit-matched Windows runs and their output markers. No test may be marked PASS solely by compilation or the 47/47 validator.
