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

## Strict original Win95 versus VST3 DRY PCM16 source comparator

The next authentic P002 evidence gate is an **external, independently captured** original Win95/86Box driver DRY waveform compared to an identically configured real native VST3 PCM window. No such verified private original RAM-SF2 capture is present in public CI.

To allow that comparison reproducibly without inventing the driver, `tools/p002_compare_original_dry_pcm.py` now compares two **44.1 kHz stereo signed PCM16 WAV files at the pre-card AWE32_WAV / EMU8000 digital tap**, with mandatory explicit original and native start-frame indices and window duration. On the VST3 the private `AWE64_P002_PRE_CARD_PCM_DIAGNOSTIC=1` must be selected before activation. **Do not** compare its normal post-CT1745 float output to pre-card PCM16 and claim hardware parity.

Example command (all PRIVATE paths and frame numbers are placeholders, NOT measured timings):

```powershell
python tools/p002_compare_original_dry_pcm.py --original-precard-wav "PRIVATE\original-awe32-wav.wav" --vst3-precard-wav "PRIVATE\native-precard-pcm16.wav" --original-start-frame 441000 --vst3-start-frame 0 --frames 44100 --private-report "PRIVATE\p002-dry-pcm-comparison.json"
```

The explicit window start positions must be justified by original MIDI/register timestamps, **not automatically guessed** from a matching waveform. The comparator reports first differing frame, count of differing channel samples and stereo frames, exact signed 16-bit LSB peak/error, RMS and SHA256 of the compared windows. It does **not** normalize amplitude, shift sample timing, resample, swap channels or accept silence as musical parity. Exit 0 = exact window equality; exit 1 = measured waveform mismatch; exit 2 = invalid/unusable evidence. Both equal and unequal results explicitly mark independent original Creative driver/ROM/SF2 provenance, register/FX consistency and release approval **NOT PROVEN**.

Automated synthetic-only negative controls cover single-bit PCM differences, a one-frame shift, gain differences, channel swaps, silence, wrong format, insufficient frames, and output-report overwrite attempts. These tests validate the *comparator*, not the Windows 95 driver playback.

The existing **single-command Win95 guest + exact SoundFont + source RAM trace evidence join** (`tools/p002_join_original_win95_ram_evidence.py`) now also supports this comparator, without requiring a separate manual analysis. Supply the existing `--original-dry-wav` together with **all four** additional options:

```text
--vst3-precard-wav PRIVATE/native-precard-pcm16.wav
--original-start-frame 441000
--vst3-start-frame 0
--frames 44100
```

All offsets above are placeholders, not timing observations. Missing or corrupt input fails closed; an explicit PCM mismatch creates a private report identifying the unequal samples and exits nonzero. Even exact PCM window equality leaves the original driver, bank/clock alignment and release gates marked **NOT PROVEN**, until the private original capture is independently authenticated. The P002 evidence join also still refuses ambiguous or later-overwritten source-SF2 RAM addresses.
## 2026-11-13 Original Creative Win95 ABI and synth-mode instrumentation

Original Creative 1994–96 `WINDOWS/INCLUDE/INDICES.H` uses
`#pragma pack(2)` under Win32: the original `CBufferObject` is
**14 bytes**, with field offsets `m_Size=0`, `m_Buffer=4`,
`m_SizeUsed=8`, `m_Flag=12`. Our previous **16-byte default-packed
guest-test struct was our developer error**. It is now corrected in
`src/win95/p002_original_creative_ram_bank_capture.c` with a
compile-time layout lock. The guest log records the 14-byte size, and
`tools/p002_verify_original_win95_ram_guest_log.py` rejects old/wrong
capture records. The physical byte offsets were already the same; do
not claim this change alone corrected the sound or verified driver RAM
upload. The pinned EMU8000 kernel is untouched.

The same original Win95 guest now calls the documented
`AWE_GET_SYN_BANK=305` exactly as demonstrated by Creative
`WINAWE32.C`: a correctly packed `CBufferObject` carries the
synth-bank MODE as `m_Flag`. The guest logs this **read-only** query
both before and after loading the SF2. The auditor demands successful
return codes, authentic event order and an enum in
`0=GENERAL_MIDI, 1=ROLAND_GS, 2=MT_32, 3=USER_CUSTOM_BANK`.
It records whether the mode changed, without requesting a mode change.
This resolves an observational blind spot in our own test harness,
but **does not** prove MIDI CC0=1 selects the uploaded user bank or that
the original chip actually rendered the RAM samples. That requires
original Creative Win95 driver/86Box register and digital PCM evidence.

Original source reference: [Creative `INDICES.H`](https://github.com/qb40/sound-blaster-awe32/blob/master/WINDOWS/INCLUDE/INDICES.H),
[`WINAWE32.C`](https://github.com/qb40/sound-blaster-awe32/blob/master/WINDOWS/SAMPLE/AWEDEMO/WINAWE32.C), and
[`AWE_API.H`](https://github.com/qb40/sound-blaster-awe32/blob/master/WINDOWS/INCLUDE/AWE_API.H).

## 2026-11-13 original Win95 MIDI submission-time evidence

`P002RAM.EXE` also emits **four original guest OS ticks**:
`ORIGINAL_WINMM_C4_ON_SUBMIT_TICK_MS`,
`ORIGINAL_WINMM_C4_OFF_SUBMIT_TICK_MS`,
`ORIGINAL_DIRECT_D4_ON_SUBMIT_TICK_MS` and
`ORIGINAL_DIRECT_D4_OFF_SUBMIT_TICK_MS`. Each value is captured with
Win95 `GetTickCount()-t0` immediately **before** the matching
`midiOutShortMsg` or Creative `AWE_SEND_MIDI` call, after the
completed bank load and explicit 0% CC91/CC93 sends.

The original-guest log auditor requires the exact source phase and
submission order, and conservative elapsed-time checks for the two
400-ms NoteOn/NoteOff holds and the 500+150-ms gap between the
WinMM C4 and original AWEManager D4 source paths. Missing, reordered,
zero-duration, pre-bank-load or too-short note timing is NO GO.
Read-back results remain separate: `midiOutShortMsg` or
`AWE_SEND_MIDI` success only means an API call was accepted, **not**
that the MIDI event reached the EMU8000 at that exact tick.

These times are **not EMU8K_TRACE sample frames or an audio clock**.
The private source analysis reports
`guest_GetTickCount_to_86Box_EMU8000_frame_time =
NOT_PROVEN_INDEPENDENT_CLOCK_DOMAINS`. Only the independent original
guest register trace and recorded DRY PCM can establish actual
original chip event frames; never convert guest ticks to 44.1-kHz PCM
positions by assuming a fixed conversion or offset.

## 2026-11-13 private original Creative driver file provenance, read only

`tools/p002_attest_private_win95_creative_driver_fat16.py` can read the
owner's **already existing** original 512-MiB FAT16 Win95 disk WITHOUT
booting or modifying it. It retrieves exact 8.3 file bytes from
`C:\WINDOWS\AWEMAN32.DLL`, `C:\WINDOWS\SYSTEM\AWEMAN32.DLL`,
`C:\WINDOWS\SYSTEM\SBAWE.VXD`, and `C:\AWE32\SBAWE.VXD`.
It rejects missing/ambiguous FAT directory entries, malformed cluster
chains, discordant FAT1/FAT2 copies, differing DLL shadow copies, differing
installed/vendor VXD copies and hashes that do not match **separately
authenticated private SHA256 reference values**. Both Creative references
are supplied by the owner, never inferred from the guest itself.

Optional `--private-synth512-sf2` checks the **entire byte content**
of `C:\SF2\SYNTH512.SF2` against the owner's separately provided source bank.
The resulting report contains SHA256 digests and NO driver/SF2/Windows
file content. Store the report **outside Git** and never upload a
private Windows-95 disk, Creative binary or personal SoundFont to public CI.

Example (ALL source paths and SHA256 values are placeholders; use actual
independent source hashes, never copy an unverified value from the VM):

```powershell
python tools/p002_attest_private_win95_creative_driver_fat16.py --private-win95-disk "PRIVATE\original-win95.img" --expected-original-sbawe-vxd-sha256 "64_HEX_DIGITS_FROM_EXTERNAL_ORIGINAL" --expected-original-aweman32-dll-sha256 "64_HEX_DIGITS_FROM_EXTERNAL_ORIGINAL" --private-synth512-sf2 "PRIVATE\Synth 512 Bank.sf2" --private-report "PRIVATE\win95-creative-file-attestation.json"
```

All five new offline regression tests run exclusively against generated
sparse synthetic FAT16 media (no Creative, Microsoft or user bytes).
This file attestation proves only **matching installed bytes** and, when
given the bank, **matching staged SF2 bytes**. It does NOT prove that
the installed driver was executed, that Creative loaded RAM SF2 into the
chip, or that a MIDI NoteOn played its sample. Those remain the separate
original-Win95 runtime register/DRY-PCM source evidence gates.
## 2026-11-13 prepared private Windows-95 original-driver VM with verified calibrated 86Box 0.93

The developer used the owner's **already-existing private** retro music-PC archive and the separately uploaded authentic `Synth 512 Bank.sf2` to prepare a **new disposable copy** of the existing 512-MiB FAT16 Win95 disk. The staged bank is **byte-identical** to the one already installed in the old guest at `C:\\SF2\\SYNTH512.SF2` (555,754 bytes; SHA256 `d5b99ec38fddf59f53f66b90921e170d207bf3bbd73960f6e4175c39ef68349b`).

`tools/p002_arm_private_win95_autorun.py` (committed in [`91cc8f4`](https://github.com/challanger2000/125A-AWE64-Gold-VST3/commit/91cc8f41afaf3bbf92967326c27afb2f84a41180)) uses the existing strict FAT16 clone/staging tool, then updates **only that clone's** `C:\\WINDOWS\\WIN.INI` `[windows] run=` to `C:\\P002RAM.EXE`. It fails closed for occupied run, unknown directories, absent/multicluster Win.ini, stale P002RAM.LOG, mismatched exact source EXE/SF2, FAT damage and in-place source disk. Five synthetic-only 512-MiB FAT16 regression tests and the original PE32 guest compilation succeeded in [workflow #43470476772](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43470476772). The **actual owner disk** was then separately staged offline and re-read to confirm the new WIN.INI, exact EXE/SF2, both unchanged FAT copies and source image SHA256 without any VM guest execution.

**Pinned, genuine calibrated emulator:** Official public `turican0/86Box-AWE32` release `v6.0-AWE32-0.93`, Windows-x64 release asset SHA256 `cefd28b691bfe1149719d26f54729356af91f9c3f713cfa6bab747aa0fa16b09`, independently downloaded, hash verified and successfully unzipped in [workflow #43471087038](https://github.com/challanger2000/125A-AWE64-Gold-VST3/actions/runs/43471087038). Its extracted calibrated `86Box.exe` SHA256 is `67ec5e58643e32a6dc4efd6e1b42ce2ce7b7da048d218355a54ff39cdc327f16`. This replaces the different, unverified `86Box.exe` from the older private retro package. It is not a DSP modification or new homemade emulator.

**User-facing private artifact created locally, not published to GitHub:** a standalone complete Windows x64 test ZIP based on the owner's own VM (private Microsoft/Creative/ROM/SF2 assets retained only in the owner's private conversation artifact). The ZIP contains `VM\\win95.img` (staged clone), `VM\\86box.cfg` (unchanged machine parameters including EMU8000 28,672 KiB RAM), pinned verified original fork `86Box\\86Box.exe`, existing privately held BIOS/Creative ROMs, and a `START-P002.bat` that sets the **original fork's** documented `EMU8K_TRACE` and `AWE32_WAV` environment variables BEFORE starting the VM. Outputs are distinct timestamped files under `P002-Nachweise`; original guest API log writes `C:\\P002RAM.LOG`. The full ZIP passed CRC and file-size/hash verification. It is private; do NOT upload owner Win95 or bank bytes to public GitHub.

**Mandatory limitation:** The developer's Linux container does not have a working 86Box graphical/runtime environment to boot this Win95 image. A real guest boot, actual `AWE_LOAD_USER_BANK` execution with source AWEMAN32.DLL/SBAWE.VXD, EMU8K_TRACE RAM port writes, WAV, guest MIDI-bank mapping and original-vs-native PCM have **NOT** been observed. The fork WAV starts at emulator boot; it may contain a lengthy silence while Win95 starts. Do not report that silence as an audio defect or infer chip frame positions from Win95 GetTickCount. The target Windows 11 user's exact 86Box boot and ASIO/Studio One at end are still open. **P002 remains DRAFT**.

## Explicit non-claims

The Win95 binary compilation, 512-MiB FAT16 synthetic clone test, positive original-chip RAM readback using synthetic trace, or any manually asserted provenance flag do NOT establish historical Creative driver RAM placement or original guest PCM parity. No genuine original Win95 RAM/SF2 upload was executed in GitHub Actions. The P002 export still rejects real RAM-SF2 playback until guest-register and PCM equivalence is proven. No new DSP, guessed ROM-bank address, memory allocator or audio-thread I/O has been introduced.
