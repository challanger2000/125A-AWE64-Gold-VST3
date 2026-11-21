# P187 — P186 independent NO-GO: coherent controller echo, RPN legacy, NoteID and SF2 identity

**Independent audit base:** PR #46 at `7a63dd37110b2c31547b7c3a4a30eb4b9a435df1`, second independent audit verdict **NO-GO**. Product code was unchanged by that auditor.

## Independent findings carried forward (not softened by green CI)

- The original A01–A05 and N01–N07 *unchanged* scripts passed the Codex P186 audit. N08 still reproduced: **111 mixed**, **954,654 stale** among **980,068 accepted** `getState` saves during *sounding, repeated controller-echo* automation. Previously green P185 and P186 tests missed this exact timeline.
- Old N09/N10 scripts expected old permissive behavior and aborted when the new explicit SHA and installer-lock rejection triggered. New separate tests found those rejection mechanisms functional, **not** a complete private-installation or disk-race proof.
- Further Codex NO-GO cases: a legacy project acquired a false effective pitch-bend range, a delayed explicit NoteID NoteOff released another active note, same-valued host edits could disappear mid-block, and active project recall could render an exchanged SoundFont while retaining the previous file's SHA. Old code provided a bit-exact/pinned `getState` snapshot but mixed it with separately acknowledged GUI overlays.
- The auditor's full `/workspace/scratch/125a-pr46-p186-audit/AUDIT-P186.md` and raw `...-nachweise.zip` were referenced in the owner message but not provided as GitHub repository files. **This remediation cannot claim byte-for-byte reuse of those original scripts.**

## Implementation principles

### One coherent publishing boundary for controller echo

The host controller and audio callback had **separate** sources of truth. Earlier P186 consumed pending scalar edits during the callback and before publishing the new audio snapshot, creating windows where `getState` read A from the previous pinned snapshot and independently B/cleared controller fields. This explains N08 even while the publisher's internal snapshot slot was fully coherent.

P187 replaces value-only pending acknowledgments with a 64-bit **generation+payload** atomic edit token for program/bank/master/GUI/FX/SEND. The audio callback captures the token at block entry. Any host edit issued later, **even if numerically identical**, receives a new version. Queue events mark the relevant slots as touched but do not clear the GUI edits. Controller-only edits are applied to the hardware path only if they are still the identical captured command. The audio callback first publishes the *entire* new immutable snapshot and **only after publication succeeds** clears the exact captured generations. Unpublished blocks never claim an edit as applied. Real MIDI point timing remains sample-offset based; no audio-thread mutex, heap, file I/O, unbounded retry or blocking wait is introduced.

Added a hostile 900 × **512-frame audible** original-chip test: before every callback *both* B host controller edits are accepted and saved byte-exact, the callback receives their identical audio queue, an independent thread attempts concurrent saves throughout, and post-callback saves must still be B. The observer uses stable phase boundaries to reject the false A/B ambiguity of a concurrent GUI transition. A separate original-chip nonzero-energy assertion prevents a zero-frame-only green oracle. This catches the independent N08 timeline that P185/P186 omitted.

### Legacy RPN safety without increasing V4/V5 stream size

Raw old Data Entry bytes alone **do not prove** the final RPN bend range. P187 tracks which channel had genuine RPN0/0 data entry and only replays a range when ownership is established. The existing 2 reserved bytes of the 580B V4/V5 prefix encode the per-channel effective bend ownership mask only when the most recent RPN selector no longer implies ownership, preserving standard old V4/V5 byte-for-byte where unambiguous. An old project with an unsupported NRPN/NULL Data Entry and reserved=0 uses its original default pitch range, instead of silently treating that value as RPN pitch-bend range.

The true legacy intent of an old project with no effective-range evidence is unknown; no automatic historical guess can reconstruct information that the old format never saved.

### Strict NoteID, asset identity and loader timing

- A **specified** NoteID NoteOff can only release the same still-active note ID; a fallback by same source/pitch is reserved for negative/unspecified NoteIDs. Test a late duplicate release followed by an independently held second voice: original-chip PCM must be identical to a no-late-release reference.
- A **pinned Windows file handle** opened with `GENERIC_READ` and `FILE_SHARE_READ` now spans the whole non-RT SHA → SoundFont parse → 28MiB original-chip bank upload → final SHA transaction. No other writer/rename can replace the file while it is being accepted. Explicit V5 project restore, fresh activation and GUI SF2 loading use this mechanism and reject a mismatch without publishing a wrong bank/SHA pair. Never lock a file on the audio thread.
- Existing positive legacy tests and the P186 exact source PCM oracles remain required, including A01–A05 / N01–N07 where accessible.

## Evidence boundary / release gate

P187 source is NOT a release and remains **NO-GO until exact-commit Win64 build, P187 targeted independent source-semantics tests, original independent N08/P186 adversarial re-audit, original Creative licensed media + hardware and actual Fender Studio Pro/ASIO acceptance**.

No user testing or private package is requested or implied. All public GitHub CI fixture ROM/SF2 assets are synthetic, NOT licensed original Creative materials.
