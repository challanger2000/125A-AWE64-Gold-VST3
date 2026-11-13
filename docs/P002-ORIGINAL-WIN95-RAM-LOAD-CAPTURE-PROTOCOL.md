# P002 — Original Creative Windows 95 RAM-SoundFont capture

**Status:** Original Creative Win95 guest loader compiled, FAT16 staging tested; actual original guest capture and RAM-bank VST3 acoustic parity **NOT YET PERFORMED**. PR #4 remains **DRAFT**. No Studio One/ASIO manual test before final GUI.

## Source authenticity

Public primary Creative Technology Ltd 1994–96 source:
- Original WINAWE32.C: https://github.com/qb40/sound-blaster-awe32/blob/master/WINDOWS/SAMPLE/AWEDEMO/WINAWE32.C
- Original AWE_API.H: https://github.com/qb40/sound-blaster-awe32/blob/master/WINDOWS/INCLUDE/AWE_API.H
- Original INDICES.H: https://github.com/qb40/sound-blaster-awe32/blob/master/WINDOWS/INCLUDE/INDICES.H

Those Creative developer examples verify AWEMAN32.DLL / AWEManager, AWE_OPEN message 101, AWE_LOAD_USER_BANK message 204, AWE_CLEAR_USER_BANK 205, AWE_QUERY_DRAM_SIZE 308, original user bank 1, and original CBufferObject with OPER_FILE 0. The updated guest also invokes the original documented **AWE_SEND_MIDI=401** as a second independent Creative driver MIDI pathway: WinMM plays C4, then direct AWEManager sends D4. Both phases send dry CC91=0 and CC93=0 and are individually bracketed in the guest log. Their successful API results do NOT alone prove correct original RAM-bank selection or DRY PCM. This guest program sends calls to the ORIGINAL installed Creative driver rather than implementing speculative EMU8000 DRAM placement.

## Artifacts and where they run

- Source: src/win95/p002_original_creative_ram_bank_capture.c
- GitHub workflow: .github/workflows/p002-win95-ram-bank-capture.yml
- Generated Win95 32-bit Pentium P54C-only executable: P002RAM.EXE in short-retention Actions artifact P002-Win95-Original-Creative-RAM-Bank-Capture-Guest-EXE. The PE imports only KERNEL32.DLL and WINMM.DLL. AWEMAN32.DLL is loaded dynamically *inside the private original Win95 machine*. No Creative ROM/DLL/VXD/SF2 or Microsoft Windows image is embedded or uploaded.
- Private staging tool: tools/p002_stage_original_win95_ram_probe.py. Copies an EXISTING original owner-controlled 512-MiB Win95 FAT16 disk to a distinct, disposable private clone. Preflights MBR, FAT16 geometry, FAT mirroring, unoccupied FAT root entries and clusters; verifies full original source disk SHA256 and exact readback of both guest files. No original private disk overwritten.
- Synthetic-only FAT16 negative regression: tests/test_p002_win95_ram_stage.py. Uses a newly created synthetic sparse 512-MiB disk, synthetic PE marker and synthetic SF2 bytes, never original Windows/Creative material.
- Automatic *read-only* Win95 guest-log retrieval: `tools/p002_extract_original_win95_ram_log_fat16.py` directly reads `C:\\P002RAM.LOG`, `C:\\P002RAM.SF2` and `C:\\P002RAM.EXE` from the already **powered-off** private FAT16 VM clone. It checks both FATs, the complete cluster chain (including cycles/duplicates), guest SoundFont byte-for-byte identity and original Creative driver API results. This removes all manual guest logfile copying; the VM image is only opened **read-only**. The 512-MiB disk SHA is streamed, not read into RAM.
- One-command private source join: `tools/p002_join_original_win95_ram_evidence.py` combines the recovered guest RAM-bank API log, exact staged SF2 bytes, a separately captured actual original 86Box `EMU8K_TRACE`, optional original 44.1-kHz stereo PCM16 DRY WAV, and source-matched **unique** RAM word address into ONE privately stored SHA256-linked audit. Optional `--private-filtered-ram-ports` writes only literal captured original SMALW/SMLD source port events. It refuses mismatched, altered, absent or duplicated source sample mappings. Neither log nor WAV proves source clock alignment, VXD provenance, or VST3 audio parity by itself.
- Win95 log auditor: tools/p002_verify_original_win95_ram_guest_log.py. Reads the original private guest C:\\P002RAM.LOG and the SAME host-side SF2, verifies exact **whole-file FNV-1a32** generated inside the original Win95 guest (in addition to source size), successful AWEManager/WinMM results, **DRAM word counts before/after** the load, 0% MIDI CC91/CC93 sends, and properly bracketed original bank loading. Its JSON report SHA256-locks the owner-side bank and the original guest log; FNV is ONLY an accidental-change correlation check, not cryptographic Creative driver provenance.
- Private original source trace matcher: tools/p002_original_ram_trace_probe.py. Requires a REAL original fork EMU8K_TRACE and the EXACT same private SF2. Finds all complete original RAM sample word matches; zero or multiple candidate addresses are rejected. Reports SHA256 of original trace/sample and a source-observed absolute EMU8000 RAM word address.
- Offline original-chip readback CLI: tools/p002_private_ram_capture_cli.cpp. Validates pinned original 1-MiB Creative ROM SHA256, exact extracted SF2 PCM and original captured hardware SMALW/SMLD words BEFORE physical chip replay; compares actual unchanged chip 28MiB RAM readback to SF2 source words. Still only a source research tool, NOT RAM-SF2 VST3 playback.

## Private existing 86Box/Win95 capture procedure

All quoted paths below are placeholders for the owner-controlled existing VM, not files asserted to exist in CI. Keep original Microsoft, Creative, personal SF2 and the private virtual disk OUT OF GIT.

1. Download the short-retention P002RAM.EXE from the completed original-Win95 GitHub Actions workflow.
2. Create a new disposable copy of the existing original Creative Win95 FAT16 disk and install exactly P002RAM.EXE and chosen bank as guest C:\P002RAM.SF2:

    python tools/p002_stage_original_win95_ram_probe.py --private-win95-disk "PRIVATE\original-win95.img" --private-clone "PRIVATE\p002-ram-guest-copy.img" --win95-p002ram-exe "PRIVATE\P002RAM.EXE" --private-sf2 "PRIVATE\selected-bank.sf2"

3. Boot the CLONE in the already configured calibrated turican0/86Box-AWE32 fork with the ORIGINAL installed Creative Windows 95 driver. Enable the fork's EMU8K_TRACE and AWE32_WAV/DRY recorder BEFORE starting the guest EXE. Guest program: C:\P002RAM.EXE; original driver log: C:\P002RAM.LOG.
4. Verify exactly one original AWE Synth WinMM device and AWEMAN32.DLL with AWEManager. The log must contain ORIGINAL_AWEMAN32_LOAD_USER_BANK_RC=0x00000000 plus RAM_LOAD_API_SENT_OK. Original driver AWE_QUERY_DRAM_SIZE reports free 16-bit DRAM words BEFORE/AFTER the actual load. A successful API call or memory delta alone is NOT actual hardware evidence: independently confirm the real EMU8000 SMALW/SMLD capture and original DRY PCM.
5. First validate the original Win95 guest API log against the EXACT SF2 file which was staged into the private disk clone:

    python tools/p002_verify_original_win95_ram_guest_log.py --private-guest-log "PRIVATE\\P002RAM.LOG" --private-sf2 "PRIVATE\\selected-bank.sf2" --private-report "PRIVATE\\source-win95-ram-api-evidence.json"

   This refuses a different same-sized SoundFont or a failed original AWEMAN32 API call. A positive original guest log is NOT itself proof of original RAM port writes or PCM.
6. Analyze original driver RAM upload trace together with THE SAME private SF2:

    python tools/p002_original_ram_trace_probe.py --original-emu8k-trace "PRIVATE\original-win95-ram.trace" --private-sf2 "PRIVATE\selected-bank.sf2" --sample-id 0 --private-report "PRIVATE\source-ram-evidence.json" --private-filtered-ram-replay "PRIVATE\source-filtered-ports.trace"

7. Independently authenticate the original SBAWE.VXD, AWEMAN32.DLL, calibrated 86Box fork, AWE32 ROM SHA, selected SF2 and full captured guest event clocks. The private PC source matcher requires exactly ONE complete full-sample source PCM match; an unknown or duplicate DRAM address is NO GO. Run the private Windows original-chip capture CLI with actual original RAM address and port trace only after these independent prerequisites are met.
8. Compare full Win95 original DRY PCM with pinned VST3/EMU8000 DRY PCM under exact same ROM, bank, MIDI, register, CC64, envelope, FX 0 and original sample rate conditions. Align timing, never amplitude-normalize to hide errors, fix root causes not individual presets.

## One offline command after the private Win95 VM has shut down

```powershell
python tools/p002_join_original_win95_ram_evidence.py `
  --private-booted-win95-clone "PRIVATE\p002-ram-guest-copy.img" `
  --private-sf2 "PRIVATE\selected-bank.sf2" `
  --original-emu8k-trace "PRIVATE\original-win95-ram.trace" `
  --sample-id 0 `
  --original-dry-wav "PRIVATE\original-guest-dry.wav" `
  --private-log-output "PRIVATE\p002-extracted-guest.log" `
  --private-report "PRIVATE\p002-unified-ram-evidence.json" `
  --private-filtered-ram-ports "PRIVATE\p002-literal-upload.trace"
```

These are illustrative **private owner-side paths**, not evidence that
a running original Windows 95 image, `SBAWE.VXD` or 86Box recording is
available to GitHub Actions. The VM must be shut down before reading the
disk image; a guest that has not actually executed `C:\P002RAM.EXE`
does not have the required `P002RAM.LOG` and fails closed. The combined
report documents `NOT_PROVEN` until original Creative driver origin,
cross-clock correlation and actual original-vs-VST3 DRY PCM identity are
independently demonstrated.

## 2026-11-13 Codex independent RAM-sample scanner regression fix

The original research control [PR #4 comment 6855958922](https://github.com/challanger2000/125A-AWE64-Gold-VST3/pull/4#issuecomment-6855958922) identified **three distinct scanner bugs**. The developer branch now keeps *historical* sample-waveform hits (`historical_pcm16_upload_matches`) separate from `final_state_unoverwritten_pcm16_matches`: an authentic final RAM-source candidate requires exactly ONE historical aligned full sample and no physical sample word rewritten at any time in the captured trace, **even when the rewritten value is identical**. This agrees with the existing unchanged strict C++ original-port replay overlap gate; unrelated RAM addresses may be overwritten without fabricating source sample evidence.

Any incomplete `SMALW` pointer address at trace EOF, or on premature register-selector change, is a **NO GO** for both scanner and existing original-port exporter. Historical waveform search checks the next byte following a rejected odd-offset match, preventing a valid sample starting at byte offset 2 from being skipped. A second even-offset waveform occurrence remains an **ambiguous source placement NO GO**. End-to-end guest/SF2/EMU8K trace join additionally rejects stale historical sample matches and dangling pointer transactions. Regression tests are explicitly synthetic-only; no original VM/Creative driver, EMU8000 time-aligned PCM or final released RAM-SF2 playback has been verified by these tests.

## 2026-11-13 original RAM capture and guest MIDI-bank parity hardening

The **native C++ preflight** and Python full original trace scanner/port exporter now
reject an interrupted SMALW low/high address when the selector is changed, a
SMLD selector without a completed pointer, and an unexpected write port
while SMALW/SMLD is active. This preserves the strict no-partial-chip-write
condition for native candidate source replay. Positive and adversarial
regression tests use only synthetic traces and SF2 bytes.

The original Creative guest `P002RAM.EXE` now sends the **same explicit
MIDI Bank Select MSB 1 probe (CC0=1)** via both WinMM and the original
`AWEManager(..., AWE_SEND_MIDI=401, packedMIDI, 0)` path, before program 0
and dry CC91/93 NoteOn sequences. Previously only WinMM selected Bank 1;
the two source routes therefore could not be meaningfully compared for
RAM-bank selection. The log auditor requires both source calls to succeed
and enforces bank-select → program → dry controllers → NoteOn → NoteOff
order inside each independent phase. Missing, failed or reordered calls
are NO GO. No original driver internals or undocumented MIDI mapping
are invented.

**Important:** CC0=1 is a controlled experiment, not proof that Creative
maps the user bank loaded by API 204 to that MIDI address. Actual sample
readback, a genuine original Win95/86Box driver register trace and source
DRY PCM are still mandatory before accepting bank selection or VST3
SoundFont parity. The recompiled Win95 Pentium guest and synthetic
tests must not be confused with running it in the private Win95 VM.

## Explicit non-claims

The Win95 binary compilation, 512-MiB FAT16 synthetic clone test, positive original-chip RAM readback using synthetic trace, or any manually asserted provenance flag do NOT establish historical Creative driver RAM placement or original guest PCM parity. No genuine original Win95 RAM/SF2 upload was executed in GitHub Actions. The P002 export still rejects real RAM-SF2 playback until guest-register and PCM equivalence is proven. No new DSP, guessed ROM-bank address, memory allocator or audio-thread I/O has been introduced.
