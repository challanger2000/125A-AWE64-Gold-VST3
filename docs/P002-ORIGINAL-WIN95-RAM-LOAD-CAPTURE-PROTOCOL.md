# P002 — Original Creative Windows 95 RAM-SoundFont capture

**Status:** Original Creative Win95 guest loader compiled, FAT16 staging tested; actual original guest capture and RAM-bank VST3 acoustic parity **NOT YET PERFORMED**. PR #4 remains **DRAFT**. No Studio One/ASIO manual test before final GUI.

## Source authenticity

Public primary Creative Technology Ltd 1994–96 source:
- Original WINAWE32.C: https://github.com/qb40/sound-blaster-awe32/blob/master/WINDOWS/SAMPLE/AWEDEMO/WINAWE32.C
- Original AWE_API.H: https://github.com/qb40/sound-blaster-awe32/blob/master/WINDOWS/INCLUDE/AWE_API.H
- Original INDICES.H: https://github.com/qb40/sound-blaster-awe32/blob/master/WINDOWS/INCLUDE/INDICES.H

Those Creative developer examples verify AWEMAN32.DLL / AWEManager, AWE_OPEN message 101, AWE_LOAD_USER_BANK message 204, AWE_CLEAR_USER_BANK 205, AWE_QUERY_DRAM_SIZE 308, original user bank 1, and original CBufferObject with OPER_FILE 0. This guest program sends calls to the ORIGINAL installed Creative driver rather than implementing speculative EMU8000 DRAM placement.

## Artifacts and where they run

- Source: src/win95/p002_original_creative_ram_bank_capture.c
- GitHub workflow: .github/workflows/p002-win95-ram-bank-capture.yml
- Generated Win95 32-bit Pentium P54C-only executable: P002RAM.EXE in short-retention Actions artifact P002-Win95-Original-Creative-RAM-Bank-Capture-Guest-EXE. The PE imports only KERNEL32.DLL and WINMM.DLL. AWEMAN32.DLL is loaded dynamically *inside the private original Win95 machine*. No Creative ROM/DLL/VXD/SF2 or Microsoft Windows image is embedded or uploaded.
- Private staging tool: tools/p002_stage_original_win95_ram_probe.py. Copies an EXISTING original owner-controlled 512-MiB Win95 FAT16 disk to a distinct, disposable private clone. Preflights MBR, FAT16 geometry, FAT mirroring, unoccupied FAT root entries and clusters; verifies full original source disk SHA256 and exact readback of both guest files. No original private disk overwritten.
- Synthetic-only FAT16 negative regression: tests/test_p002_win95_ram_stage.py. Uses a newly created synthetic sparse 512-MiB disk, synthetic PE marker and synthetic SF2 bytes, never original Windows/Creative material.
- Private original source trace matcher: tools/p002_original_ram_trace_probe.py. Requires a REAL original fork EMU8K_TRACE and the EXACT same private SF2. Finds all complete original RAM sample word matches; zero or multiple candidate addresses are rejected. Reports SHA256 of original trace/sample and a source-observed absolute EMU8000 RAM word address.
- Offline original-chip readback CLI: tools/p002_private_ram_capture_cli.cpp. Validates pinned original 1-MiB Creative ROM SHA256, exact extracted SF2 PCM and original captured hardware SMALW/SMLD words BEFORE physical chip replay; compares actual unchanged chip 28MiB RAM readback to SF2 source words. Still only a source research tool, NOT RAM-SF2 VST3 playback.

## Private existing 86Box/Win95 capture procedure

All quoted paths below are placeholders for the owner-controlled existing VM, not files asserted to exist in CI. Keep original Microsoft, Creative, personal SF2 and the private virtual disk OUT OF GIT.

1. Download the short-retention P002RAM.EXE from the completed original-Win95 GitHub Actions workflow.
2. Create a new disposable copy of the existing original Creative Win95 FAT16 disk and install exactly P002RAM.EXE and chosen bank as guest C:\P002RAM.SF2:

    python tools/p002_stage_original_win95_ram_probe.py --private-win95-disk "PRIVATE\original-win95.img" --private-clone "PRIVATE\p002-ram-guest-copy.img" --win95-p002ram-exe "PRIVATE\P002RAM.EXE" --private-sf2 "PRIVATE\selected-bank.sf2"

3. Boot the CLONE in the already configured calibrated turican0/86Box-AWE32 fork with the ORIGINAL installed Creative Windows 95 driver. Enable the fork's EMU8K_TRACE and AWE32_WAV/DRY recorder BEFORE starting the guest EXE. Guest program: C:\P002RAM.EXE; original driver log: C:\P002RAM.LOG.
4. Verify exactly one original AWE Synth WinMM device and AWEMAN32.DLL with AWEManager. The log must contain ORIGINAL_AWEMAN32_LOAD_USER_BANK_RC=0x00000000 plus RAM_LOAD_API_SENT_OK. Original driver AWE_QUERY_DRAM_SIZE reports free 16-bit DRAM words BEFORE/AFTER the actual load. A successful API call or memory delta alone is NOT actual hardware evidence: independently confirm the real EMU8000 SMALW/SMLD capture and original DRY PCM.
5. Analyze original driver RAM upload trace together with THE SAME private SF2:

    python tools/p002_original_ram_trace_probe.py --original-emu8k-trace "PRIVATE\original-win95-ram.trace" --private-sf2 "PRIVATE\selected-bank.sf2" --sample-id 0 --private-report "PRIVATE\source-ram-evidence.json" --private-filtered-ram-replay "PRIVATE\source-filtered-ports.trace"

6. Independently authenticate the original SBAWE.VXD, AWEMAN32.DLL, calibrated 86Box fork, AWE32 ROM SHA, selected SF2 and full captured guest event clocks. The private PC source matcher requires exactly ONE complete full-sample source PCM match; an unknown or duplicate DRAM address is NO GO. Run the private Windows original-chip capture CLI with actual original RAM address and port trace only after these independent prerequisites are met.
7. Compare full Win95 original DRY PCM with pinned VST3/EMU8000 DRY PCM under exact same ROM, bank, MIDI, register, CC64, envelope, FX 0 and original sample rate conditions. Align timing, never amplitude-normalize to hide errors, fix root causes not individual presets.

## Explicit non-claims

The Win95 binary compilation, 512-MiB FAT16 synthetic clone test, positive original-chip RAM readback using synthetic trace, or any manually asserted provenance flag do NOT establish historical Creative driver RAM placement or original guest PCM parity. No genuine original Win95 RAM/SF2 upload was executed in GitHub Actions. The P002 export still rejects real RAM-SF2 playback until guest-register and PCM equivalence is proven. No new DSP, guessed ROM-bank address, memory allocator or audio-thread I/O has been introduced.
