# P007BJ — owner private Win95 original Creative SF2 hot-swap PCM acceptance (2026-11-19)

## Scope

Owner's file: `P007BJ-ERGEBNIS.txt` (41 lines). The owner ran the private Windows VST3 + genuine 86Box/Win95/Creative AWEManager test with the genuine selected `Vintage Dreams Bank(1).sf2`; the private Microsoft/Creative/ROM/SF2 binaries and user-local paths are **not** committed here.

This document transcribes only observed test markers and numeric diagnostics. It does **not** assert sonic parity with physical AWE64 Gold hardware.

## Observed pass markers (in log order)

- `P007AV_REAL_WINDOWS_VST3_SF2_PATH_V3_RECALL_AND_CORRUPTION_REJECTION_PASS`
- `P007BI_ROM_ONLY_COLD_BOOT_STATE_PREPARED`
- `P007BI_LIVE_SF2_SWAP_REQUESTED guest_ready=1 original_cold_boots=1 selected_bytes=315982`
- `P007BI_ORIGINAL_WIN95_CREATIVE_LIVE_SF2_NO_REBOOT_LOAD_ACK_PASS`
- `P007AV_REAL_WIN95_CREATIVE_SF2_LOAD_ACK_PASS sf2_bytes=315982 original_cpu=1 winmm_ready=1`
- `P007AV_ORIGINAL_CREATIVE_USER_BANK1_NOTEON_SENT`
- `P007AV_ORIGINAL_ROM_BANK0_RESELECTED`
- `P007AV_REAL_ORIGINAL_CREATIVE_SF2_BANK1_TO_EMU8000_PCM_PASS`
- `P007AV_REAL_WIN95_ORIGINAL_ROM_BANK0_RETURN_PCM_PASS`
- `P007BI_OWNER_PRIVATE_ORIGINAL_CREATIVE_HOT_SF2_TO_EMU8000_PCM_AND_ROM_PASS`
- `P007W_ACTUAL_WINDOWS_VST3_HOST_QA_PASS`
- `P007BJ_REAL_WIN95_IN_PLACE_VINTAGE_DREAMS_SF2_PCM_OWNER_PASS`

The test's C++ acceptance gates observe original 86Box cold-boot count before and after the SF2 load, genuine AWEManager ACK, separate original-chip register traces, acoustically nonzero and distinguishable PCM for SF2 user bank 1 and ROM bank 0, and no false positive on synthetic-core fixtures. Thus this particular private original-guest **single hot-swap** is owner-validated.

## Actual measurements

| Quantity | SF2 bank 1 | Original ROM bank 0 |
|---|---:|---:|
| Samples in measurement | 108544 frames | 108544 frames |
| RMS | 0.037359951 | 0.002936271 |
| Peak | 0.113128662 | 0.021423340 |
| Roughness metric | 0.038456524 | 0.259549006 |
| Original EMU8000 register events | 152 | 76 |
| Register hash | `8356214be0680c1e` | `38e73cd316bcfdee` |

Baseline RMS: `0.000000000`. SoundFont file: `315982` bytes. Probe used source bank 0, program 7, MIDI note 60, routed through original Creative user bank 1.

## Remaining mandatory release gates

**NOT covered** by this particular private test: two consecutive **different** SF2 files without reboot; entire GUI bank listing and interactive preset selection (including all 128 melodic and eight source SF2 drum kits); authentic audible MIDI channel-10 drum kits; state restoration after active-DAW load and re-open; multi-note/voice and timing stress; CPU/ROM hardware parity; final Studio One practice; independent adversarial FULL AUDIT under `125A-Engineering/START-HERE.md` and `QA/AUDIT-COMPLETENESS-CONTRACT.md`.

The 86Box logfile still contains one `Illegal instruction 00008B55 (FF)` diagnostic before the guest reached READY. It did not block this PASS. This should be classified separately by the safety audit, not silently marked resolved.

## Release decision

**P007BJ technical original-guest no-reboot SF2 + PCM milestone: PASS. Full product release: NO-GO until remaining gates pass.**

The original private user result file must remain private; this evidence summary deliberately omits OS usernames, host paths and proprietary assets.
