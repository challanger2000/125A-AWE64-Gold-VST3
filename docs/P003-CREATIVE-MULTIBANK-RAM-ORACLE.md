# P003 — original Creative RAM allocator differential (not yet original parity)

## Implemented

- Standalone fixed-capacity 64-bank, bounded, word-addressed EMU8000 allocator `CreativeCandidateRamAllocator.h`; it needs no actual Creative driver to reserve RAM.
- The first-fit policy and first physical address `0x200010` are **candidates**, not verified general Windows 95 allocation rules. The known one-bank original capture begins at `0x200010`, but that does not establish placement for a second bank or fragmentation behavior.
- Allocation with rollback on invalid SF2, no overlaps, stale-handle generation protection, and lease pins prohibiting release while a chip voice still references the bank.
- `p003CompareCandidateAllocationToOriginalCapture` verifies the exact independently SHA-bound SF2 and complete original RAM trace before comparing physical base and word count. Any mismatch frees the provisional lease, leaves the actual chip untouched and is a **NO-GO** for source parity.
- The original Win95 guest `P002RAM.EXE` now optionally loads a *second* private SF2 into Creative user-bank slot 2 after the baseline slot 1 musical probe. It queries the actual original driver DRAM counts before/after, emits trace markers, frees slot 2 and re-queries DRAM. It uses only the original AWEMAN32 `AWE_LOAD_USER_BANK` (204), `AWE_CLEAR_USER_BANK` (205) and `AWE_QUERY_DRAM_SIZE` (308). No guessed VXD routine is run in the guest.
- The original private FAT16 stager now accepts a second SF2 and independently SHA256-verifies its exact disk readback, without changing the owner Win95 disk. If absent, the legacy one-bank test is unchanged.

## Exact private two-bank experiment

Use the existing owner-only original Creative Windows 95 / 86Box environment, not CI. Prepare **two different** supported, trusted SF2 files, ideally a short second bank with a different pool length; do not put either bank or the VM image on GitHub:

```powershell
python tools/p002_stage_original_win95_ram_probe.py `
  --private-win95-disk "PRIVATE\original-win95.img" `
  --private-clone "PRIVATE\p003-two-bank-guest-copy.img" `
  --win95-p002ram-exe "PRIVATE\P002RAM.EXE" `
  --private-sf2 "PRIVATE\bank-one.sf2" `
  --private-second-sf2 "PRIVATE\bank-two.sf2"
```

Boot the *clone*, run `C:\P002RAM.EXE` with the original installed Creative driver and **record the complete EMU8K_TRACE** for both uploads. Keep both SF2s, the guest log, original chip trace, ROM hash, original VXD provenance and the final physical RAM snapshots linked by SHA256. Expected private guest events include `P003_ORIGINAL_SECOND_BANK_EXPERIMENT_BEGIN`, `P003_ORIGINAL_SECOND_BANK_PORT_CAPTURE_BEGIN/END`, `P003_ORIGINAL_SECOND_BANK_LOAD_RC=0`, `P003_ORIGINAL_SECOND_BANK_CLEAR_RC=0`, and `P003_ORIGINAL_SECOND_BANK_EXPERIMENT_FINISHED`. These labels indicate API success and boundaries only; **physical addresses require the separate EMU8K_TRACE**.

After the guest shuts down, audit the guest log against **both exact SF2 files**; missing, reordered, duplicate or inconsistent multi-bank observations must fail:

```powershell
python tools/p002_verify_original_win95_ram_guest_log.py `
  --private-guest-log "PRIVATE\\P002RAM.LOG" `
  --private-sf2 "PRIVATE\\bank-one.sf2" `
  --private-second-sf2 "PRIVATE\\bank-two.sf2" `
  --private-report "PRIVATE\\p003-original-two-bank-api.json"
```

The audit cryptographically binds the host-side two SF2 inputs to its report and independently checks original guest whole-file FNV values, Creative API 204/205 results, 308 DRAM word deltas and release results. API success alone does **NOT** prove the Creative physical address allocator; the separately captured EMU8K_TRACE is mandatory.

Compare predicted addresses with *each real capture*. Repeat with swapped load order, release/reload, sizes near boundaries and known source-preserved voice activity. **Do not mark original allocator equivalence until original-vs-native SMALW/SMLD address, word counts, ordering, chip RAM and matched DRY PCM all pass.** The custom first-fit allocator is never silently substituted into the strict original-native playback mode.

## Current limitations

- Only mono, unlinked, 16-bit SF2 banks pass this candidate whole-pool gate. Original Creative bank allocator behavior for multichannel SF2, mixed ROM/RAM, loop packing, memory fragmentation, 3-tap conversion and free-list compaction is **NOT PROVEN**.
- The local owner-only original Win95 two-bank capture has **not been executed by the public GitHub CI**; synthetic CI validates the guest build, FAT16 staging and allocator safety, not original source parity.
- Do not alter the pinned EMU8000 core or release P003 based on the independent allocator tests.
