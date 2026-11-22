# P205: Source-derived original Creative Win95 allocator VTFT reset

The independently reconstructed original Win95 `OriginalCreativeMidiEngine.cpp`
in branch `codex/p005-vxd-full-port` contains `physical_reset`: after
the allocator's physical DCYSUSV=0x00FF stop, it writes
`VTFT=0x0000FFFF` via original EMU8000 DATA0 (E22 0x60|voice, 0x620).
This precedes *any* note-main registers. For a stereo pair, both
allocator stops precede both VTFT resets; independent mono allocations
each reset their own voice. These are distinct from the two
`StartVoice` VTFT resets in each subsequent original main body.

P205 adds this exact prologue only to P204's bounded Win95/SF2
worklist. Original 86Box EMU8000 C core, ROM/SBK, DOS, VST3
interface, GUI and saved-state format are untouched.

Acceptance: authentic original 86Box C chip logs in three valid
public synthetic SF2 setups (3 mono, mono+linked stereo, linked stereo
+mono). Require exactly three one-per-physical VTFT=FFFF writes
strictly after each allocator DCYSUSV stop and before first note-main
stop. Require nonzero generated original-chip PCM and existing P204
voice order. Separate test-only macro suppresses just these
allocation resets, and is rejected by the specific P205 causal
oracle. This does not assert real owner Win95 guest timing/PCM parity,
13-SF2 bank coverage, ASIO CPU acceptance or full release approval.

## Native Windows x64 acceptance at exact unchanged product commit

**Code / CMake / QA SHA:** `fccc87d634fcead6a59a0acc356a132dce032e4b`.
**Full real Windows workflow:** [Actions #45062480280](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/45062480280), **COMPLETED SUCCESS**, job `137331259413`, 44 successful steps.

- Original 86Box C chip + real hardware port I/O: **3/3** independent SF2 layouts pass; actual 44.1k PCM nonzero sample counts **16,195, 16,201, 16,201**. All three *allocator* VTFT=0x0000FFFF writes occur before the first note-main DCYSUSV stop, on the exact separately reserved physical voice, and after its allocator DCYSUSV stop.
- **Deliberate negative source mutation killed at the precise reason:** `P204_ORIGINAL_CHIP_RESERVATION_NO_GO P205 original allocator VTFT reset missing before first main stop`. Negative macro is **not** compiled into actual product VST3.
- P204 separate three-voice allocation chronology and legacy-mutant negative both retain **PASS**.
- Unmodified original-chip P202 **8 PCM/44.1k and 48k reference cases x2**, dense 44.1k CPU p95 improvement versus legacy duplicate synthesis and falsifying PCM mutation **PASS**.
- Steinberg Validator native x64 Windows VST3 **47/47 PASS**.
- Exact published 125A Plugin Tester v0.2.10 **41 PASS / 0 WARNING / 0 FAIL** and independently tested v0.3.3 **41/0/0**.
- P187 sounding host state echo **939,687 accepted / 0 stale / 0 mixed / 0 rejected**; real positive original-chip PCM energy **1280.720460930**.
- GUI/EditorHost two-view tests and interrupted Windows installer **BACKUP** and **TARGET** integrity checks **PASS**.
- Owner ROM `awe32.raw` + Creative `SYNTHGM.SBK` and **13 user SF2 files** were independently located and CRC/SHA-checked **outside** GitHub CI and **never placed in GitHub artifacts**. Merely finding original assets is NOT a Windows VST3 owner preset test. The CI bundle contains synthetic SF2 test fixtures, not the private ROM.

**Overall product status: RELEASE NO-GO.** This is a documented correction, not a completed AWE64 Gold instrument. Original guest Win95/Creative live register+PCM comparison for the user's 13 actual banks, reliable DAW project behavior under Studio Pro/ASIO and workload p95/p99, and the independent full Codex audit have **not** passed. Do not ship the synthetic CI bundle or overwrite the installed private version.
