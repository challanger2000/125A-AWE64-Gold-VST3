#!/usr/bin/env python3
"""P002 fail-closed audit of the ORIGINAL Windows 95 Creative API guest log.

The log is produced by PRIVATE guest C:\\P002RAM.EXE using the installed
1996 Creative AWEMAN32.DLL + original VXD. A syntactically valid log does
NOT independently prove original Creative DLL provenance, register
upload, correct MIDI bank selection, or any dry PCM waveform; these are
separate required evidence gates.

Always keep the actual private C:\\P002RAM.LOG and SoundFont OUT OF GIT.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

KEY_VALUE = re.compile(r"^([A-Z][A-Z0-9_]*)=(0x[0-9A-F]{8})$")
MAX_LOG = 128 * 1024
MAX_BANK = 32 * 1024 * 1024
MAX_RAM_WORDS = 0xE00000
MUST_ZERO = (
    "ORIGINAL_AWEMAN32_AWE_OPEN_RC",
    "ORIGINAL_AWEMAN32_LOAD_USER_BANK_RC",
    "ORIGINAL_AWEMAN32_AWE_CLOSE_RC",
    "ORIGINAL_AWEMAN32_CLEAR_USER_BANK_RC",
    "PRE_LOAD_RESULT",
    "POST_LOAD_RESULT",
    "ORIGINAL_CREATIVE_MIDIOUT_OPEN_RESULT",
    "ORIGINAL_RESET_CONTROLLERS_RC",
    "ORIGINAL_BANK_SELECT_MSB1_RC",
    "ORIGINAL_PROGRAM0_RC",
    "ORIGINAL_CC91_REVERB_SEND_ZERO_RC",
    "ORIGINAL_CC93_CHORUS_SEND_ZERO_RC",
    "ORIGINAL_MIDI_C4_VEL100_RC",
    "ORIGINAL_MIDI_C4_OFF_RC",
    "ORIGINAL_AWEMAN32_DIRECT_BANK_SELECT_MSB1_RC",
    "ORIGINAL_AWEMAN32_DIRECT_PROGRAM0_RC",
    "ORIGINAL_AWEMAN32_DIRECT_CC91_ZERO_RC",
    "ORIGINAL_AWEMAN32_DIRECT_CC93_ZERO_RC",
    "ORIGINAL_AWEMAN32_DIRECT_D4_VEL100_RC",
    "ORIGINAL_AWEMAN32_DIRECT_D4_OFF_RC",
)
CAPTURE_BEGIN = "ORIGINAL_WIN95_RAM_PORT_CAPTURE_MARKER_BEGIN"
CAPTURE_END = "ORIGINAL_WIN95_RAM_PORT_CAPTURE_MARKER_END"
SUCCESS = "P002_WIN95_ORIGINAL_CREATIVE_RAM_LOAD_API_SENT_OK"
NO_EVIDENCE = "GUEST_CAPTURE_EMU8000_RAM_REGISTER_AND_PCM_EVIDENCE_NOT_ASSERTED_BY_API"
DIRECT_BEGIN = "ORIGINAL_WIN95_AWEMAN32_DIRECT_MIDI_PHASE_BEGIN"
DIRECT_END = "ORIGINAL_WIN95_AWEMAN32_DIRECT_MIDI_PHASE_END"
REQUIRED = MUST_ZERO + (
    "PRIVATE_WIN95_SF2_SIZE",
    "PRIVATE_WIN95_SF2_FNV1A32",
    "WIN95_ORIGINAL_MIDI_ENDPOINTS",
    "CREATIVE_ORIGINAL_ENDPOINT_MATCH_COUNT",
    "PRE_LOAD_TOTAL_16BIT_WORDS",
    "PRE_LOAD_FREE_16BIT_WORDS",
    "POST_LOAD_TOTAL_16BIT_WORDS",
    "POST_LOAD_FREE_16BIT_WORDS",
    "CREATIVE_DRIVER_ALLOCATED_WORD_DELTA",
    "ORIGINAL_API_LOAD_BEGIN_TICK_MS",
    "ORIGINAL_API_LOAD_END_TICK_MS",
)


def fnv1a32(original: bytes) -> int:
    """Full original guest-readable bank bytes; source identity only,
    not collision-resistant SHA256 or original Creative provenance."""
    value=2166136261
    for byte in original:
        value=((value ^ byte)*16777619) & 0xffffffff
    return value


def audit(raw: bytes, source_sf2: bytes) -> dict:
    if not 32 < len(raw) <= MAX_LOG:
        raise ValueError("Missing or oversized private original Creative guest log")
    if (not 96 <= len(source_sf2) <= MAX_BANK or
            source_sf2[:4] != b"RIFF" or source_sf2[8:12] != b"sfbk" or
            int.from_bytes(source_sf2[4:8], "little") != len(source_sf2) - 8):
        raise ValueError("Missing, truncated or invalid private SF2 source bank")
    try:
        content = raw.decode("ascii", "strict")
    except UnicodeDecodeError as exc:
        raise ValueError("Non-ASCII original Creative guest log") from exc
    lines = content.splitlines()
    if len(lines) > 250:
        raise ValueError("Unbounded original Creative guest log events")
    if not lines or lines[0] != "P002_WIN95_ORIGINAL_CREATIVE_RAM_BANK_CAPTURE_START":
        raise ValueError("Missing original Creative Win95 capture start marker")
    if lines[-1] != NO_EVIDENCE or lines[-2] != SUCCESS:
        raise ValueError("Guest capture did not complete original API and no-evidence warnings")
    if content.count(CAPTURE_BEGIN) != 1 or content.count(CAPTURE_END) != 1:
        raise ValueError("Duplicate/missing original Creative bank-upload markers")
    if not (lines.index(CAPTURE_BEGIN) < lines.index(CAPTURE_END) <
            lines.index(SUCCESS)):
        raise ValueError("Original Creative bank-upload marker order invalid")
    if content.count(DIRECT_BEGIN) != 1 or content.count(DIRECT_END) != 1:
        raise ValueError("Original Creative direct AWE_SEND_MIDI capture markers absent")
    if not (lines.index(CAPTURE_END) < lines.index(DIRECT_BEGIN) <
            lines.index(DIRECT_END) < lines.index(SUCCESS)):
        raise ValueError("Original Creative WinMM vs AWE_SEND_MIDI phase ordering invalid")

    seen: dict[str, int] = {}
    dll_path: str | None = None
    for line in lines[1:]:
        if not line:
            raise ValueError("Unexpected empty line in original guest capture")
        if line.startswith("LOADED_CREATIVE_MODULE_PATH="):
            if dll_path is not None:
                raise ValueError("Duplicate AWEMAN32 loaded DLL path")
            dll_path = line.split("=", 1)[1].replace("/", "\\").upper()
            if not dll_path.endswith("\\AWEMAN32.DLL"):
                raise ValueError("Original Win95 loaded module is not AWEMAN32.DLL")
        if line.startswith("WIN95_MIDI_DEVICE="):
            # Device name is independent Windows 95 driver metadata,
            # not a resource ID or proof of original VXD authenticity.
            if not re.fullmatch(r"WIN95_MIDI_DEVICE=0x[0-9A-F]{8} NAME=.{1,80}", line):
                raise ValueError("Malformed original Creative WinMM device inventory")
        item = KEY_VALUE.fullmatch(line)
        if item:
            key, word = item.groups()
            if key in seen:
                raise ValueError("Duplicated original Creative guest result: " + key)
            seen[key] = int(word, 16)
        elif line not in (CAPTURE_BEGIN, CAPTURE_END, DIRECT_BEGIN, DIRECT_END,
                          SUCCESS, NO_EVIDENCE,
                          "GUEST_DRAM_ALLOCATION_API_ALONE_NOT_HARDWARE_UPLOAD_PROOF") and not (
                          line.startswith("LOADED_CREATIVE_MODULE_PATH=") or
                          line.startswith("WIN95_MIDI_DEVICE=")):
            raise ValueError("Unrecognized original Creative guest log line: " + line[:80])
    if dll_path is None:
        raise ValueError("Original Creative AWEMAN32 loaded DLL path absent")
    for key in REQUIRED:
        if key not in seen:
            raise ValueError("Missing original Creative result: " + key)
    for key in MUST_ZERO:
        if seen[key] != 0:
            raise ValueError("Original Creative/WinMM failure: " + key)
    if seen["CREATIVE_ORIGINAL_ENDPOINT_MATCH_COUNT"] != 1 or not (
            1 <= seen["WIN95_ORIGINAL_MIDI_ENDPOINTS"] <= 64):
        raise ValueError("Ambiguous/non-original AWE MIDI device selection")
    if seen["PRIVATE_WIN95_SF2_SIZE"] != len(source_sf2):
        raise ValueError("Private guest SoundFont bytes differ from audited original bank")
    if seen["PRIVATE_WIN95_SF2_FNV1A32"] != fnv1a32(source_sf2):
        raise ValueError("Private guest source SF2 whole-file identity differs")
    if not (0 < seen["PRE_LOAD_TOTAL_16BIT_WORDS"] <= MAX_RAM_WORDS and
            seen["POST_LOAD_TOTAL_16BIT_WORDS"] ==
            seen["PRE_LOAD_TOTAL_16BIT_WORDS"]):
        raise ValueError("Inconsistent/unavailable original driver RAM installed size")
    before = seen["PRE_LOAD_FREE_16BIT_WORDS"]
    after = seen["POST_LOAD_FREE_16BIT_WORDS"]
    if not 0 <= after < before <= seen["PRE_LOAD_TOTAL_16BIT_WORDS"]:
        raise ValueError("Original driver reported no positive RAM SoundFont allocation")
    delta = before - after
    if delta != seen["CREATIVE_DRIVER_ALLOCATED_WORD_DELTA"]:
        raise ValueError("Original Creative reported RAM word delta is inconsistent")
    begin_ms = seen["ORIGINAL_API_LOAD_BEGIN_TICK_MS"]
    end_ms = seen["ORIGINAL_API_LOAD_END_TICK_MS"]
    if end_ms < begin_ms:
        raise ValueError("Original guest RAM bank load time order invalid")
    if not (lines.index(CAPTURE_BEGIN) <
            next(i for i, x in enumerate(lines) if x.startswith(
                "ORIGINAL_AWEMAN32_LOAD_USER_BANK_RC=")) <
            lines.index(CAPTURE_END)):
        raise ValueError("Original Win95 load result escaped bank-upload capture markers")
    # Both original-driver MIDI routes must select the SAME controlled
    # user-bank candidate before program and notes. An accepted API call
    # does NOT establish that Bank 1 was actually rendered from RAM;
    # that still requires independent original registers + dry PCM.
    winmm_order = (
        "ORIGINAL_RESET_CONTROLLERS_RC",
        "ORIGINAL_BANK_SELECT_MSB1_RC",
        "ORIGINAL_PROGRAM0_RC",
        "ORIGINAL_CC91_REVERB_SEND_ZERO_RC",
        "ORIGINAL_CC93_CHORUS_SEND_ZERO_RC",
        "ORIGINAL_MIDI_C4_VEL100_RC",
        "ORIGINAL_MIDI_C4_OFF_RC",
    )
    direct_order = (
        "ORIGINAL_AWEMAN32_DIRECT_BANK_SELECT_MSB1_RC",
        "ORIGINAL_AWEMAN32_DIRECT_PROGRAM0_RC",
        "ORIGINAL_AWEMAN32_DIRECT_CC91_ZERO_RC",
        "ORIGINAL_AWEMAN32_DIRECT_CC93_ZERO_RC",
        "ORIGINAL_AWEMAN32_DIRECT_D4_VEL100_RC",
        "ORIGINAL_AWEMAN32_DIRECT_D4_OFF_RC",
    )
    for labels, start, end in (
            (winmm_order, CAPTURE_END, DIRECT_BEGIN),
            (direct_order, DIRECT_BEGIN, DIRECT_END)):
        offsets = [lines.index(label + "=" + "0x00000000") for label in labels]
        if not (lines.index(start) < offsets[0] and
                offsets[-1] < lines.index(end) and
                all(left < right for left, right
                    in zip(offsets, offsets[1:]))):
            raise ValueError(
                "Original Creative WinMM/direct bank-select and MIDI "
                "source phase/order invalid")
    if "GUEST_DRAM_ALLOCATION_API_ALONE_NOT_HARDWARE_UPLOAD_PROOF" not in lines:
        raise ValueError("Guest no-hardware-proof caveat missing")
    return {
        "status": "ORIGINAL_CREATIVE_WIN95_API_GUEST_LOG_CONTRACT_PASS",
        "source_guest_log_sha256": hashlib.sha256(raw).hexdigest(),
        "source_sf2_sha256": hashlib.sha256(source_sf2).hexdigest(),
        "source_sf2_original_guest_fnv1a32": f"0x{fnv1a32(source_sf2):08X}",
        "source_guest_fnv_not_cryptographic_provenance": True,
        "source_sf2_bytes": len(source_sf2),
        "creative_aweman32_module_path": dll_path,
        "original_driver_total_ram_words": seen["PRE_LOAD_TOTAL_16BIT_WORDS"],
        "original_driver_free_ram_before_words": before,
        "original_driver_free_ram_after_words": after,
        "original_driver_ram_allocated_words": delta,
        "original_guest_load_begin_ms": begin_ms,
        "original_guest_load_end_ms": end_ms,
        "original_MIDI_C4_vel100_91_93_zero_send_status": "WINMM_CALL_ACCEPTED",
        "original_Creative_AWE_SEND_MIDI_401_D4_vel100_zero_send_status":
            "ORIGINAL_AWEMAN32_API_ACCEPTED",
        "winmm_and_direct_bank_msb1_probe_order": "API_CALLS_CONFIRMED",
        "original_bank_msb1_selects_loaded_RAM_bank": "NOT_PROVEN",
        "independent_WinMM_C4_vs_Creative_API_D4_register_parity":
            "REQUIRES_SOURCE_GUEST_TRACE",
        "original_86box_emu8000_register_trace": "REQUIRES_INDEPENDENT_CAPTURE",
        "original_86box_guest_dry_pcm": "REQUIRES_INDEPENDENT_CAPTURE",
        "original_creative_dll_vxd_provenance": "REQUIRES_INDEPENDENT_VERIFICATION",
        "release_approved": False,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-guest-log", type=Path, required=True)
    parser.add_argument("--private-sf2", type=Path, required=True)
    parser.add_argument("--private-report", type=Path, required=True)
    args = parser.parse_args()
    path = args.private_report.resolve()
    repo = Path(__file__).resolve().parents[1]
    if path == repo or repo in path.parents or path.exists():
        parser.error("Private source report must be new and outside Git")
    log = args.private_guest_log.read_bytes()
    source = args.private_sf2.read_bytes()
    report = audit(log, source)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="ascii") as f:
        json.dump(report, f, indent=2)
        f.write("\n")
    print("P002_WIN95_ORIGINAL_CREATIVE_DRIVER_RAM_GUEST_API_LOG_PASS",
          "original_Win95_chip_trace=NOT_YET_PROVEN",
          "original_Win95_dry_PCM=NOT_YET_PROVEN",
          "release_approved=FALSE")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as exc:
        print("P002_WIN95_GUEST_RAM_LOG_NO_GO", exc, file=sys.stderr)
        sys.exit(1)
